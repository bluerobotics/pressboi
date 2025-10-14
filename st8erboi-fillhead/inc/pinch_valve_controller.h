/**
 * @file pinch_valve_controller.h
 * @author Eldin Miller-Stead
 * @date September 10, 2025
 * @brief Defines the controller for a single motorized pinch valve.
 *
 * @details This file defines the `PinchValve` class, which encapsulates the logic for
 * controlling one of the motorized pinch valves. It includes a detailed state machine
 * for managing homing, opening, closing, and jogging operations. The class also handles
 * torque-based sensing for homing and provides a telemetry interface.
 */
#pragma once

#include "config.h"
#include "comms_controller.h"
#include "ClearCore.h"
#include "commands.h"

class Fillhead; // Forward declaration

/**
 * @enum PinchValveState
 * @brief Defines the main operational states of a pinch valve.
 */
enum PinchValveState {
	VALVE_NOT_HOMED,        ///< The valve has not been homed and its position is unknown. This is the default state on boot.
	VALVE_CLOSED,           ///< The valve is stationary in the fully closed position.
	VALVE_OPEN,             ///< The valve is stationary in the fully open position (after a successful home).
	VALVE_HALTED,           ///< The valve was stopped mid-move by an abort command and is in a known, intermediate position. It is still homed.
	VALVE_MOVING,           ///< The valve is actively moving between the open and closed positions.
	VALVE_HOMING,           ///< The valve is currently executing its homing sequence.
	VALVE_JOGGING,          ///< The valve is performing a manual jog move.
	VALVE_RESETTING,        ///< The valve is waiting for motion to stop before completing a reset.
	VALVE_ERROR,            ///< An error has occurred (e.g., timeout, motor fault, torque limit).
};

/**
 * @class PinchValve
 * @brief Controls a single motorized pinch valve.
 *
 * @details This class encapsulates all logic for a single pinch valve actuator.
 * It manages a multi-level state machine to handle complex, non-blocking
 * operations like the multi-phase homing sequence. It provides a simple public
 * interface (`open()`, `close()`, `home()`) while abstracting away the underlying
 * motor control, torque monitoring, and state transitions.
 */
class PinchValve {
public:
	/**
	 * @brief Constructs a new PinchValve object.
	 * @param name A string identifier for the valve (e.g., "InjectionValve"), used for logging.
	 * @param motor A pointer to the ClearCore `MotorDriver` object that this class will control.
	 * @param controller A pointer to the main `Fillhead` controller, used for reporting events.
	 */
	PinchValve(const char* name, MotorDriver* motor, Fillhead* controller);

	/**
	 * @brief Initializes the pinch valve motor and its configuration.
	 * @details Configures the motor's operational parameters and enables the driver.
	 * This should be called once at startup.
	 */
	void setup();

	/**
	 * @brief Updates the internal state machine for the pinch valve.
	 * @details This method should be called repeatedly in the main application loop. It is
	 * the core of the valve's non-blocking operation, advancing the state machines
	 * for homing, opening, closing, and jogging.
	 */
	void updateState();

	/**
	 * @brief Handles user commands specific to this pinch valve.
	 * @param cmd The `Command` enum representing the command to execute.
	 * @param args A C-style string containing any arguments for the command (e.g., jog distance).
	 */
	void handleCommand(Command cmd, const char* args);

	/**
	 * @brief Initiates the homing sequence for the valve.
	 * @param is_tubed A boolean indicating whether the homing sequence should
	 *                 use the parameters for a tubed or untubed state (affects torque and offset).
	 */
	void home(bool is_tubed);

	/**
	 * @brief Commands the valve to move to the fully open position (its homed position).
	 */
	void open();

	/**
	 * @brief Commands the valve to move to the fully closed (pinched) position.
	 */
	void close();

	/**
	 * @brief Commands the valve to perform a manual jog move.
	 * @param args A C-style string containing the distance and speed for the jog.
	 */
	void jog(const char* args);

	/**
	 * @brief Enables the motor for this valve.
	 */
	void enable();

	/**
	 * @brief Disables the motor for this valve.
	 */
	void disable();

	/**
	 * @brief Aborts any ongoing motion by commanding a deceleration stop.
	 */
	void abort();

	/**
	 * @brief Resets the valve's state machines and error conditions to their default idle state.
	 */
	void reset();

	/**
	 * @brief Checks if the valve has been successfully homed.
	 * @return `true` if homed, `false` otherwise.
	 */
	bool isHomed() const;

	/**
	 * @brief Checks if the valve is currently in the fully open state.
	 * @return `true` if the `m_state` is `VALVE_OPEN`.
	 */
	bool isOpen() const;

	/**
	 * @brief Gets the telemetry string for this valve.
	 * @return A constant character pointer to a formatted string of telemetry data
	 *         (e.g., position, torque, homing status).
	 */
	const char* getTelemetryString();

	/**
	 * @brief Checks if the valve's motor is in a hardware fault state.
	 * @return `true` if the motor driver's fault bit is set, `false` otherwise.
	 */
	bool isInFault() const;

	/**
	 * @brief Checks if the valve is busy with an operation.
	 * @return `true` if the valve is homing, opening, closing, or jogging.
	 */
	bool isBusy() const;

	/**
	 * @brief Gets the current state of the valve as a human-readable string.
	 * @return A `const char*` representing the current state (e.g., "VALVE_HOMING").
	 */
	const char* getState() const;

private:
	/**
	 * @enum MoveType
	 * @brief Defines the type of motion being performed in the VALVE_MOVING state.
	 */
	typedef enum {
		MOVE_TYPE_NONE,     ///< No specific move type is active.
		MOVE_TYPE_OPEN,     ///< The valve is performing an 'open' operation.
		MOVE_TYPE_CLOSE     ///< The valve is performing a 'close' operation.
	} MoveType;

	/**
	 * @brief Low-level helper to command a motor move.
	 * @param steps The target position relative to the current position, in motor steps.
	 * @param velocity_sps The maximum velocity for the move, in steps per second.
	 * @param accel_sps2 The acceleration for the move, in steps per second squared.
	 */
	void moveSteps(long steps, int velocity_sps, int accel_sps2);

	/**
	 * @brief Gets the smoothed motor torque value using an EWMA filter.
	 * @return The smoothed torque value as a percentage.
	 */
	float getSmoothedTorque();

	/**
	 * @brief Gets the instantaneous, unfiltered motor torque value.
	 * @return The current torque reading as a percentage.
	 */
	float getInstantaneousTorque();

	/**
	 * @brief Checks if the motor torque has exceeded the current limit.
	 * @return `true` if the torque limit is exceeded, `false` otherwise.
	 */
	bool checkTorqueLimit();

	/**
	 * @brief Formats and sends a status message via the main `Fillhead` controller.
	 * @param statusType The prefix for the message (e.g., "INFO: ").
	 * @param message The content of the message to send.
	 */
	void reportEvent(const char* statusType, const char* message);

	/**
	 * @enum OperatingPhase
	 * @brief Defines the sub-states for simple motion operations like open, close, or jog.
	 */
	typedef enum {
		PHASE_IDLE,         ///< No operation is active.
		PHASE_START,        ///< The operation is being initiated.
		PHASE_WAIT_TO_START,///< Waiting for the motor to begin moving before checking status.
		PHASE_MOVING        ///< The motor is actively moving towards its target.
	} OperatingPhase;

	/**
	 * @enum HomingPhase
	 * @brief Defines the detailed sub-states for the multi-step homing sequence.
	 */
	typedef enum {
		HOMING_PHASE_IDLE,              ///< Homing is not active.
		INITIAL_BACKOFF_START,          ///< Starting the initial move away from a potential start-on-hard-stop condition.
		INITIAL_BACKOFF_WAIT_TO_START,  ///< Waiting for the initial backoff move to start.
		INITIAL_BACKOFF_MOVING,         ///< Executing the initial backoff move.
		RAPID_SEARCH_START,             ///< Starting the high-speed search for the hard stop.
		RAPID_SEARCH_WAIT_TO_START,     ///< Waiting for the rapid search to start.
		RAPID_SEARCH_MOVING,            ///< Executing the rapid search and monitoring torque.
		BACKOFF_START,                  ///< Starting the move away from the hard stop after the first touch.
		BACKOFF_WAIT_TO_START,          ///< Waiting for the backoff move to start.
		BACKOFF_MOVING,                 ///< Executing the backoff move.
		SLOW_SEARCH_START,              ///< Starting the slow-speed search for a precise hard stop location.
		SLOW_SEARCH_WAIT_TO_START,      ///< Waiting for the slow search to start.
		SLOW_SEARCH_MOVING,             ///< Executing the slow search and monitoring torque.
		SET_OFFSET_START,               ///< Starting the final move to the operational 'open' position.
		SET_OFFSET_WAIT_TO_START,       ///< Waiting for the final offset move to start.
		SET_OFFSET_MOVING,              ///< Executing the final offset move.
		SET_ZERO                        ///< Setting the final 'open' position as the logical zero.
	} HomingPhase;

	const char* m_name;                 ///< The name of the valve instance (e.g., "InjectionValve").
	MotorDriver* m_motor;               ///< Pointer to the motor driver for this valve.
	Fillhead* m_controller;             ///< Pointer to the main `Fillhead` controller for event reporting.
	PinchValveState m_state;            ///< The main state of the pinch valve.
	HomingPhase m_homingPhase;          ///< The current phase of the homing sequence.
	OperatingPhase m_opPhase;           ///< The current phase of a standard open/close/jog operation.
	MoveType m_moveType;                ///< The type of move being performed (open or close).
	uint32_t m_moveStartTime;           ///< Timestamp (ms) when a move operation was started, used for timeout.
	bool m_isHomed;                     ///< Flag indicating if the valve has been successfully homed.
	uint32_t m_homingStartTime;         ///< Timestamp (ms) when a homing sequence was started, used for timeout.
	float m_torqueLimit;                ///< The current torque limit (%) for detecting hard stops.
	float m_smoothedTorque;             ///< The smoothed torque value (EWMA filtered).
	bool m_firstTorqueReading;          ///< Flag to handle the first reading for the torque smoothing filter.
	char m_telemetryBuffer[128];        ///< Buffer for the formatted telemetry string.
	long m_homingDistanceSteps;         ///< The total travel distance (steps) for homing searches.
	long m_homingBackoffSteps;          ///< The backoff distance (steps) used in homing.
	long m_homingFinalOffsetSteps;      ///< The final offset from the hard stop to the 'open' position, in steps.
	int m_homingUnifiedSps;             ///< The unified speed (steps/sec) for all homing moves.
	int m_homingAccelSps2;              ///< The acceleration (steps/sec^2) for homing moves.
	float m_homingSearchTorque;         ///< The torque limit (%) used for hard stop detection during homing.
	float m_homingBackoffTorque;        ///< The higher torque limit (%) used for backoff moves during homing.
};