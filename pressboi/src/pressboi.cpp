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
#include <cstring>
#include <cstdlib>

//==================================================================================================
// --- Global Telemetry Data ---
//==================================================================================================
TelemetryData g_telemetry;

//==================================================================================================
// --- External sendMessage function for events and telemetry ---
//==================================================================================================
extern Pressboi pressboi;

void sendMessage(const char* msg) {
    // Events and telemetry already have their prefixes, so send directly
    pressboi.m_comms.enqueueTx(msg, pressboi.m_comms.getGuiIp(), pressboi.m_comms.getGuiPort());
}

//==================================================================================================
// --- Pressboi Class Implementation ---
//==================================================================================================

/**
 * @brief Constructs the Pressboi master controller.
 */
Pressboi::Pressboi() :
    m_comms(),
    m_motor(&MOTOR_INJECTOR_A, &MOTOR_INJECTOR_B, this)
{
    m_mainState = STATE_STANDBY;
    m_lastTelemetryTime = 0;
    
    // Initialize telemetry
    telemetry_init(&g_telemetry);
}

/**
 * @brief Initializes all hardware and sub-controllers for the entire system.
 */
void Pressboi::setup() {
    // Configure all motors for step and direction control mode.
    MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_STEP_AND_DIR);

    m_comms.setup();
    m_motor.setup();
    
    m_comms.reportEvent(STATUS_PREFIX_INFO, "Pressboi system setup complete. All components initialized.");
}

/**
 * @brief The main execution loop for the Pressboi system.
 */
void Pressboi::loop() {
    // 1. Process all incoming/outgoing communication queues.
    m_comms.update();

    // 2. Check for and handle one new command from the receive queue.
    Message msg;
    if (m_comms.dequeueRx(msg)) {
        dispatchCommand(msg);
    }

    // 3. Update the main state machine and all sub-controllers.
    updateState();

    // 4. Handle time-based periodic tasks.
    uint32_t now = Milliseconds();
	
    if (m_comms.isGuiDiscovered() && (now - m_lastTelemetryTime >= TELEMETRY_INTERVAL_MS)) {
        m_lastTelemetryTime = now;
        publishTelemetry();
    }
}

//==================================================================================================
// --- Private Methods: State, Command, and Telemetry Handling ---
//==================================================================================================

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

        case STATE_ERROR:
        case STATE_DISABLED:
            // These are terminal states. No logic is performed here.
            // They are only exited by explicit commands (CLEAR_ERRORS, ENABLE).
            break;
    }
}

/**
 * @brief Master command handler; acts as a switchboard to delegate tasks.
 * @param msg The message object containing the command string and remote IP details.
 */
void Pressboi::dispatchCommand(const Message& msg) {
    Command command_enum = parseCommand(msg.buffer);
    
    // If the system is in an error state, block most commands.
    if (m_mainState == STATE_ERROR) {
        if (command_enum != CMD_DISCOVER_DEVICE && command_enum != CMD_CLEAR_ERRORS) {
            m_comms.reportEvent(STATUS_PREFIX_ERROR, "Command ignored: System is in ERROR state. Send clear_errors to reset.");
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
                // Report device ID and the port we're listening on
                char discoveryMsg[64];
                snprintf(discoveryMsg, sizeof(discoveryMsg), "DEVICE_ID=pressboi PORT=%d", LOCAL_PORT);
                m_comms.reportEvent(STATUS_PREFIX_DISCOVERY, discoveryMsg);
            }
            break;
        }
        case CMD_ABORT:
            abort();
            break;
        case CMD_CLEAR_ERRORS:
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

        // --- Motor Commands (Delegated to MotorController) ---
        case CMD_HOME:
        case CMD_MOVE_ABS:
        case CMD_MOVE_INC:
        case CMD_SET_START_POS:
        case CMD_MOVE_TO_START:
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

    // Update telemetry data from motor controller
    m_motor.updateTelemetry(&g_telemetry);
    
    // Set main state
    switch(m_mainState) {
        case STATE_STANDBY:  g_telemetry.MAIN_STATE = "STANDBY"; break;
        case STATE_BUSY:     g_telemetry.MAIN_STATE = "BUSY"; break;
        case STATE_ERROR:    g_telemetry.MAIN_STATE = "ERROR"; break;
        case STATE_DISABLED: g_telemetry.MAIN_STATE = "DISABLED"; break;
        default:             g_telemetry.MAIN_STATE = "UNKNOWN"; break;
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
    reportEvent(STATUS_PREFIX_INFO, "ABORT received. Stopping all motion.");
    m_motor.abortMove();
    standby();
    reportEvent(STATUS_PREFIX_DONE, "abort");
}

/**
 * @brief Resets any error states, clears motor faults, and returns the system to standby.
 */
void Pressboi::clearErrors() {
    reportEvent(STATUS_PREFIX_INFO, "CLEAR_ERRORS received. Resetting all sub-systems...");

    // Abort any active motion first to ensure a clean state.
    m_motor.abortMove();

    // Power cycle the motors to clear hardware faults.
    m_motor.disable();
    Delay_ms(100);
    m_motor.enable();
    
    // The system is now fully reset and ready.
    standby();
    reportEvent(STATUS_PREFIX_DONE, "CLEAR_ERRORS complete.");
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
