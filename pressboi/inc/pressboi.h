/**
 * @file pressboi.h
 * @author Eldin Miller-Stead
 * @date November 3, 2025
 * @brief Defines the master controller for the Pressboi press system.
 *
 * @details This header file defines the `Pressboi` class, which serves as the central
 * orchestrator for all press operations. It owns and manages the motor controller
 * and comms controller, and is responsible for the main application state machine,
 * command dispatch, and telemetry reporting.
 */
#pragma once

#include "config.h"
#include "comms_controller.h"
#include "motor_controller.h"
#include "commands.h"

/**
 * @enum MainState
 * @brief Defines the top-level operational state of the entire Pressboi system.
 * @details This enum represents the high-level status of the machine.
 */
enum MainState : uint8_t {
	STATE_STANDBY,       ///< System is idle, initialized, and ready to accept commands.
	STATE_BUSY,          ///< A non-error operation (e.g., homing, moving) is in progress.
	STATE_ERROR,         ///< A fault has occurred, typically a motor fault. Requires `CLEAR_ERRORS` to recover.
	STATE_DISABLED,      ///< System is disabled; motors will not move. Requires `ENABLE` to recover.
	STATE_CLEARING_ERRORS///< A special state to manage the non-blocking error recovery process.
};

/**
 * @enum ErrorState
 * @brief Defines various specific error conditions the system can encounter.
 */
enum ErrorState : uint8_t {
	ERROR_NONE,                   ///< No error.
	ERROR_MANUAL_ABORT,           ///< Operation aborted by user command.
	ERROR_TORQUE_ABORT,           ///< Operation aborted due to exceeding torque limits.
	ERROR_MOTION_EXCEEDED_ABORT,  ///< Operation aborted because motion limits were exceeded.
	ERROR_NO_HOME,                ///< A required home position is not set.
	ERROR_HOMING_TIMEOUT,         ///< Homing operation took too long to complete.
	ERROR_HOMING_NO_TORQUE_RAPID, ///< Homing failed to detect torque during the rapid move.
	ERROR_HOMING_NO_TORQUE_TOUCH, ///< Homing failed to detect torque during the touch-off move.
	ERROR_NOT_HOMED,              ///< An operation required homing, but the system is not homed.
	ERROR_INVALID_PARAMETERS,     ///< A command was received with invalid parameters.
	ERROR_MOTORS_DISABLED         ///< An operation was blocked because motors are disabled.
};

/**
 * @class Pressboi
 * @brief The master controller for the Pressboi press system.
 * @details This class acts as the "brain" of the press. It follows the Singleton
 * pattern (instantiated once globally) and is responsible for the entire application
 * lifecycle, from setup to the continuous execution of the main loop.
 */
class Pressboi {
public:
    /**
     * @brief Constructs the Pressboi master controller.
     * @details The constructor initializes all member variables and uses a member
     * initializer list to instantiate the specialized controllers.
     */
    Pressboi();

    /**
     * @brief Initializes all hardware and controllers for the entire system.
     * @details This method should be called once at startup.
     */
    void setup();

    /**
     * @brief The main execution loop for the Pressboi system.
     * @details This function is called continuously from `main()`. It orchestrates all
     * real-time operations in a non-blocking manner.
     */
    void loop();

    /**
     * @brief Public interface for sub-controllers to send status messages.
     * @param statusType The prefix for the message (e.g., "INFO: ").
     * @param message The content of the message to send.
     */
    void reportEvent(const char* statusType, const char* message);

private:
    /**
     * @brief Updates the main system state and the state machines of all sub-controllers.
     */
    void updateState();

	/**
	 * @brief Master command handler; dispatches incoming commands to the correct sub-system.
	 * @param msg The incoming message object containing the command to be executed.
	 */
    void dispatchCommand(const Message& msg);

	/**
	 * @brief Aggregates telemetry data from all sub-controllers and sends it as a single packet.
	 */
    void publishTelemetry();

    // --- System-Level Command Handlers ---
    /**
     * @brief Enables all motors and places the system in a ready state.
     */
    void enable();

    /**
     * @brief Disables all motors and stops all operations.
     */
    void disable();

    /**
     * @brief Halts all motion immediately and resets the system state to standby.
     */
    void abort();

    /**
     * @brief Resets any error states, clears motor faults, and returns the system to standby.
     */
    void clearErrors();

    /**
     * @brief Resets all sub-controllers to their idle states and sets the main state to STANDBY.
     */
    void standby();

public:
    // --- Component Ownership ---
    CommsController  m_comms;           ///< Manages all network and serial communication.

private:
    MotorController  m_motor;           ///< Manages the dual-motor press system.

    // Main system state machine
    MainState m_mainState;              ///< The current high-level state of the Pressboi system.

    // Timers for periodic tasks
    uint32_t m_lastTelemetryTime;       ///< Timestamp of the last telemetry transmission.
};
