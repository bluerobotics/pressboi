/**
 * @file injector_controller.h
 * @author Eldin Miller-Stead
 * @date September 10, 2025
 * @brief Defines the controller for the dual-motor material injector system.
 *
 * @details This file defines the `Injector` class, which is responsible for managing
 * the two ganged motors that drive the material dispensing mechanism. It contains
 * the complex state machines required for homing (both to the machine's physical
 * zero and to a material cartridge), precision injection moves, and manual jogging.
 * The various state enums that govern the injector's behavior are also defined here.
 */
#pragma once

#include "config.h"
#include "comms_controller.h"
#include "commands.h"

class Fillhead; // Forward declaration

/**
 * @enum HomingState
 * @brief Defines the top-level active homing operation for the injector motors.
 */
enum HomingState : uint8_t {
	HOMING_NONE,        ///< No homing operation is active.
	HOMING_MACHINE,     ///< Homing to the machine's physical zero point (fully retracted).
	HOMING_CARTRIDGE    ///< Homing to the front of the material cartridge.
};

/**
 * @enum HomingPhase
 * @brief Defines the detailed sub-state or "phase" within an active homing operation.
 * @note This is a legacy global definition and may differ from the private one inside the `Injector` class.
 */
enum HomingPhase : uint8_t {
	HOMING_PHASE_IDLE_GLOBAL,   ///< Homing is not active.
	HOMING_PHASE_STARTING_MOVE, ///< Initial delay to confirm motor movement has begun.
	HOMING_PHASE_RAPID_MOVE,    ///< High-speed move towards the hard stop.
	HOMING_PHASE_BACK_OFF,      ///< Moving away from the hard stop after initial contact.
	HOMING_PHASE_TOUCH_OFF,     ///< Slow-speed move to precisely locate the hard stop.
	HOMING_PHASE_RETRACT,       ///< Moving to a final, safe position after homing is complete.
	HOMING_PHASE_COMPLETE,      ///< The homing sequence finished successfully.
	HOMING_PHASE_ERROR_GLOBAL   ///< The homing sequence failed.
};

/**
 * @enum FeedState
 * @brief Defines the state of an injection or material feed operation.
 * @details This enum tracks the progress of a dispensing action, from starting
 * to pausing, resuming, and completing.
 */
enum FeedState : uint8_t {
	FEED_NONE,                  ///< No feed operation is active or defined.
	FEED_STANDBY,               ///< Ready to start a feed operation.
	FEED_INJECT_STARTING,       ///< An injection move has been commanded and is starting.
	FEED_INJECT_ACTIVE,         ///< An injection move is currently in progress.
	FEED_INJECT_PAUSED,         ///< An active injection has been paused by the user.
	FEED_INJECT_RESUMING,       ///< A paused injection is resuming.
	FEED_MOVING_TO_HOME,        ///< Moving to the previously established cartridge home position.
	FEED_MOVING_TO_RETRACT,     ///< Moving to a retracted position relative to the cartridge home.
	FEED_INJECTION_CANCELLED,   ///< The injection was cancelled by the user.
	FEED_INJECTION_COMPLETED    ///< The injection finished successfully.
};


/**
 * @class Injector
 * @brief Manages the dual-motor injector system for dispensing material.
 *
 * @details This class is the core of the material dispensing functionality. It orchestrates
 * the two ganged motors (M0 and M1) to perform complex, multi-stage operations.
 * Key responsibilities include:
 * - A hierarchical state machine for managing overall state (e.g., STANDBY, HOMING, FEEDING).
 * - Nested state machines for detailed processes like the multi-phase homing sequence.
 * - Torque-based sensing for detecting hard stops during homing and stall conditions.
 * - Volume-based control of injections, converting mL to motor steps.
 * - Handling user commands for jogging, homing, injecting, and pausing/resuming operations.
 * - Reporting detailed telemetry on position, torque, and operational status.
 */
class Injector {
public:
    /**
     * @brief Constructs a new Injector object.
     * @param motorA Pointer to the primary ClearCore motor driver (M0).
     * @param motorB Pointer to the secondary (ganged) ClearCore motor driver (M1).
     * @param controller Pointer to the main `Fillhead` controller, used for reporting events.
     */
    Injector(MotorDriver* motorA, MotorDriver* motorB, Fillhead* controller);

    /**
     * @brief Initializes the injector motors and their configurations.
     * @details Configures motor settings such as max velocity and acceleration, and enables
     * the motor drivers. This should be called once at startup.
     */
    void setup();

    /**
     * @brief Updates the internal state machines for the injector.
     * @details This is the heart of the injector's non-blocking operation. It should be
     * called repeatedly in the main application loop to advance the active state machines
     * for homing, feeding, and jogging operations.
     */
    void updateState();

    /**
     * @brief Handles user commands related to the injector.
     * @param cmd The `Command` enum representing the command to be executed.
     * @param args A C-style string containing any arguments for the command (e.g., distance, volume).
     */
    void handleCommand(Command cmd, const char* args);

    /**
     * @brief Gets the telemetry string for the injector system.
     * @return A constant character pointer to a formatted string containing key-value pairs
     *         of injector telemetry data (position, torque, homing status, etc.).
     */
    const char* getTelemetryString();

    /**
     * @brief Enables the injector motors and sets their default parameters.
     */
    void enable();

    /**
     * @brief Disables the injector motors.
     */
    void disable();

    /**
     * @brief Aborts any ongoing injector motion by commanding a deceleration stop.
     */
    void abortMove();

    /**
     * @brief Resets the injector's state machines and error conditions to their default idle state.
     */
    void reset();

    /**
     * @brief Checks if either of the injector motors is in a hardware fault state.
     * @return `true` if a motor is in fault, `false` otherwise.
     */
    bool isInFault() const;

    /**
     * @brief Checks if the injector is busy with any operation.
     * @return `true` if the injector's main state is not `STATE_STANDBY`.
     */
    bool isBusy() const;

    /**
     * @brief Gets the current high-level state of the injector as a human-readable string.
     * @return A `const char*` representing the current state (e.g., "Homing", "Feeding").
     */
    const char* getState() const;

private:
    /**
     * @name Private Helper Methods
     * @{
     */
    void startMove(long steps, int velSps, int accelSps2);
    bool isMoving();
    float getSmoothedTorque(MotorDriver *motor, float *smoothedValue, bool *firstRead);
    bool checkTorqueLimit();
    void finalizeAndResetActiveDispenseOperation(bool success);
    void fullyResetActiveDispenseOperation();
    void reportEvent(const char* statusType, const char* message);
    /** @} */

    /**
     * @name Private Command Handlers
     * @{
     */
    void jogMove(const char* args);
    void machineHome();
    void cartridgeHome();
    void moveToCartridgeHome();
    void moveToCartridgeRetract(const char* args);
    void initiateInjectMove(const char* args, float piston_a_diam, float piston_b_diam, const char* command_str);
    void pauseOperation();
    void resumeOperation();
    void cancelOperation();
    void setTorqueOffset(const char* args);
    /** @} */
    
    Fillhead* m_controller;      ///< Pointer to the main `Fillhead` controller for event reporting.
    MotorDriver* m_motorA;       ///< Pointer to the primary motor driver (M0).
    MotorDriver* m_motorB;       ///< Pointer to the secondary, ganged motor driver (M1).

    /**
     * @enum State
     * @brief Defines the top-level operational state of the injector.
     */
    typedef enum {
        STATE_STANDBY,      ///< Injector is idle and ready for commands.
        STATE_HOMING,       ///< Injector is performing a homing sequence.
        STATE_JOGGING,      ///< Injector is performing a manual jog move.
        STATE_FEEDING,      ///< Injector is performing an injection or retract move.
        STATE_MOTOR_FAULT   ///< An injector motor has entered a hardware fault state.
    } State;
    State m_state; ///< The current top-level state of the injector.

    HomingState m_homingState; ///< The type of homing being performed (Machine vs. Cartridge).

    /**
     * @enum HomingPhase
     * @brief Defines the detailed sub-states for the injector's internal homing sequence.
     */
    typedef enum {
		HOMING_PHASE_IDLE,              ///< Homing is not active.
		RAPID_SEARCH_START,             ///< Starting high-speed move towards hard stop.
		RAPID_SEARCH_WAIT_TO_START,     ///< Waiting for rapid move to begin before checking torque.
		RAPID_SEARCH_MOVING,            ///< Executing high-speed move and monitoring for torque limit.
		BACKOFF_START,                  ///< Starting move away from hard stop.
		BACKOFF_WAIT_TO_START,          ///< Waiting for backoff move to begin.
		BACKOFF_MOVING,                 ///< Executing backoff move.
		SLOW_SEARCH_START,              ///< Starting slow-speed move to find precise stop.
		SLOW_SEARCH_WAIT_TO_START,      ///< Waiting for slow move to begin.
		SLOW_SEARCH_MOVING,             ///< Executing slow-speed move and monitoring for torque limit.
		SET_OFFSET_START,               ///< Not used in this implementation.
		SET_OFFSET_WAIT_TO_START,       ///< Not used in this implementation.
		SET_OFFSET_MOVING,              ///< Not used in this implementation.
		SET_ZERO,                       ///< Setting the final position as the logical zero.
        HOMING_PHASE_ERROR              ///< Homing sequence failed.
	} HomingPhase;
    HomingPhase m_homingPhase; ///< The current phase of an active homing sequence.

    FeedState m_feedState;             ///< The current state of an injection or feed operation.
    bool m_homingMachineDone;          ///< Flag indicating if machine homing has been successfully completed.
    bool m_homingCartridgeDone;        ///< Flag indicating if cartridge homing has been successfully completed.
    uint32_t m_homingStartTime;        ///< Timestamp (ms) when the homing sequence started, used for timeout.
    bool m_isEnabled;                  ///< Flag indicating if motors are currently enabled.
    float m_torqueLimit;               ///< Current torque limit (%) for detecting hard stops or stalls.
    float m_torqueOffset;              ///< User-configurable offset (%) for torque readings to account for bias.
    float m_smoothedTorqueValue0, m_smoothedTorqueValue1; ///< Smoothed torque values for each motor.
    bool m_firstTorqueReading0, m_firstTorqueReading1;   ///< Flags for initializing the torque smoothing EWMA filter.
    int32_t m_machineHomeReferenceSteps, m_cartridgeHomeReferenceSteps; ///< Stored step counts for machine and cartridge home positions.
    float m_cumulative_dispensed_ml;   ///< Cumulative dispensed volume (mL) since the last cartridge home.
    int m_feedDefaultTorquePercent;    ///< Default torque (%) for feed moves.
    long m_feedDefaultVelocitySPS;     ///< Default velocity (steps/sec) for feed moves.
    long m_feedDefaultAccelSPS2;       ///< Default acceleration (steps/sec^2) for feed moves.
    long m_homingDistanceSteps;        ///< Max travel distance (steps) for a homing move.
    long m_homingBackoffSteps;         ///< Backoff distance (steps) for a homing move.
    int m_homingRapidSps;              ///< Rapid speed (steps/sec) for a homing move.
    int m_homingTouchSps;              ///< Slow touch-off speed (steps/sec) for a homing move.
    int m_homingBackoffSps;            ///< Backoff speed (steps/sec) for a homing move.
    int m_homingAccelSps2;             ///< Acceleration (steps/sec^2) for homing moves.
    const char* m_activeFeedCommand;   ///< Stores the original feed command string for logging upon completion.
    const char* m_activeJogCommand;    ///< Stores the original jog command string for logging upon completion.
    
    /**
     * @name Active Dispense Operation Variables
     * @{
     */
    float m_active_op_target_ml;            ///< Target dispense volume in mL for the current operation.
    float m_active_op_total_dispensed_ml;   ///< Total volume dispensed so far in the current operation.
    float m_last_completed_dispense_ml;     ///< The volume of the last successful dispense operation.
    float m_active_op_steps_per_ml;         ///< Conversion factor from steps to mL for the current operation.
    long m_active_op_total_target_steps;    ///< Target dispense distance in steps for the current operation.
    long m_active_op_remaining_steps;       ///< Remaining steps in the current operation (used for pause/resume).
    long m_active_op_segment_initial_axis_steps; ///< Start position for a move segment (used for pause/resume).
    long m_active_op_initial_axis_steps;    ///< Start position for the entire operation.
    int m_active_op_velocity_sps;           ///< Velocity (steps/sec) for the current operation.
    int m_active_op_accel_sps2;             ///< Acceleration (steps/sec^2) for the current operation.
    int m_active_op_torque_percent;         ///< Torque limit (%) for the current operation.
    uint32_t m_feedStartTime;               ///< Timestamp (ms) when a feed operation started.
    /** @} */
    
    char m_telemetryBuffer[256]; ///< Buffer for the formatted telemetry string.
};
