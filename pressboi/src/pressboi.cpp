/**
 * @file pressboi.cpp
 * @author Eldin Miller-Stead
 * @date November 3, 2025
 * @brief Implements the master controller for the Pressboi press system.
 *
 * @details This file provides the concrete implementation for the `Pressboi` class
 * as defined in `pressboi.h`. It contains the logic for the main application loop,
 * command dispatch, state management, and the orchestration of the motor controller.
 * The program's entry point, `main()`, is also located here.
 */

//==================================================================================================
// --- Includes ---
//==================================================================================================
#include "pressboi.h"
#include "commands.h"
#include "variables.h"
#include "events.h"
#include "NvmManager.h"
#include <cstring>
#include <cstdlib>
#include <sam.h>  // For watchdog timer (WDT) register access

//==================================================================================================
// --- Global Telemetry Data ---
//==================================================================================================
TelemetryData g_telemetry;

//==================================================================================================
// --- Watchdog Recovery Flag and Breadcrumb (Survives Reset) ---
//==================================================================================================
#if WATCHDOG_ENABLED
// Place in .noinit section so it survives reset but is cleared on power-up
__attribute__((section(".noinit"))) volatile uint32_t g_watchdogRecoveryFlag;
__attribute__((section(".noinit"))) volatile uint32_t g_watchdogBreadcrumb;

// Breadcrumb codes to identify where the watchdog timeout occurred
#define WD_BREADCRUMB_SAFETY_CHECK      0x01
#define WD_BREADCRUMB_COMMS_UPDATE      0x02
#define WD_BREADCRUMB_RX_DEQUEUE        0x03
#define WD_BREADCRUMB_UPDATE_STATE      0x04
#define WD_BREADCRUMB_FORCE_UPDATE      0x05
#define WD_BREADCRUMB_MOTOR_UPDATE      0x06
#define WD_BREADCRUMB_TELEMETRY         0x07
#define WD_BREADCRUMB_UNKNOWN           0xFF
#endif

//==================================================================================================
// --- External sendMessage function for events and telemetry ---
//==================================================================================================
extern Pressboi pressboi;

void sendMessage(const char* msg) {
    // Events and telemetry already have their prefixes, so send directly
    pressboi.m_comms.enqueueTx(msg, pressboi.m_comms.getGuiIp(), pressboi.m_comms.getGuiPort());
}

//==================================================================================================
// --- Watchdog Early Warning Interrupt Handler ---
//==================================================================================================
#if WATCHDOG_ENABLED
/**
 * @brief Watchdog early warning interrupt handler.
 * @details This interrupt is triggered before the watchdog causes a system reset,
 * giving us a chance to safely disable motors and float all outputs. Sets a persistent
 * flag so the system knows it recovered from a watchdog reset on the next boot.
 */
extern "C" void WDT_Handler(void) {
    // Clear the early warning interrupt flag
    WDT->INTFLAG.reg = WDT_INTFLAG_EW;
    
    // Immediately disable all motors to prevent damage
    MOTOR_A.EnableRequest(false);
    MOTOR_B.EnableRequest(false);
    
    // Turn on the LED to indicate watchdog trigger (red/error state)
    ConnectorLed.Mode(Connector::OUTPUT_DIGITAL);
    ConnectorLed.State(true);
    
    // Set recovery flag in .noinit memory (survives reset)
    g_watchdogRecoveryFlag = WATCHDOG_RECOVERY_FLAG;
    
    // Blink the LED rapidly to make it obvious something went wrong
    for (int i = 0; i < 5; i++) {
        ConnectorLed.State(true);
        for (volatile int d = 0; d < 5000; d++);  // Short delay
        ConnectorLed.State(false);
        for (volatile int d = 0; d < 5000; d++);  // Short delay
    }
    
    // System will reset shortly after this ISR completes
    // On next boot, setup() will detect the recovery flag and enter STATE_RECOVERED
}
#endif

//==================================================================================================
// --- Pressboi Class Implementation ---
//==================================================================================================

/**
 * @brief Constructs the Pressboi master controller.
 */
Pressboi::Pressboi() :
    m_comms(),
    m_motor(&MOTOR_A, &MOTOR_B, this)
{
    m_mainState = STATE_STANDBY;
    m_lastTelemetryTime = 0;
    m_resetStartTime = 0;
    
    // Initialize telemetry
    telemetry_init(&g_telemetry);
}

/**
 * @brief Initializes all hardware and sub-controllers for the entire system.
 */
void Pressboi::setup() {
    // Configure all motors for step and direction control mode.
    MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_STEP_AND_DIR);

    // Initialize comms first (can take time for network setup)
    m_comms.setup();
    m_motor.setup();
    m_forceSensor.setup();
    
#if WATCHDOG_ENABLED
    // Initialize watchdog AFTER comms setup to avoid timeout during network initialization
    handleWatchdogRecovery();
    initializeWatchdog();
#endif
    
    // Only report normal startup if not in RECOVERED state
    if (m_mainState != STATE_RECOVERED) {
        m_comms.reportEvent(STATUS_PREFIX_INFO, "Pressboi system setup complete. All components initialized.");
    }
}

/**
 * @brief The main execution loop for the Pressboi system.
 */
void Pressboi::loop() {
    // 1. Perform safety checks and feed the watchdog timer.
    #if WATCHDOG_ENABLED
    g_watchdogBreadcrumb = WD_BREADCRUMB_SAFETY_CHECK;
    #endif
    performSafetyCheck();

    // 2. Process all incoming/outgoing communication queues.
    #if WATCHDOG_ENABLED
    g_watchdogBreadcrumb = WD_BREADCRUMB_COMMS_UPDATE;
    #endif
    m_comms.update();

    // 3. Check for and handle one new command from the receive queue.
    #if WATCHDOG_ENABLED
    g_watchdogBreadcrumb = WD_BREADCRUMB_RX_DEQUEUE;
    #endif
    Message msg;
    if (m_comms.dequeueRx(msg)) {
        dispatchCommand(msg);
    }

    // 4. Update force sensor readings.
    #if WATCHDOG_ENABLED
    g_watchdogBreadcrumb = WD_BREADCRUMB_FORCE_UPDATE;
    #endif
    m_forceSensor.update();

    // 5. Update the main state machine and all sub-controllers.
    #if WATCHDOG_ENABLED
    g_watchdogBreadcrumb = WD_BREADCRUMB_UPDATE_STATE;
    #endif
    updateState();

    // 6. Handle time-based periodic tasks.
    uint32_t now = Milliseconds();
	
    if (m_comms.isGuiDiscovered() && (now - m_lastTelemetryTime >= TELEMETRY_INTERVAL_MS)) {
        #if WATCHDOG_ENABLED
        g_watchdogBreadcrumb = WD_BREADCRUMB_TELEMETRY;
        #endif
        m_lastTelemetryTime = now;
        publishTelemetry();
    }
    
    // If we're in RECOVERED state and GUI just connected, resend the recovery message
    static bool recovery_msg_sent = false;
    if (m_mainState == STATE_RECOVERED && m_comms.isGuiDiscovered() && !recovery_msg_sent) {
        recovery_msg_sent = true;
        reportEvent(STATUS_PREFIX_RECOVERY, "Watchdog timeout - main loop blocked >128ms. Motors disabled. Send RESET to clear.");
    }
    
    // Reset the flag when leaving RECOVERED state
    if (m_mainState != STATE_RECOVERED && recovery_msg_sent) {
        recovery_msg_sent = false;
    }
}

//==================================================================================================
// --- Private Methods: State, Command, and Telemetry Handling ---
//==================================================================================================

/**
 * @brief Performs safety checks and feeds the watchdog timer.
 */
void Pressboi::performSafetyCheck() {
#if WATCHDOG_ENABLED
    feedWatchdog();
    
    // Note: Motor fault detection is handled in updateState() as part of the normal state machine
    // This function focuses on feeding the watchdog and can be extended with additional
    // hardware-level safety checks if needed (e.g., force sensor limits, temperature monitoring)
#endif
}

/**
 * @brief Updates the main system state and the state machines of all sub-controllers.
 */
void Pressboi::updateState() {
    // First, update the state of the motor controller
    m_motor.updateState();

    // Now, update the main Pressboi state based on the motor controller state.
    switch (m_mainState) {
        case STATE_STANDBY:
        case STATE_BUSY: {
            // In normal operation, constantly monitor for faults.
            if (m_motor.isInFault()) {
                m_mainState = STATE_ERROR;
                reportEvent(STATUS_PREFIX_ERROR, "Motor fault detected. System entering ERROR state. Use CLEAR_ERRORS to reset.");
                break;
            }

            // If no faults, update the state based on whether motor is busy.
            if (m_motor.isBusy()) {
                m_mainState = STATE_BUSY;
            } else {
                m_mainState = STATE_STANDBY;
            }
            break;
        }

        case STATE_CLEARING_ERRORS: {
            // Wait for motor controller to finish its reset sequence.
            if (!m_motor.isBusy()) {
                // Now it's safe to cycle motor power.
                m_motor.disable();
                Delay_ms(10);
                m_motor.enable();

                // Recovery is complete.
                m_mainState = STATE_STANDBY;
                reportEvent(STATUS_PREFIX_DONE, "CLEAR_ERRORS complete. System is in STANDBY state.");
            }
            break;
        }

        case STATE_RESETTING: {
            // Non-blocking reset: wait for 100ms delay before re-enabling motors
            uint32_t now = Milliseconds();
            if (now - m_resetStartTime >= 100) {  // 100ms delay
                // Clear any motor faults before re-enabling
                MOTOR_A.ClearAlerts();
                MOTOR_B.ClearAlerts();
                
                // Re-enable motors
                m_motor.enable();
                
                // Reset complete, transition to standby
                standby();
                reportEvent(STATUS_PREFIX_DONE, "reset");
                m_mainState = STATE_STANDBY;
            }
            break;
        }

        case STATE_ERROR:
        case STATE_DISABLED:
        case STATE_RECOVERED:
            // These are terminal states. No logic is performed here.
            // They are only exited by explicit commands (CLEAR_ERRORS, ENABLE, or RESET).
            break;
    }
}

/**
 * @brief Master command handler; acts as a switchboard to delegate tasks.
 * @param msg The message object containing the command string and remote IP details.
 */
void Pressboi::dispatchCommand(const Message& msg) {
    Command command_enum = parseCommand(msg.buffer);
    
    // If the system is in RECOVERED state, block ALL commands except reset
    if (m_mainState == STATE_RECOVERED) {
        if (command_enum != CMD_DISCOVER_DEVICE && command_enum != CMD_RESET) {
            m_comms.reportEvent(STATUS_PREFIX_ERROR, "Command ignored: System in RECOVERED state from watchdog timeout. Send RESET to clear.");
            return;
        }
    }
    
    // If the system is in an error state, block most commands.
    if (m_mainState == STATE_ERROR) {
        if (command_enum != CMD_DISCOVER_DEVICE && command_enum != CMD_RESET) {
            m_comms.reportEvent(STATUS_PREFIX_ERROR, "Command ignored: System is in ERROR state. Send reset to recover.");
            return;
        }
    }

    // Isolate arguments by finding the first space in the command string.
    const char* args = strchr(msg.buffer, ' ');
    if (args) {
        args++; // Move pointer past the space to the start of the arguments.
    }

    // --- Master Command Delegation Switchboard ---
    switch (command_enum) {
        // --- System-Level Commands (Handled by Pressboi) ---
        case CMD_DISCOVER_DEVICE: {
            char* portStr = strstr(msg.buffer, "PORT=");
            if (portStr) {
                m_comms.setGuiIp(msg.remoteIp);
                m_comms.setGuiPort(atoi(portStr + 5));
                m_comms.setGuiDiscovered(true);
                // Report device ID, port, and firmware version
                char discoveryMsg[96];
                snprintf(discoveryMsg, sizeof(discoveryMsg), "DEVICE_ID=pressboi PORT=%d FW=%s", LOCAL_PORT, FIRMWARE_VERSION);
                m_comms.reportEvent(STATUS_PREFIX_DISCOVERY, discoveryMsg);
            }
            break;
        }
        case CMD_REBOOT_BOOTLOADER: {
            reportEvent(STATUS_PREFIX_INFO, "Rebooting to bootloader...");
            SysMgr.ResetBoard(SysManager::RESET_TO_BOOTLOADER);
            break; // The system will reset before reaching here
        }
        case CMD_RESET:
            clearErrors();
            break;
        case CMD_ENABLE:
            enable();
            m_comms.reportEvent(STATUS_PREFIX_DONE, "enable");
            break;
        case CMD_DISABLE:
            disable();
            m_comms.reportEvent(STATUS_PREFIX_DONE, "disable");
            break;
        case CMD_TEST_WATCHDOG:
            // Test command: block for 1 second to trigger watchdog
            reportEvent(STATUS_PREFIX_INFO, "TEST_WATCHDOG: Blocking for 1 second...");
            Delay_ms(1000);
            // This line should never execute if watchdog works
            reportEvent(STATUS_PREFIX_ERROR, "TEST_WATCHDOG: Watchdog did not trigger!");
            break;
        
        case CMD_SET_FORCE_OFFSET: {
            float offset = 0.0f;
            if (sscanf(args, "%f", &offset) == 1) {
                const char* mode = m_motor.getForceMode();
                if (strcmp(mode, "load_cell") == 0) {
                    // Load cell mode: set load cell offset
                    m_forceSensor.setOffset(offset);
                    char msg_buf[128];
                    snprintf(msg_buf, sizeof(msg_buf), "Load cell offset set to %.2f kg and saved to NVM", offset);
                    reportEvent(STATUS_PREFIX_INFO, msg_buf);
                } else {
                    // Motor torque mode: set motor torque offset
                    m_motor.setForceCalibrationOffset(offset);
                    char msg_buf[128];
                    snprintf(msg_buf, sizeof(msg_buf), "Motor torque offset set to %.4f and saved to NVM", offset);
                    reportEvent(STATUS_PREFIX_INFO, msg_buf);
                }
                reportEvent(STATUS_PREFIX_DONE, "set_force_offset");
            } else {
                reportEvent(STATUS_PREFIX_ERROR, "Invalid parameter for set_force_offset");
            }
            break;
        }
        
        case CMD_SET_FORCE_SCALE: {
            float scale = 1.0f;
            if (sscanf(args, "%f", &scale) == 1) {
                const char* mode = m_motor.getForceMode();
                if (strcmp(mode, "load_cell") == 0) {
                    // Load cell mode: set load cell scale
                    m_forceSensor.setScale(scale);
                    char msg_buf[128];
                    snprintf(msg_buf, sizeof(msg_buf), "Load cell scale set to %.6f and saved to NVM", scale);
                    reportEvent(STATUS_PREFIX_INFO, msg_buf);
                } else {
                    // Motor torque mode: set motor torque scale
                    m_motor.setForceCalibrationScale(scale);
                    char msg_buf[128];
                    snprintf(msg_buf, sizeof(msg_buf), "Motor torque scale set to %.6f and saved to NVM", scale);
                    reportEvent(STATUS_PREFIX_INFO, msg_buf);
                }
                reportEvent(STATUS_PREFIX_DONE, "set_force_scale");
            } else {
                reportEvent(STATUS_PREFIX_ERROR, "Invalid parameter for set_force_scale");
            }
            break;
        }
        
        case CMD_SET_STRAIN_CAL: {
            float coeff_x4 = MACHINE_STRAIN_COEFF_X4;
            float coeff_x3 = MACHINE_STRAIN_COEFF_X3;
            float coeff_x2 = MACHINE_STRAIN_COEFF_X2;
            float coeff_x1 = MACHINE_STRAIN_COEFF_X1;
            float coeff_c  = MACHINE_STRAIN_COEFF_C;
            if (sscanf(args, "%f %f %f %f %f", &coeff_x4, &coeff_x3, &coeff_x2, &coeff_x1, &coeff_c) == 5) {
                m_motor.setMachineStrainCoeffs(coeff_x4, coeff_x3, coeff_x2, coeff_x1, coeff_c);
                char msg_buf[160];
                snprintf(msg_buf, sizeof(msg_buf),
                         "Machine strain polynomial updated: f(x) = %.3f x^4 %+.3f x^3 %+.3f x^2 %+.3f x %+.3f",
                         coeff_x4, coeff_x3, coeff_x2, coeff_x1, coeff_c);
                reportEvent(STATUS_PREFIX_INFO, msg_buf);
                reportEvent(STATUS_PREFIX_DONE, "set_strain_cal");
            } else {
                reportEvent(STATUS_PREFIX_ERROR, "Invalid parameters for set_strain_cal");
            }
            break;
        }
        
        case CMD_SET_FORCE_MODE: {
            char mode[32] = "";
            if (sscanf(args, "%31s", mode) == 1) {
                if (m_motor.setForceMode(mode)) {
                    char msg_buf[128];
                    snprintf(msg_buf, sizeof(msg_buf), "Force mode set to '%s' and saved to NVM", mode);
                    reportEvent(STATUS_PREFIX_INFO, msg_buf);
                    reportEvent(STATUS_PREFIX_DONE, "set_force_mode");
                } else {
                    reportEvent(STATUS_PREFIX_ERROR, "Invalid mode. Use 'motor_torque' or 'load_cell'");
                }
            } else {
                reportEvent(STATUS_PREFIX_ERROR, "Invalid parameter for set_force_mode");
            }
            break;
        }

        case CMD_SET_FORCE_ZERO: {
            const char* mode = m_motor.getForceMode();
            if (strcmp(mode, "load_cell") == 0) {
                // Load cell mode: capture current force reading and set as new offset
                float old_offset = m_forceSensor.getOffset();
                float current_force = m_forceSensor.getForce();
                float new_offset = old_offset - current_force;
                
                m_forceSensor.setOffset(new_offset);
                
                char msg_buf[128];
                snprintf(msg_buf, sizeof(msg_buf), "Load cell offset: %.2f kg -> %.2f kg", 
                         old_offset, new_offset);
                reportEvent(STATUS_PREFIX_INFO, msg_buf);
            } else {
                // Motor torque mode: set offset to current torque reading
                float old_offset = m_motor.getForceCalibrationOffset();
                float current_torque = (MOTOR_A.HlfbPercent() + MOTOR_B.HlfbPercent()) / 2.0f;
                float new_offset = -current_torque;
                
                m_motor.setForceCalibrationOffset(new_offset);
                
                char msg_buf[128];
                snprintf(msg_buf, sizeof(msg_buf), "Motor torque offset: %.4f%% -> %.4f%%", 
                         old_offset, new_offset);
                reportEvent(STATUS_PREFIX_INFO, msg_buf);
            }
            reportEvent(STATUS_PREFIX_DONE, "set_force_zero");
            break;
        }

        case CMD_DUMP_NVM: {
            ClearCore::NvmManager &nvmMgr = ClearCore::NvmManager::Instance();
            char msg_buf[256];

            // Read all 16 locations first to avoid hanging on NVM access in loop
            int32_t nvm_values[16];
            for (int i = 0; i < 16; ++i) {
                int byte_offset = i * 4;
                nvm_values[i] = nvmMgr.Int32(static_cast<ClearCore::NvmManager::NvmLocations>(byte_offset));
            }

            // Now format and send all messages
            for (int i = 0; i < 16; ++i) {
                int byte_offset = i * 4;
                int32_t value = nvm_values[i];
                unsigned char* bytes = (unsigned char*)&value;
                
                // Format hex bytes
                char hex_str[50];
                char ascii_str[10];
                snprintf(hex_str, sizeof(hex_str), "%02X %02X %02X %02X", 
                         bytes[0], bytes[1], bytes[2], bytes[3]);
                
                // ASCII representation (printable chars only)
                for (int j = 0; j < 4; j++) {
                    ascii_str[j] = (bytes[j] >= 32 && bytes[j] <= 126) ? bytes[j] : '.';
                }
                ascii_str[4] = '\0';
                
                // Send with NVMDUMP prefix for GUI routing
                snprintf(msg_buf, sizeof(msg_buf), "NVMDUMP:pressboi:%04X:%s:%s", byte_offset, hex_str, ascii_str);
                m_comms.enqueueTx(msg_buf, m_comms.getGuiIp(), m_comms.getGuiPort());
            }

            // Summary with interpreted calibration values (use cached values)
            int32_t magic = nvm_values[7];        // Location 7 (byte offset 28)
            int32_t force_mode = nvm_values[4];   // Location 4 (byte offset 16)
            
            const char* magic_status = (magic == 0x50425231) ? "OK" : "INVALID";
            const char* mode_str = (force_mode == 0) ? "motor_torque" : "load_cell";
            
            // Show magic and mode
            snprintf(msg_buf, sizeof(msg_buf), "NVMDUMP:pressboi:SUMMARY: Magic=0x%08X(%s) CurrentMode=%s", 
                     (unsigned int)magic, magic_status, mode_str);
            m_comms.enqueueTx(msg_buf, m_comms.getGuiIp(), m_comms.getGuiPort());
            
            // Load cell calibration (locations 0 & 1 - IEEE float as bits)
            int32_t lc_offset_bits = nvm_values[0];   // Location 0
            int32_t lc_scale_bits = nvm_values[1];    // Location 1
            float lc_offset, lc_scale;
            memcpy(&lc_offset, &lc_offset_bits, sizeof(float));
            memcpy(&lc_scale, &lc_scale_bits, sizeof(float));
            
            snprintf(msg_buf, sizeof(msg_buf), "NVMDUMP:pressboi:SUMMARY: LoadCell: Scale=%.6f Offset=%.4f kg", 
                     lc_scale, lc_offset);
            m_comms.enqueueTx(msg_buf, m_comms.getGuiIp(), m_comms.getGuiPort());
            
            // Motor torque calibration (locations 5 & 6 - fixed-point)
            int32_t mt_scale_raw = nvm_values[5];    // Location 5
            int32_t mt_offset_raw = nvm_values[6];   // Location 6
            float mt_scale = (float)mt_scale_raw / 100000.0f;
            float mt_offset = (float)mt_offset_raw / 10000.0f;
            
            snprintf(msg_buf, sizeof(msg_buf), "NVMDUMP:pressboi:SUMMARY: MotorTorque: Scale=%.6f Offset=%.4f %%", 
                     mt_scale, mt_offset);
            m_comms.enqueueTx(msg_buf, m_comms.getGuiIp(), m_comms.getGuiPort());

            reportEvent(STATUS_PREFIX_DONE, "dump_nvm");
            break;
        }

        case CMD_RESET_NVM: {
            ClearCore::NvmManager &nvmMgr = ClearCore::NvmManager::Instance();

            // Reset all NVM locations to 0xFFFFFFFF (erased flash state)
            // Each location is 4 bytes, so use proper byte offsets
            for (int i = 0; i < 16; ++i) {
                nvmMgr.Int32(static_cast<ClearCore::NvmManager::NvmLocations>(i * 4), -1);
            }

            reportEvent(STATUS_PREFIX_INFO, "All NVM locations reset to erased state. Reboot required for changes to take effect.");
            reportEvent(STATUS_PREFIX_DONE, "reset_nvm");
            break;
        }

        // --- Motor Commands (Delegated to MotorController) ---
        case CMD_HOME:
        case CMD_MOVE_ABS:
        case CMD_MOVE_INC:
        case CMD_SET_RETRACT:
        case CMD_RETRACT:
            m_motor.handleCommand(command_enum, args);
            break;

        // --- Motion Control Commands ---
        case CMD_PAUSE:
            m_motor.pauseOperation();
            break;
        case CMD_RESUME:
            m_motor.resumeOperation();
            break;
        case CMD_CANCEL:
            m_motor.cancelOperation();
            // Cancel also acts as abort - stops motion and resets to standby
            abort();
            break;

        // --- Default/Unknown ---
        case CMD_UNKNOWN:
        default:
            m_comms.reportEvent(STATUS_PREFIX_ERROR, "Unknown command sent to Pressboi.");
            break;
    }
}

/**
 * @brief Aggregates telemetry data from all sub-controllers and sends it as a single UDP packet.
 */
void Pressboi::publishTelemetry() {
    if (!m_comms.isGuiDiscovered()) return;

    // Update telemetry data from motor controller (includes force calculation)
    m_motor.updateTelemetry(&g_telemetry, &m_forceSensor);
    
    // Set main state
    switch(m_mainState) {
        case STATE_STANDBY:        g_telemetry.MAIN_STATE = "STANDBY"; break;
        case STATE_BUSY:           g_telemetry.MAIN_STATE = "BUSY"; break;
        case STATE_ERROR:          g_telemetry.MAIN_STATE = "ERROR"; break;
        case STATE_DISABLED:       g_telemetry.MAIN_STATE = "DISABLED"; break;
        case STATE_CLEARING_ERRORS:g_telemetry.MAIN_STATE = "CLEARING_ERRORS"; break;
        case STATE_RESETTING:      g_telemetry.MAIN_STATE = "RESETTING"; break;
        case STATE_RECOVERED:      g_telemetry.MAIN_STATE = "RECOVERED"; break;
        default:                   g_telemetry.MAIN_STATE = "UNKNOWN"; break;
    }

    // Build and send telemetry message
    char telemetryBuffer[1024];
    telemetry_build_message(&g_telemetry, telemetryBuffer, sizeof(telemetryBuffer));
    m_comms.enqueueTx(telemetryBuffer, m_comms.getGuiIp(), m_comms.getGuiPort());
}

/**
 * @brief Enables all motors and places the system in a ready state.
 */
void Pressboi::enable() {
    if (m_mainState == STATE_DISABLED) {
        m_mainState = STATE_STANDBY;
        m_motor.enable();
        // DONE message sent by dispatchCommand
    } else {
        m_comms.reportEvent(STATUS_PREFIX_INFO, "System already enabled.");
    }
}

/**
 * @brief Disables all motors and stops all operations.
 */
void Pressboi::disable() {
    abort(); // Safest to abort any motion first.
    m_mainState = STATE_DISABLED;
    m_motor.disable();
    // DONE message sent by dispatchCommand
}

/**
 * @brief Halts all motion and resets the system state to standby.
 */
void Pressboi::abort() {
    reportEvent(STATUS_PREFIX_INFO, "Stopping all motion.");
    m_motor.abortMove();
    standby();
}

/**
 * @brief Resets any error states, clears motor faults, and returns the system to standby.
 */
void Pressboi::clearErrors() {
    reportEvent(STATUS_PREFIX_INFO, "Reset received. Clearing errors and resetting system...");

#if WATCHDOG_ENABLED
    clearWatchdogRecovery();
#endif

    // Abort any active motion first to ensure a clean state.
    m_motor.abortMove();

    // Disable motors and start non-blocking reset timer
    m_motor.disable();
    m_resetStartTime = Milliseconds();
    m_mainState = STATE_RESETTING;
    
    // The actual motor re-enable and completion happens in updateState()
    // This makes the reset operation non-blocking and watchdog-safe
}

void Pressboi::standby() {
    // Reset motor controller to its idle state.
    m_motor.reset();

    // Set the main state and report.
    m_mainState = STATE_STANDBY;
    reportEvent(STATUS_PREFIX_INFO, "System is in STANDBY state.");
}

/**
 * @brief Public interface to send a status message.
 */
void Pressboi::reportEvent(const char* statusType, const char* message) {
    m_comms.reportEvent(statusType, message);
}

//==================================================================================================
// --- Watchdog Functions ---
//==================================================================================================

#if WATCHDOG_ENABLED
/**
 * @brief Checks for watchdog recovery after motor setup.
 */
void Pressboi::handleWatchdogRecovery() {
    // Check reset cause to detect watchdog reset
    uint8_t reset_cause = RSTC->RCAUSE.reg;
    bool is_watchdog_reset = (reset_cause & RSTC_RCAUSE_WDT) != 0;
    
    // If watchdog reset detected, immediately disable motors and enter RECOVERED state
    if (is_watchdog_reset) {
        m_motor.disable();
        m_mainState = STATE_RECOVERED;
        
        // Report reset cause for debugging
        char debug_msg[120];
        snprintf(debug_msg, sizeof(debug_msg), 
                 "Reset cause: 0x%02X (POR=%d BODCORE=%d BODVDD=%d EXT=%d WDT=%d SYST=%d)", 
                 reset_cause,
                 (reset_cause & RSTC_RCAUSE_POR) ? 1 : 0,
                 (reset_cause & RSTC_RCAUSE_BODCORE) ? 1 : 0,
                 (reset_cause & RSTC_RCAUSE_BODVDD) ? 1 : 0,
                 (reset_cause & RSTC_RCAUSE_EXT) ? 1 : 0,
                 (reset_cause & RSTC_RCAUSE_WDT) ? 1 : 0,
                 (reset_cause & RSTC_RCAUSE_SYST) ? 1 : 0);
        m_comms.reportEvent(STATUS_PREFIX_INFO, debug_msg);
        
        // Clear any motor alerts
        MOTOR_A.ClearAlerts();
        MOTOR_B.ClearAlerts();
        
        // Send recovery message with breadcrumb
        char recoveryMsg[128];
        const char* breadcrumb_name = "UNKNOWN";
        switch (g_watchdogBreadcrumb) {
            case WD_BREADCRUMB_SAFETY_CHECK: breadcrumb_name = "SAFETY_CHECK"; break;
            case WD_BREADCRUMB_COMMS_UPDATE: breadcrumb_name = "COMMS_UPDATE"; break;
            case WD_BREADCRUMB_RX_DEQUEUE: breadcrumb_name = "RX_DEQUEUE"; break;
            case WD_BREADCRUMB_UPDATE_STATE: breadcrumb_name = "UPDATE_STATE"; break;
            case WD_BREADCRUMB_FORCE_UPDATE: breadcrumb_name = "FORCE_UPDATE"; break;
            case WD_BREADCRUMB_MOTOR_UPDATE: breadcrumb_name = "MOTOR_UPDATE"; break;
            case WD_BREADCRUMB_TELEMETRY: breadcrumb_name = "TELEMETRY"; break;
        }
        snprintf(recoveryMsg, sizeof(recoveryMsg), "Watchdog timeout in %s - main loop blocked >128ms. Motors disabled. Send RESET to clear.", breadcrumb_name);
        m_comms.reportEvent(STATUS_PREFIX_RECOVERY, recoveryMsg);
        
        // Keep LED on solid to indicate recovered state
        ConnectorLed.Mode(Connector::OUTPUT_DIGITAL);
        ConnectorLed.State(true);
    } else {
        // Normal boot - report reset cause for debugging
        char debug_msg[120];
        snprintf(debug_msg, sizeof(debug_msg), 
                 "Reset cause: 0x%02X (POR=%d BODCORE=%d BODVDD=%d EXT=%d WDT=%d SYST=%d)", 
                 reset_cause,
                 (reset_cause & RSTC_RCAUSE_POR) ? 1 : 0,
                 (reset_cause & RSTC_RCAUSE_BODCORE) ? 1 : 0,
                 (reset_cause & RSTC_RCAUSE_BODVDD) ? 1 : 0,
                 (reset_cause & RSTC_RCAUSE_EXT) ? 1 : 0,
                 (reset_cause & RSTC_RCAUSE_WDT) ? 1 : 0,
                 (reset_cause & RSTC_RCAUSE_SYST) ? 1 : 0);
        m_comms.reportEvent(STATUS_PREFIX_INFO, debug_msg);
    }
}

/**
 * @brief Initializes the watchdog timer with early warning interrupt.
 */
void Pressboi::initializeWatchdog() {
    // Disable watchdog during configuration
    WDT->CTRLA.reg = 0;
    while(WDT->SYNCBUSY.reg);
    
    // Configure watchdog:
    // - For timeout at 1kHz WDT clock
    // - Period values: 0x3 = 64 cycles (~64ms), 0x4 = 128 cycles (~128ms)
    // - Use 0x4 for ~128ms - aggressive timeout to catch real hangs quickly
    uint8_t per_value = 0x4;  // 128 cycles ≈ 128ms at 1kHz
    
    // Configure watchdog with early warning interrupt
    WDT->CONFIG.reg = WDT_CONFIG_PER(per_value);
    while(WDT->SYNCBUSY.reg);
    
    // Enable early warning interrupt
    WDT->INTENSET.reg = WDT_INTENSET_EW;
    
    // Enable WDT interrupt in NVIC (Nested Vectored Interrupt Controller)
    NVIC_EnableIRQ(WDT_IRQn);
    NVIC_SetPriority(WDT_IRQn, 0);  // Highest priority
    
    // Enable watchdog
    WDT->CTRLA.reg = WDT_CTRLA_ENABLE;
    while(WDT->SYNCBUSY.reg);
    
    m_comms.reportEvent(STATUS_PREFIX_INFO, "Watchdog timer initialized with early warning interrupt.");
}

/**
 * @brief Feeds (clears/resets) the watchdog timer.
 */
void Pressboi::feedWatchdog() {
    // ALWAYS feed the watchdog - this is the ONLY place it should be fed
    // Feed it in ALL states to keep the system responsive
    WDT->CLEAR.reg = WDT_CLEAR_CLEAR_KEY;
    while(WDT->SYNCBUSY.reg);
}

/**
 * @brief Cleans up watchdog recovery state when clearing errors.
 */
void Pressboi::clearWatchdogRecovery() {
    // Check if we're in RECOVERED state
    if (m_mainState == STATE_RECOVERED) {
        reportEvent(STATUS_PREFIX_INFO, "Clearing watchdog recovery state...");
        
        // Turn off the LED
        ConnectorLed.State(false);
        
        reportEvent(STATUS_PREFIX_INFO, "Watchdog recovery cleared. System will now initialize normally.");
    }
}
#endif

//==================================================================================================
// --- Program Entry Point ---
//==================================================================================================

/**
 * @brief Global instance of the entire Pressboi system.
 */
Pressboi pressboi;

/**
 * @brief The main function and entry point of the application.
 */
int main(void) {
    // Initialize all hardware and controllers.
    pressboi.setup();

    // Enter the main application loop, which runs forever.
    while (true) {
        pressboi.loop();
    }
}
