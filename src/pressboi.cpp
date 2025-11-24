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
#include "error_log.h"
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
// Breadcrumb codes are now defined in config.h
#endif

//==================================================================================================
// --- External sendMessage function for events and telemetry ---
//==================================================================================================
extern Pressboi pressboi;

void sendMessage(const char* msg) {
    // Events and telemetry already have their prefixes, so send directly
    // Always queue messages - they will be sent to network (if GUI discovered) and USB
    // If GUI not discovered, use dummy IP - processTxQueue will still mirror to USB
    bool guiDiscovered = pressboi.m_comms.isGuiDiscovered();
    IpAddress targetIp = guiDiscovered ? pressboi.m_comms.getGuiIp() : IpAddress(0, 0, 0, 0);
    uint16_t targetPort = guiDiscovered ? pressboi.m_comms.getGuiPort() : 0;
    
    pressboi.m_comms.enqueueTx(msg, targetIp, targetPort);
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
    m_faultGracePeriodEnd = 0;
    
    // Initialize telemetry
    telemetry_init(&g_telemetry);
}

/**
 * @brief Initializes all hardware and sub-controllers for the entire system.
 */
void Pressboi::setup() {
    // Configure all motors for step and direction control mode.
    MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_STEP_AND_DIR);

    // Log firmware startup
    g_errorLog.log(LOG_INFO, "=== FIRMWARE STARTUP ===");
    g_errorLog.logf(LOG_INFO, "Firmware version: %s", FIRMWARE_VERSION);
    
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
        g_errorLog.log(LOG_INFO, "Setup complete - normal boot");
    } else {
        g_errorLog.log(LOG_ERROR, "Setup complete - RECOVERED from watchdog");
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
	
    // Always send telemetry (for both network and USB)
    if (now - m_lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
        // Skip telemetry if we're too close to discovery time (network may not be stable yet)
        static uint32_t discoveryTime = 0;
        static bool wasDiscovered = false;
        if (!wasDiscovered && m_comms.isGuiDiscovered()) {
            discoveryTime = now;
            wasDiscovered = true;
        }
        if (!m_comms.isGuiDiscovered()) {
            wasDiscovered = false;
        }
        
        // Wait at least 500ms after GUI discovery before sending (only affects network, USB always works)
        bool skipForNetworkStability = m_comms.isGuiDiscovered() && (now - discoveryTime < 500);
        
        if (!skipForNetworkStability) {
            #if WATCHDOG_ENABLED
            g_watchdogBreadcrumb = WD_BREADCRUMB_TELEMETRY;
            #endif
            m_lastTelemetryTime = now;
            publishTelemetry();
        } else {
            m_lastTelemetryTime = now; // Reset timer so we don't immediately spam after 500ms
        }
    }
    
    // If we're in RECOVERED state and GUI just connected, resend the recovery message with breadcrumb
    static bool recovery_msg_sent = false;
    if (m_mainState == STATE_RECOVERED && m_comms.isGuiDiscovered() && !recovery_msg_sent) {
        recovery_msg_sent = true;
        
        // Build recovery message with breadcrumb
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
            case WD_BREADCRUMB_UDP_PROCESS: breadcrumb_name = "UDP_PROCESS"; break;
            case WD_BREADCRUMB_USB_PROCESS: breadcrumb_name = "USB_PROCESS"; break;
            case WD_BREADCRUMB_TX_QUEUE: breadcrumb_name = "TX_QUEUE"; break;
            case WD_BREADCRUMB_UDP_SEND: breadcrumb_name = "UDP_SEND"; break;
            case WD_BREADCRUMB_NETWORK_REFRESH: breadcrumb_name = "NETWORK_REFRESH"; break;
            case WD_BREADCRUMB_USB_SEND: breadcrumb_name = "USB_SEND"; break;
            case WD_BREADCRUMB_USB_RECONNECT: breadcrumb_name = "USB_RECONNECT"; break;
            case WD_BREADCRUMB_USB_RECOVERY: breadcrumb_name = "USB_RECOVERY"; break;
            case WD_BREADCRUMB_REPORT_EVENT: breadcrumb_name = "REPORT_EVENT"; break;
            case WD_BREADCRUMB_ENQUEUE_TX: breadcrumb_name = "ENQUEUE_TX"; break;
            case WD_BREADCRUMB_MOTOR_IS_FAULT: breadcrumb_name = "MOTOR_IS_FAULT"; break;
            case WD_BREADCRUMB_MOTOR_STATE_SWITCH: breadcrumb_name = "MOTOR_STATE_SWITCH"; break;
            case WD_BREADCRUMB_PROCESS_TX_QUEUE: breadcrumb_name = "PROCESS_TX_QUEUE"; break;
            case WD_BREADCRUMB_TX_QUEUE_DEQUEUE: breadcrumb_name = "TX_QUEUE_DEQUEUE"; break;
            case WD_BREADCRUMB_TX_QUEUE_UDP: breadcrumb_name = "TX_QUEUE_UDP"; break;
            case WD_BREADCRUMB_TX_QUEUE_USB: breadcrumb_name = "TX_QUEUE_USB"; break;
            case WD_BREADCRUMB_DISPATCH_CMD: breadcrumb_name = "DISPATCH_CMD"; break;
            case WD_BREADCRUMB_PARSE_CMD: breadcrumb_name = "PARSE_CMD"; break;
            case WD_BREADCRUMB_MOTOR_FAULT_REPORT: breadcrumb_name = "MOTOR_FAULT_REPORT"; break;
            case WD_BREADCRUMB_STATE_BUSY_CHECK: breadcrumb_name = "STATE_BUSY_CHECK"; break;
            case WD_BREADCRUMB_UDP_PACKET_READ: breadcrumb_name = "UDP_PACKET_READ"; break;
            case WD_BREADCRUMB_RX_ENQUEUE: breadcrumb_name = "RX_ENQUEUE"; break;
            case WD_BREADCRUMB_USB_AVAILABLE: breadcrumb_name = "USB_AVAILABLE"; break;
            case WD_BREADCRUMB_USB_READ: breadcrumb_name = "USB_READ"; break;
            case WD_BREADCRUMB_NETWORK_INPUT: breadcrumb_name = "NETWORK_INPUT"; break;
            case WD_BREADCRUMB_LWIP_INPUT: breadcrumb_name = "LWIP_INPUT"; break;
            case WD_BREADCRUMB_LWIP_TIMEOUT: breadcrumb_name = "LWIP_TIMEOUT"; break;
        }
        snprintf(recoveryMsg, sizeof(recoveryMsg), "Watchdog timeout in %s - main loop blocked >128ms. Motors disabled. Send RESET to clear.", breadcrumb_name);
        reportEvent(STATUS_PREFIX_RECOVERY, recoveryMsg);
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
    #if WATCHDOG_ENABLED
    g_watchdogBreadcrumb = WD_BREADCRUMB_MOTOR_UPDATE;
    #endif
    m_motor.updateState();

    // Now, update the main Pressboi state based on the motor controller state.
    #if WATCHDOG_ENABLED
    g_watchdogBreadcrumb = WD_BREADCRUMB_UPDATE_STATE;
    #endif
    #if WATCHDOG_ENABLED
    g_watchdogBreadcrumb = WD_BREADCRUMB_MOTOR_STATE_SWITCH;
    #endif
    switch (m_mainState) {
        case STATE_STANDBY:
        case STATE_BUSY: {
            // In normal operation, constantly monitor for faults.
            // Skip fault detection during grace period (after clearing faults) to prevent false alarms
            uint32_t now = Milliseconds();
            bool inGracePeriod = (now < m_faultGracePeriodEnd);
            
            #if WATCHDOG_ENABLED
            g_watchdogBreadcrumb = WD_BREADCRUMB_MOTOR_IS_FAULT;
            #endif
            if (!inGracePeriod && m_motor.isInFault()) {
                #if WATCHDOG_ENABLED
                g_watchdogBreadcrumb = WD_BREADCRUMB_MOTOR_FAULT_REPORT;
                #endif
                m_mainState = STATE_ERROR;
                g_errorLog.log(LOG_ERROR, "Motor fault detected -> ERROR state");
                reportEvent(STATUS_PREFIX_ERROR, "Motor fault detected. System entering ERROR state. Use CLEAR_ERRORS to reset.");
                break;
            }

            // If no faults, update the state based on whether motor is busy.
            #if WATCHDOG_ENABLED
            g_watchdogBreadcrumb = WD_BREADCRUMB_STATE_BUSY_CHECK;
            #endif
            MainState newState = m_motor.isBusy() ? STATE_BUSY : STATE_STANDBY;
            if (newState != m_mainState) {
                g_errorLog.logf(LOG_DEBUG, "State: %s -> %s", 
                    (m_mainState == STATE_STANDBY) ? "STANDBY" : "BUSY",
                    (newState == STATE_STANDBY) ? "STANDBY" : "BUSY");
            }
            m_mainState = newState;
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
                
                // Set grace period to ignore faults for 500ms after clearing
                // This prevents false alarms as the motor driver status stabilizes
                m_faultGracePeriodEnd = now + 500;
                
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
    #if WATCHDOG_ENABLED
    g_watchdogBreadcrumb = WD_BREADCRUMB_PARSE_CMD;
    #endif
    Command command_enum = parseCommand(msg.buffer);
    
    #if WATCHDOG_ENABLED
    g_watchdogBreadcrumb = WD_BREADCRUMB_DISPATCH_CMD;
    #endif
    
    // Log incoming commands (except telemetry spam and discovery)
    if (command_enum != CMD_DISCOVER_DEVICE) {
        g_errorLog.logf(LOG_DEBUG, "Dispatch cmd: %s", msg.buffer);
    }
    
    // If the system is in RECOVERED state, block ALL commands except reset
    if (m_mainState == STATE_RECOVERED) {
        if (command_enum != CMD_DISCOVER_DEVICE && command_enum != CMD_RESET && command_enum != CMD_DUMP_ERROR_LOG) {
            m_comms.reportEvent(STATUS_PREFIX_ERROR, "Command ignored: System in RECOVERED state from watchdog timeout. Send RESET to clear.");
            g_errorLog.logf(LOG_WARNING, "Cmd blocked (RECOVERED): %s", msg.buffer);
            return;
        }
    }
    
    // If the system is in an error state, block most commands.
    if (m_mainState == STATE_ERROR) {
        if (command_enum != CMD_DISCOVER_DEVICE && command_enum != CMD_RESET && command_enum != CMD_DUMP_ERROR_LOG) {
            m_comms.reportEvent(STATUS_PREFIX_ERROR, "Command ignored: System is in ERROR state. Send reset to recover.");
            g_errorLog.logf(LOG_WARNING, "Cmd blocked (ERROR): %s", msg.buffer);
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
                uint16_t guiPort = atoi(portStr + 5);
                
                // Always respond to the IP that sent the discovery command
                // Don't update stored GUI IP if it came from localhost (USB)
                IpAddress localhost(127, 0, 0, 1);
                bool fromUsb = (msg.remoteIp == localhost);
                
                if (!fromUsb) {
                    // Network discovery - update stored GUI IP
                    m_comms.setGuiIp(msg.remoteIp);
                    m_comms.setGuiPort(guiPort);
                    m_comms.setGuiDiscovered(true);
                }
                // USB discovery - don't update, keep previous GUI IP
                
                // Report device ID, port, and firmware version
                char discoveryMsg[128];
                snprintf(discoveryMsg, sizeof(discoveryMsg), "%sDEVICE_ID=pressboi PORT=%d FW=%s", 
                        STATUS_PREFIX_DISCOVERY, LOCAL_PORT, FIRMWARE_VERSION);
                
                // Send response directly to the requester (not via reportEvent which uses stored GUI IP)
                m_comms.enqueueTx(discoveryMsg, msg.remoteIp, guiPort);
            }
            break;
        }
        case CMD_REBOOT_BOOTLOADER: {
            #if WATCHDOG_ENABLED
            // Disable watchdog FIRST - before any messages that could block
            WDT->CTRLA.reg = 0;
            while(WDT->SYNCBUSY.reg);
            #endif
            
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

        case CMD_SET_POLARITY: {
            char polarity[32] = "";
            if (sscanf(args, "%31s", polarity) == 1) {
                if (m_motor.setPolarity(polarity)) {
                    char msg_buf[128];
                    snprintf(msg_buf, sizeof(msg_buf), "Coordinate system polarity set to '%s' and saved to NVM", polarity);
                    reportEvent(STATUS_PREFIX_INFO, msg_buf);
                    reportEvent(STATUS_PREFIX_DONE, "set_polarity");
                } else {
                    reportEvent(STATUS_PREFIX_ERROR, "Invalid polarity. Use 'normal' or 'inverted'");
                }
            } else {
                reportEvent(STATUS_PREFIX_ERROR, "Invalid parameter for set_polarity");
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

        case CMD_DUMP_ERROR_LOG: {
            // Feed watchdog periodically during log dump (don't disable it - causes reboot!)
            // Log dumps can take >100ms for large logs, so we need to feed watchdog every few entries
            
            // Send error log entries over network/USB
            int entryCount = g_errorLog.getEntryCount();
            
            char msg[256];
            snprintf(msg, sizeof(msg), "=== ERROR LOG: %d entries ===", entryCount);
            reportEvent(STATUS_PREFIX_INFO, msg);
            
            // Send each entry (with small delay to prevent TX queue overflow)
            for (int i = 0; i < entryCount; i++) {
                LogEntry entry;
                if (g_errorLog.getEntry(i, &entry)) {
                    const char* levelStr;
                    switch (entry.level) {
                        case LOG_DEBUG:    levelStr = "DEBUG"; break;
                        case LOG_INFO:     levelStr = "INFO"; break;
                        case LOG_WARNING:  levelStr = "WARN"; break;
                        case LOG_ERROR:    levelStr = "ERROR"; break;
                        case LOG_CRITICAL: levelStr = "CRIT"; break;
                        default:           levelStr = "???"; break;
                    }
                    
                    // Format: [timestamp_ms] LEVEL: message
                    snprintf(msg, sizeof(msg), "[%lu] %s: %s", entry.timestamp, levelStr, entry.message);
                    reportEvent(STATUS_PREFIX_INFO, msg);
                    
                    // Small delay to allow TX queue to drain (prevent overflow)
                    Delay_ms(5);
                    
                    // Feed watchdog every 5 entries to prevent timeout
                    if ((i + 1) % 5 == 0) {
                        feedWatchdog();
                    }
                }
            }
            
            // Feed watchdog after error log
            feedWatchdog();
            
            snprintf(msg, sizeof(msg), "=== END ERROR LOG ===");
            reportEvent(STATUS_PREFIX_INFO, msg);
            
            // Also dump heartbeat log (24 hours of data!)
            int heartbeatCount = g_heartbeatLog.getEntryCount();
            
            // Calculate time span
            if (heartbeatCount > 0) {
                HeartbeatEntry firstEntry, lastEntry;
                g_heartbeatLog.getEntry(0, &firstEntry);
                g_heartbeatLog.getEntry(heartbeatCount - 1, &lastEntry);
                uint32_t spanMs = lastEntry.timestamp - firstEntry.timestamp;
                uint32_t spanHours = spanMs / 3600000;
                uint32_t spanMins = (spanMs % 3600000) / 60000;
                
                snprintf(msg, sizeof(msg), "=== HEARTBEAT LOG: %d entries (%luh%lum span) ===", 
                         heartbeatCount, (unsigned long)spanHours, (unsigned long)spanMins);
            } else {
                snprintf(msg, sizeof(msg), "=== HEARTBEAT LOG: 0 entries ===");
            }
            reportEvent(STATUS_PREFIX_INFO, msg);
            
            // Send entries in compact format: timestamp,U,N,A (USB,Network,Available)
            for (int i = 0; i < heartbeatCount; i++) {
                HeartbeatEntry entry;
                if (g_heartbeatLog.getEntry(i, &entry)) {
                    // Ultra-compact format: [timestamp] U:0/1 N:0/1 A:bytes
                    snprintf(msg, sizeof(msg), "[%lu] U:%d N:%d A:%d", 
                             entry.timestamp, entry.usbConnected, 
                             entry.networkActive, entry.usbAvailable);
                    reportEvent(STATUS_PREFIX_INFO, msg);
                    
                    // Every 10 entries, add delay to prevent TX queue overflow and feed watchdog
                    if ((i + 1) % 10 == 0) {
                        Delay_ms(50);
                        feedWatchdog();
                    }
                }
            }
            
            // Feed watchdog one final time
            feedWatchdog();
            
            snprintf(msg, sizeof(msg), "=== END HEARTBEAT LOG ===");
            reportEvent(STATUS_PREFIX_INFO, msg);
            
            reportEvent(STATUS_PREFIX_DONE, "dump_error_log");
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
    // Always publish telemetry - it will be sent to both network (if GUI discovered) and USB
    
    
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
    
    // Always queue telemetry - use dummy IP if GUI not discovered yet
    IpAddress targetIp = m_comms.isGuiDiscovered() ? m_comms.getGuiIp() : IpAddress(0, 0, 0, 0);
    uint16_t targetPort = m_comms.isGuiDiscovered() ? m_comms.getGuiPort() : 0;
    
    m_comms.enqueueTx(telemetryBuffer, targetIp, targetPort);
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
            case WD_BREADCRUMB_UDP_PROCESS: breadcrumb_name = "UDP_PROCESS"; break;
            case WD_BREADCRUMB_USB_PROCESS: breadcrumb_name = "USB_PROCESS"; break;
            case WD_BREADCRUMB_TX_QUEUE: breadcrumb_name = "TX_QUEUE"; break;
            case WD_BREADCRUMB_UDP_SEND: breadcrumb_name = "UDP_SEND"; break;
            case WD_BREADCRUMB_NETWORK_REFRESH: breadcrumb_name = "NETWORK_REFRESH"; break;
            case WD_BREADCRUMB_USB_SEND: breadcrumb_name = "USB_SEND"; break;
            case WD_BREADCRUMB_USB_RECONNECT: breadcrumb_name = "USB_RECONNECT"; break;
            case WD_BREADCRUMB_USB_RECOVERY: breadcrumb_name = "USB_RECOVERY"; break;
            case WD_BREADCRUMB_REPORT_EVENT: breadcrumb_name = "REPORT_EVENT"; break;
            case WD_BREADCRUMB_ENQUEUE_TX: breadcrumb_name = "ENQUEUE_TX"; break;
            case WD_BREADCRUMB_MOTOR_IS_FAULT: breadcrumb_name = "MOTOR_IS_FAULT"; break;
            case WD_BREADCRUMB_MOTOR_STATE_SWITCH: breadcrumb_name = "MOTOR_STATE_SWITCH"; break;
            case WD_BREADCRUMB_PROCESS_TX_QUEUE: breadcrumb_name = "PROCESS_TX_QUEUE"; break;
            case WD_BREADCRUMB_TX_QUEUE_DEQUEUE: breadcrumb_name = "TX_QUEUE_DEQUEUE"; break;
            case WD_BREADCRUMB_TX_QUEUE_UDP: breadcrumb_name = "TX_QUEUE_UDP"; break;
            case WD_BREADCRUMB_TX_QUEUE_USB: breadcrumb_name = "TX_QUEUE_USB"; break;
            case WD_BREADCRUMB_DISPATCH_CMD: breadcrumb_name = "DISPATCH_CMD"; break;
            case WD_BREADCRUMB_PARSE_CMD: breadcrumb_name = "PARSE_CMD"; break;
            case WD_BREADCRUMB_MOTOR_FAULT_REPORT: breadcrumb_name = "MOTOR_FAULT_REPORT"; break;
            case WD_BREADCRUMB_STATE_BUSY_CHECK: breadcrumb_name = "STATE_BUSY_CHECK"; break;
            case WD_BREADCRUMB_UDP_PACKET_READ: breadcrumb_name = "UDP_PACKET_READ"; break;
            case WD_BREADCRUMB_RX_ENQUEUE: breadcrumb_name = "RX_ENQUEUE"; break;
            case WD_BREADCRUMB_USB_AVAILABLE: breadcrumb_name = "USB_AVAILABLE"; break;
            case WD_BREADCRUMB_USB_READ: breadcrumb_name = "USB_READ"; break;
            case WD_BREADCRUMB_NETWORK_INPUT: breadcrumb_name = "NETWORK_INPUT"; break;
            case WD_BREADCRUMB_LWIP_INPUT: breadcrumb_name = "LWIP_INPUT"; break;
            case WD_BREADCRUMB_LWIP_TIMEOUT: breadcrumb_name = "LWIP_TIMEOUT"; break;
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
    // - Period values: 0x3 = 64 cycles (~64ms), 0x4 = 128 cycles (~128ms), 0x5 = 256 cycles (~256ms)
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
