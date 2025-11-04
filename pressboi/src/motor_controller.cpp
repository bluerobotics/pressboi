/**
 * @file motor_controller.cpp
 * @author Eldin Miller-Stead
 * @date November 3, 2025
 * @brief Implements the controller for the dual-motor press system.
 *
 * @details This file provides the concrete implementation for the `MotorController` class.
 * It contains the logic for the hierarchical state machines that manage homing
 * and moving operations. It also includes the command handlers, motion
 * control logic, and telemetry reporting for the motor system.
 */

//==================================================================================================
// --- Includes ---
//==================================================================================================
#include "motor_controller.h"
#include "pressboi.h" // Include full header for Pressboi
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

//==================================================================================================
// --- Class Implementation ---
//==================================================================================================

/**
 * @brief Constructs the MotorController controller.
 */
MotorController::MotorController(MotorDriver* motorA, MotorDriver* motorB, Pressboi* controller) {
    m_motorA = motorA;
    m_motorB = motorB;
    m_controller = controller;

    // Initialize state machine
    m_state = STATE_STANDBY;
    m_homingState = HOMING_NONE;
    m_homingPhase = HOMING_PHASE_IDLE;
    m_moveState = MOVE_STANDBY;

    m_homingDone = false;
    m_retractDone = false;
    m_homingStartTime = 0;
    m_isEnabled = true;
    m_pausedMessageSent = false;

    // Initialize from config file constants
    m_torqueLimit = DEFAULT_TORQUE_LIMIT;
    m_torqueOffset = DEFAULT_TORQUE_OFFSET;
    m_moveDefaultTorquePercent = MOVE_DEFAULT_TORQUE_PERCENT;
    m_moveDefaultVelocitySPS = MOVE_DEFAULT_VELOCITY_SPS;
    m_moveDefaultAccelSPS2 = MOVE_DEFAULT_ACCEL_SPS2;

    m_smoothedTorqueValue0 = 0.0f;
    m_smoothedTorqueValue1 = 0.0f;
    m_firstTorqueReading0 = true;
    m_firstTorqueReading1 = true;
    m_machineHomeReferenceSteps = 0;
    m_retractReferenceSteps = 0;
    m_cumulative_distance_mm = 0.0f;
    m_moveStartTime = 0;
    m_active_op_target_position_steps = 0;

    fullyResetActiveMove();
    m_activeMoveCommand = nullptr;
}

/**
 * @brief Performs one-time setup and configuration of the motors.
 */
void MotorController::setup() {
    m_motorA->HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
    m_motorA->HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
    m_motorA->VelMax(MOTOR_DEFAULT_VEL_MAX_SPS);
    m_motorA->AccelMax(MOTOR_DEFAULT_ACCEL_MAX_SPS2);

    m_motorB->HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
    m_motorB->HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
    m_motorB->VelMax(MOTOR_DEFAULT_VEL_MAX_SPS);
    m_motorB->AccelMax(MOTOR_DEFAULT_ACCEL_MAX_SPS2);

    m_motorA->EnableRequest(true);
    m_motorB->EnableRequest(true);
}

/**
 * @brief The main update loop for the motor controller's state machines.
 */
void MotorController::updateState() {
    switch (m_state) {
        case STATE_STANDBY:
        // Do nothing while in standby
        break;
        case STATE_MOTOR_FAULT:
        // Do nothing, wait for reset
        break;

        case STATE_HOMING: {
            switch (m_homingPhase) {
                case RAPID_SEARCH_START: {
                    reportEvent(STATUS_PREFIX_INFO, "Homing: Starting rapid search.");
                    m_torqueLimit = HOMING_SEARCH_TORQUE_PERCENT;
                    long rapid_search_steps = m_homingDistanceSteps;
                    if (m_homingState == HOMING) {
                        rapid_search_steps = -rapid_search_steps;
                    }
                    startMove(rapid_search_steps, m_homingRapidSps, m_homingAccelSps2);
                    m_homingStartTime = Milliseconds();
                    m_homingPhase = RAPID_SEARCH_WAIT_TO_START;
                    break;
                }
                case RAPID_SEARCH_WAIT_TO_START:
                    if (isMoving()) {
                        m_homingPhase = RAPID_SEARCH_MOVING;
                    }
                    else if (Milliseconds() - m_homingStartTime > 500) {
                        abortMove();
                        char errorMsg[200];
                        snprintf(errorMsg, sizeof(errorMsg), "Homing failed: Motor did not start moving. M0 Status=0x%04X, M1 Status=0x%04X",
                                 (unsigned int)m_motorA->StatusReg().reg, (unsigned int)m_motorB->StatusReg().reg);
                        reportEvent(STATUS_PREFIX_INFO, errorMsg);
                        m_state = STATE_STANDBY;
                        m_homingPhase = HOMING_PHASE_IDLE;
                    }
                break;
                case RAPID_SEARCH_MOVING: {
                    if (checkTorqueLimit()) {
                        reportEvent(STATUS_PREFIX_INFO, "Homing: Rapid search torque limit hit.");
                        m_homingPhase = BACKOFF_START;
                        } else if (!isMoving()) {
                        abortMove();
                        reportEvent(STATUS_PREFIX_ERROR, "Homing failed: Axis stopped before torque limit was reached.");
                        m_state = STATE_STANDBY;
                        m_homingPhase = HOMING_PHASE_IDLE;
                    }
                    break;
                }
                case BACKOFF_START: {
                    reportEvent(STATUS_PREFIX_INFO, "Homing: Starting backoff.");
                    m_torqueLimit = HOMING_BACKOFF_TORQUE_PERCENT;
                    long backoff_steps = (m_homingState == HOMING) ? m_homingBackoffSteps : -m_homingBackoffSteps;
                    startMove(backoff_steps, m_homingBackoffSps, m_homingAccelSps2);
                    m_homingPhase = BACKOFF_WAIT_TO_START;
                    break;
                }
                case BACKOFF_WAIT_TO_START:
                if (isMoving()) {
                    m_homingPhase = BACKOFF_MOVING;
                }
                break;
                case BACKOFF_MOVING:
                if (!isMoving()) {
                    reportEvent(STATUS_PREFIX_INFO, "Homing: Backoff complete.");
                    m_homingPhase = SLOW_SEARCH_START;
                }
                break;
                case SLOW_SEARCH_START: {
                    reportEvent(STATUS_PREFIX_INFO, "Homing: Starting slow search.");
                    m_torqueLimit = HOMING_SEARCH_TORQUE_PERCENT;
                    long slow_search_steps = (m_homingState == HOMING) ? -m_homingBackoffSteps * 2 : m_homingBackoffSteps * 2;
                    startMove(slow_search_steps, m_homingTouchSps, m_homingAccelSps2);
                    m_homingPhase = SLOW_SEARCH_WAIT_TO_START;
                    break;
                }
                case SLOW_SEARCH_WAIT_TO_START:
                if (isMoving()) {
                    m_homingPhase = SLOW_SEARCH_MOVING;
                }
                break;
                case SLOW_SEARCH_MOVING: {
                    if (checkTorqueLimit()) {
                        reportEvent(STATUS_PREFIX_INFO, "Homing: Precise position found. Moving to offset.");
                        m_homingPhase = SET_OFFSET_START;
                        } else if (!isMoving()) {
                        abortMove();
                        reportEvent(STATUS_PREFIX_ERROR, "Homing failed during slow search.");
                        m_state = STATE_STANDBY;
                        m_homingPhase = HOMING_PHASE_IDLE;
                    }
                    break;
                }
                case SET_OFFSET_START: {
                    m_torqueLimit = HOMING_BACKOFF_TORQUE_PERCENT;
                    long offset_steps = (m_homingState == HOMING) ? m_homingBackoffSteps : -m_homingBackoffSteps;
                    startMove(offset_steps, m_homingBackoffSps, m_homingAccelSps2);
                    m_homingPhase = SET_OFFSET_WAIT_TO_START;
                    break;
                }
                case SET_OFFSET_WAIT_TO_START:
                if (isMoving()) {
                    m_homingPhase = SET_OFFSET_MOVING;
                }
                break;
                case SET_OFFSET_MOVING:
                if (!isMoving()) {
                    reportEvent(STATUS_PREFIX_INFO, "Homing: Offset position reached.");
                    m_homingPhase = SET_ZERO;
                }
                break;
                case SET_ZERO: {
                    const char* commandStr = (m_homingState == HOMING) ? "home" : "cartridge_home";
                    
                    if (m_homingState == HOMING) {
                        m_machineHomeReferenceSteps = m_motorA->PositionRefCommanded();
                        m_homingDone = true;
                        } else { // HOMING_CARTRIDGE
                        m_retractReferenceSteps = m_motorA->PositionRefCommanded();
                        m_retractDone = true;
                    }
                    
                    // Send standardized DONE message with just the command name
                    reportEvent(STATUS_PREFIX_DONE, commandStr);

                    m_state = STATE_STANDBY;
                    m_homingPhase = HOMING_PHASE_IDLE;
                    break;
                }
                case HOMING_PHASE_ERROR:
                reportEvent(STATUS_PREFIX_ERROR, "Homing sequence ended with error.");
                m_state = STATE_STANDBY;
                m_homingPhase = HOMING_PHASE_IDLE;
                break;
                default:
                abortMove();
                reportEvent(STATUS_PREFIX_ERROR, "Unknown homing phase, aborting.");
                m_state = STATE_STANDBY;
                m_homingPhase = HOMING_PHASE_IDLE;
                break;
            }
            break;
        }

        case STATE_MOVING: {
            // Check force sensor status during move
            const char* errorMsg = nullptr;
            if (checkForceSensorStatus(&errorMsg)) {
                abortMove();
                char fullMsg[STATUS_MESSAGE_BUFFER_SIZE];
                snprintf(fullMsg, sizeof(fullMsg), "Move stopped: %s", errorMsg);
                reportEvent(STATUS_PREFIX_ERROR, fullMsg);
                m_moveState = MOVE_PAUSED;  // Hold the controller
                return;
            }
            
            // Check force limit during move (if force limit was set and sensor is enabled)
            #if FORCE_SENSOR_ENABLED
            if (m_moveState == MOVE_ACTIVE && m_active_op_force_limit_kg > 0.1f) {
                float current_force = m_controller->m_forceSensor.getForce();
                if (current_force >= m_active_op_force_limit_kg) {
                    abortMove();
                    char msg[STATUS_MESSAGE_BUFFER_SIZE];
                    snprintf(msg, sizeof(msg), "Force limit (%.2f kg) reached. Current force: %.2f kg", 
                             m_active_op_force_limit_kg, current_force);
                    reportEvent(STATUS_PREFIX_INFO, msg);
                    
                    // Handle action based on force_action parameter
                    if (strcmp(m_active_op_force_action, "retract") == 0) {
                        // Send DONE for the original command, then start retract
                        if (m_activeMoveCommand) {
                            reportEvent(STATUS_PREFIX_DONE, m_activeMoveCommand);
                        }
                        
                        if (m_retractReferenceSteps == 0) {
                            reportEvent(STATUS_PREFIX_ERROR, "Cannot retract: retract position not set.");
                            finalizeAndResetActiveMove(true);
                            m_state = STATE_STANDBY;
                        } else {
                            // Start retract move
                            m_moveState = MOVE_TO_HOME;
                            m_activeMoveCommand = "retract";
                            m_active_op_target_position_steps = m_retractReferenceSteps;
                            long current_pos = m_motorA->PositionRefCommanded();
                            long steps_to_retract = m_retractReferenceSteps - current_pos;
                            m_torqueLimit = DEFAULT_TORQUE_LIMIT;
                            startMove(steps_to_retract, m_moveDefaultVelocitySPS, m_moveDefaultAccelSPS2);
                            reportEvent(STATUS_PREFIX_START, "retract");
                        }
                    } else if (strcmp(m_active_op_force_action, "skip") == 0) {
                        // Skip the rest of the move - complete at current position and send DONE
                        if (m_activeMoveCommand) {
                            reportEvent(STATUS_PREFIX_DONE, m_activeMoveCommand);
                        }
                        finalizeAndResetActiveMove(true);
                        m_state = STATE_STANDBY;
                    } else {
                        // "hold" action - pause and wait for user
                        m_moveState = MOVE_PAUSED;
                    }
                    return;
                }
            }
            #endif // FORCE_SENSOR_ENABLED
            
            if (checkTorqueLimit()) {
                reportEvent(STATUS_PREFIX_ERROR,"MOVE: Torque limit! Operation stopped.");
                finalizeAndResetActiveMove(false);
                m_state = STATE_STANDBY;
                return;
            }

            // Transition from STARTING/RESUMING to ACTIVE when motor begins moving
            if ((m_moveState == MOVE_STARTING || m_moveState == MOVE_RESUMING) && isMoving()) {
                m_moveState = MOVE_ACTIVE;
                m_active_op_segment_initial_axis_steps = m_motorA->PositionRefCommanded();
            }

            // Only check for move completion when in ACTIVE or RESUMING state
            // Don't send DONE while still in STARTING state (motor hasn't begun moving yet)
            if (!isMoving() && m_moveState != MOVE_PAUSED) {
                bool isStarting = (m_moveState == MOVE_STARTING || m_moveState == MOVE_RESUMING);
                uint32_t elapsed = Milliseconds() - m_moveStartTime;
                
                // If still in STARTING/RESUMING state, only timeout if we've waited too long
                // Otherwise, only send DONE if we're in ACTIVE state
                if (isStarting && elapsed > MOVE_START_TIMEOUT_MS) {
                    // Timeout - motor never started
                    if (m_activeMoveCommand) {
                        reportEvent(STATUS_PREFIX_ERROR, "Move timeout: Motor failed to start");
                    }
                    finalizeAndResetActiveMove(false);
                    m_state = STATE_STANDBY;
                } else if (!isStarting) {
                    // Move completed normally (was in ACTIVE state)
                    if (m_activeMoveCommand) {
                        // Send standardized DONE message with just the command name
                        reportEvent(STATUS_PREFIX_DONE, m_activeMoveCommand);
                    }
                    finalizeAndResetActiveMove(true);
                    m_state = STATE_STANDBY;
                }
            }

            if (m_moveState == MOVE_ACTIVE) {
                // Update distance traveled in mm
                long current_pos = m_motorA->PositionRefCommanded();
                long steps_moved_since_start = current_pos - m_active_op_initial_axis_steps;
                m_active_op_total_distance_mm = (float)std::abs(steps_moved_since_start) / STEPS_PER_MM;
            }

            if (m_moveState == MOVE_PAUSED && !isMoving()) {
                // Calculate remaining steps based on distance traveled (only once when first entering paused state)
                if (!m_pausedMessageSent) {
                    long steps_moved = m_active_op_initial_axis_steps + (long)(m_active_op_total_distance_mm * STEPS_PER_MM) - m_active_op_initial_axis_steps;
                    m_active_op_remaining_steps = m_active_op_total_target_steps - std::abs(steps_moved);
                    if (m_active_op_remaining_steps < 0) m_active_op_remaining_steps = 0;
                    reportEvent(STATUS_PREFIX_INFO, "Move: Operation Paused. Waiting for Resume/Cancel.");
                    m_pausedMessageSent = true;
                }
            } else {
                m_pausedMessageSent = false;  // Reset flag when not paused
            }
            break;
        }

    }
}

/**
 * @brief Handles a command specifically for the motor system.
 */
void MotorController::handleCommand(Command cmd, const char* args) {
    if (!m_isEnabled) {
        reportEvent(STATUS_PREFIX_ERROR, "Motor command ignored: Motors are disabled.");
        return;
    }
	
	if (m_motorA->StatusReg().bit.MotorInFault || m_motorB->StatusReg().bit.MotorInFault) {
        char errorMsg[200];
        snprintf(errorMsg, sizeof(errorMsg), "Motor command ignored: Motor in fault. M0 Status=0x%04X, M1 Status=0x%04X",
                 (unsigned int)m_motorA->StatusReg().reg, (unsigned int)m_motorB->StatusReg().reg);
        reportEvent(STATUS_PREFIX_ERROR, errorMsg);
        return;
    }
	
    if (m_state != STATE_STANDBY &&
    (cmd == CMD_HOME || cmd == CMD_MOVE_ABS || cmd == CMD_MOVE_INC || cmd == CMD_RETRACT)) {
        reportEvent(STATUS_PREFIX_ERROR, "Motor command ignored: Another operation is in progress.");
        return;
    }

    switch(cmd) {
        case CMD_HOME:
            home();
            break;
        case CMD_MOVE_ABS:
            moveAbsolute(args);
            break;
        case CMD_MOVE_INC:
            moveIncremental(args);
            break;
        case CMD_SET_RETRACT:
            setRetract(args);
            break;
        case CMD_RETRACT:
            retract(args);
            break;
        default:
            break;
    }
}

/**
 * @brief Enables both motors.
 */
void MotorController::enable() {
    m_motorA->EnableRequest(true);
    m_motorB->EnableRequest(true);

    // Always set motor parameters on enable to ensure a known good state after a fault,
    // as the ClearCore driver may reset them to zero.
    m_motorA->VelMax(MOTOR_DEFAULT_VEL_MAX_SPS);
    m_motorA->AccelMax(MOTOR_DEFAULT_ACCEL_MAX_SPS2);
    m_motorB->VelMax(MOTOR_DEFAULT_VEL_MAX_SPS);
    m_motorB->AccelMax(MOTOR_DEFAULT_ACCEL_MAX_SPS2);
    
    m_isEnabled = true;
    reportEvent(STATUS_PREFIX_INFO, "Motors enabled.");
}

/**
 * @brief Disables both motors.
 */
void MotorController::disable() {
    m_motorA->EnableRequest(false);
    m_motorB->EnableRequest(false);
    m_isEnabled = false;
    reportEvent(STATUS_PREFIX_INFO, "Motors disabled.");
}

/**
 * @brief Decelerates any ongoing motion to a stop and resets the state machines.
 */
void MotorController::abortMove() {
    m_motorA->MoveStopDecel();
    m_motorB->MoveStopDecel();
    Delay_ms(POST_ABORT_DELAY_MS);
}

/**
 * @brief Resets all state machines to their idle state.
 */
void MotorController::reset() {
    m_state = STATE_STANDBY;
    m_homingState = HOMING_NONE;
    m_homingPhase = HOMING_PHASE_IDLE;
    m_moveState = MOVE_STANDBY;
    fullyResetActiveMove();
}

/**
 * @brief Handles the HOME command.
 */
void MotorController::home() {
    // Check force sensor status before starting
    const char* errorMsg = nullptr;
    if (checkForceSensorStatus(&errorMsg)) {
        char fullMsg[STATUS_MESSAGE_BUFFER_SIZE];
        snprintf(fullMsg, sizeof(fullMsg), "Homing aborted: %s", errorMsg);
        reportEvent(STATUS_PREFIX_ERROR, fullMsg);
        return;
    }
    
    m_homingDistanceSteps = (long)(fabs(HOMING_STROKE_MM) * STEPS_PER_MM);
    m_homingBackoffSteps = (long)(HOMING_BACKOFF_MM * STEPS_PER_MM);
    m_homingRapidSps = (int)fabs(HOMING_RAPID_VEL_MMS * STEPS_PER_MM);
    m_homingBackoffSps = (int)fabs(HOMING_BACKOFF_VEL_MMS * STEPS_PER_MM);
    m_homingTouchSps = (int)fabs(HOMING_TOUCH_VEL_MMS * STEPS_PER_MM);
    m_homingAccelSps2 = (int)fabs(HOMING_ACCEL_MMSS * STEPS_PER_MM);
    
    // Set target position to 0 (home position) for telemetry
    m_active_op_target_position_steps = 0;
    
    char logMsg[200];
    snprintf(logMsg, sizeof(logMsg), "Homing params: dist_steps=%ld, rapid_sps=%d, touch_sps=%d, accel_sps2=%d",
             m_homingDistanceSteps, m_homingRapidSps, m_homingTouchSps, m_homingAccelSps2);
    reportEvent(STATUS_PREFIX_INFO, logMsg);

    if (m_homingDistanceSteps == 0) {
        reportEvent(STATUS_PREFIX_ERROR, "Homing failed: Calculated distance is zero. Check config.");
        return;
    }

    m_state = STATE_HOMING;
    m_homingState = HOMING;
    m_homingPhase = RAPID_SEARCH_START;
    m_homingStartTime = Milliseconds();
    m_homingDone = false;

    reportEvent(STATUS_PREFIX_START, "HOME initiated.");
}

/**
 * @brief Handles the SET_RETRACT command.
 */
void MotorController::setRetract(const char* args) {
    if (!m_homingDone) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Must home before setting retract position.");
        return;
    }
    
    float position_mm = 0.0f;
    if (std::sscanf(args, "%f", &position_mm) != 1) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Invalid position for SET_RETRACT.");
        return;
    }
    
    // Store the retract position as an offset from home
    long position_steps = (long)(position_mm * STEPS_PER_MM);
    m_retractReferenceSteps = m_machineHomeReferenceSteps + position_steps;
    
    char msg[128];
    snprintf(msg, sizeof(msg), "Retract position set to %.2f mm (%ld steps from home)", position_mm, position_steps);
    reportEvent(STATUS_PREFIX_INFO, msg);
    
    // Send standardized DONE message
    reportEvent(STATUS_PREFIX_DONE, "set_retract");
}

/**
 * @brief Handles the RETRACT command - move to preset retract position.
 */
void MotorController::retract(const char* args) {
    if (!m_homingDone) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Must home before moving to retract position.");
        return;
    }
    
    if (m_retractReferenceSteps == 0) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Retract position not set. Use SET_RETRACT first.");
        return;
    }
    
    // Check force sensor status before starting
    const char* errorMsg = nullptr;
    if (checkForceSensorStatus(&errorMsg)) {
        char fullMsg[STATUS_MESSAGE_BUFFER_SIZE];
        snprintf(fullMsg, sizeof(fullMsg), "Move aborted: %s", errorMsg);
        reportEvent(STATUS_PREFIX_ERROR, fullMsg);
        return;
    }
    
    // Parse optional speed parameter
    float speed_mms = MOVE_DEFAULT_VELOCITY_MMS;
    if (args && args[0] != '\0') {
        int parsed = std::sscanf(args, "%f", &speed_mms);
        if (parsed < 1) {
            reportEvent(STATUS_PREFIX_ERROR, "Error: Invalid speed for RETRACT.");
            return;
        }
    }
    
    // Limit speed to 100 mm/s for safety
    if (speed_mms > 100.0f) {
        speed_mms = 100.0f;
        reportEvent(STATUS_PREFIX_INFO, "Speed limited to 100 mm/s for safety.");
    }
    
    fullyResetActiveMove();
    m_state = STATE_MOVING;
    m_moveState = MOVE_TO_HOME;
    m_activeMoveCommand = "retract";
    
    m_active_op_target_position_steps = m_retractReferenceSteps;  // Store for telemetry
    long current_pos = m_motorA->PositionRefCommanded();
    long steps_to_move = m_retractReferenceSteps - current_pos;
    
    int velocity_sps = (int)(speed_mms * STEPS_PER_MM);
    m_torqueLimit = DEFAULT_TORQUE_LIMIT;  // Use default motor torque limit (80%)
    
    // Store move parameters for pause/resume
    m_active_op_initial_axis_steps = current_pos;
    m_active_op_total_target_steps = std::abs(steps_to_move);
    m_active_op_velocity_sps = velocity_sps;
    m_active_op_accel_sps2 = m_moveDefaultAccelSPS2;
    m_active_op_torque_percent = (int)m_torqueLimit;
    m_moveStartTime = Milliseconds();  // Set start time for timeout tracking
    
    startMove(steps_to_move, velocity_sps, m_moveDefaultAccelSPS2);
    
    char msg[128];
    snprintf(msg, sizeof(msg), "retract to %.2f mm at %.2f mm/s initiated", 
             (float)(m_retractReferenceSteps - m_machineHomeReferenceSteps) / STEPS_PER_MM, speed_mms);
    reportEvent(STATUS_PREFIX_START, msg);
}

/**
 * @brief Handles the MOVE_ABS command - move to absolute position.
 */
void MotorController::moveAbsolute(const char* args) {
    if (!m_homingDone) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Must home before absolute moves.");
        return;
    }
    
    // Check force sensor status before starting
    const char* errorMsg = nullptr;
    if (checkForceSensorStatus(&errorMsg)) {
        char fullMsg[STATUS_MESSAGE_BUFFER_SIZE];
        snprintf(fullMsg, sizeof(fullMsg), "Move aborted: %s", errorMsg);
        reportEvent(STATUS_PREFIX_ERROR, fullMsg);
        return;
    }
    
    float position_mm = 0.0f;
    float speed_mms = MOVE_DEFAULT_VELOCITY_MMS;
    float force_kg = 0.0f;
    char force_action[32] = "hold";  // Default action
    
    // Parse: position, speed, force, [force_action]
    int parsed = std::sscanf(args, "%f %f %f %31s", &position_mm, &speed_mms, &force_kg, force_action);
    if (parsed < 1) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Invalid parameters for MOVE_ABS. Need at least position.");
        return;
    }
    
    // Limit speed to 100 mm/s for safety
    if (speed_mms > 100.0f) {
        speed_mms = 100.0f;
        reportEvent(STATUS_PREFIX_INFO, "Speed limited to 100 mm/s for safety.");
    }
    
    // Check if current force already exceeds target (for "hold" action)
    #if FORCE_SENSOR_ENABLED
    float current_force = m_controller->m_forceSensor.getForce();
    if (parsed >= 4 && strcmp(force_action, "hold") == 0 && current_force >= force_kg) {
        char msg[STATUS_MESSAGE_BUFFER_SIZE];
        snprintf(msg, sizeof(msg), "Force limit (%.2f kg) already reached. Current force: %.2f kg", 
                 force_kg, current_force);
        reportEvent(STATUS_PREFIX_ERROR, msg);  // ERROR message will auto-hold script
        return;
    }
    #endif
    
    fullyResetActiveMove();
    m_state = STATE_MOVING;
    m_moveState = MOVE_STARTING;
    m_activeMoveCommand = "move_abs";
    
    long target_steps = m_machineHomeReferenceSteps + (long)(position_mm * STEPS_PER_MM);
    m_active_op_target_position_steps = target_steps;  // Store for telemetry
    long current_pos = m_motorA->PositionRefCommanded();
    long steps_to_move = target_steps - current_pos;
    
    int velocity_sps = (int)(speed_mms * STEPS_PER_MM);
    m_torqueLimit = DEFAULT_TORQUE_LIMIT;  // Use default motor torque limit (80%)
    
    // Store move parameters for pause/resume
    m_active_op_initial_axis_steps = current_pos;
    m_active_op_total_target_steps = std::abs(steps_to_move);
    m_active_op_velocity_sps = velocity_sps;
    m_active_op_accel_sps2 = m_moveDefaultAccelSPS2;
    m_active_op_torque_percent = (int)m_torqueLimit;
    m_moveStartTime = Milliseconds();  // Set start time for timeout tracking
    
    // Store force limit and action for use during move
    m_active_op_force_limit_kg = force_kg;
    strncpy(m_active_op_force_action, force_action, sizeof(m_active_op_force_action) - 1);
    m_active_op_force_action[sizeof(m_active_op_force_action) - 1] = '\0';
    
    startMove(steps_to_move, velocity_sps, m_moveDefaultAccelSPS2);
    
    char msg[128];
    snprintf(msg, sizeof(msg), "move_abs to %.2f mm initiated", position_mm);
    reportEvent(STATUS_PREFIX_START, msg);
}

/**
 * @brief Handles the MOVE_INC command - move by incremental distance.
 */
void MotorController::moveIncremental(const char* args) {
    if (!m_homingDone) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Must home before incremental moves.");
        return;
    }
    
    // Check force sensor status before starting
    const char* errorMsg = nullptr;
    if (checkForceSensorStatus(&errorMsg)) {
        char fullMsg[STATUS_MESSAGE_BUFFER_SIZE];
        snprintf(fullMsg, sizeof(fullMsg), "Move aborted: %s", errorMsg);
        reportEvent(STATUS_PREFIX_ERROR, fullMsg);
        return;
    }
    
    float distance_mm = 0.0f;
    float speed_mms = MOVE_DEFAULT_VELOCITY_MMS;
    float force_kg = 0.0f;
    char force_action[32] = "hold";  // Default action
    
    // Parse: distance, speed, force, [force_action]
    int parsed = std::sscanf(args, "%f %f %f %31s", &distance_mm, &speed_mms, &force_kg, force_action);
    if (parsed < 1) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Invalid parameters for MOVE_INC. Need at least distance.");
        return;
    }
    
    // Limit speed to 100 mm/s for safety
    if (speed_mms > 100.0f) {
        speed_mms = 100.0f;
        reportEvent(STATUS_PREFIX_INFO, "Speed limited to 100 mm/s for safety.");
    }
    
    // Check if current force already exceeds target (for "hold" action)
    #if FORCE_SENSOR_ENABLED
    float current_force = m_controller->m_forceSensor.getForce();
    if (parsed >= 4 && strcmp(force_action, "hold") == 0 && current_force >= force_kg) {
        char msg[STATUS_MESSAGE_BUFFER_SIZE];
        snprintf(msg, sizeof(msg), "Force limit (%.2f kg) already reached. Current force: %.2f kg", 
                 force_kg, current_force);
        reportEvent(STATUS_PREFIX_ERROR, msg);  // ERROR message will auto-hold script
        return;
    }
    #endif
    
    fullyResetActiveMove();
    m_state = STATE_MOVING;
    m_moveState = MOVE_STARTING;
    m_activeMoveCommand = "move_inc";
    
    long steps_to_move = (long)(distance_mm * STEPS_PER_MM);
    long current_pos = m_motorA->PositionRefCommanded();
    m_active_op_target_position_steps = current_pos + steps_to_move;  // Store for telemetry
    int velocity_sps = (int)(speed_mms * STEPS_PER_MM);
    m_torqueLimit = DEFAULT_TORQUE_LIMIT;  // Use default motor torque limit (80%)
    
    // Store move parameters for pause/resume
    m_active_op_initial_axis_steps = current_pos;
    m_active_op_total_target_steps = std::abs(steps_to_move);
    m_active_op_velocity_sps = velocity_sps;
    m_active_op_accel_sps2 = m_moveDefaultAccelSPS2;
    m_active_op_torque_percent = (int)m_torqueLimit;
    m_moveStartTime = Milliseconds();  // Set start time for timeout tracking
    
    // Store force limit and action for use during move
    m_active_op_force_limit_kg = force_kg;
    strncpy(m_active_op_force_action, force_action, sizeof(m_active_op_force_action) - 1);
    m_active_op_force_action[sizeof(m_active_op_force_action) - 1] = '\0';
    
    startMove(steps_to_move, velocity_sps, m_moveDefaultAccelSPS2);
    
    char msg[128];
    snprintf(msg, sizeof(msg), "move_inc by %.2f mm initiated", distance_mm);
    reportEvent(STATUS_PREFIX_START, msg);
}

/**
 * @brief Handles pause request from GUI (script hold event).
 * Works for any active move - homing or feeding.
 */
void MotorController::pauseOperation() {
    if (m_state == STATE_HOMING) {
        // Pause homing - stop motors and save current phase
        abortMove();
        reportEvent(STATUS_PREFIX_INFO, "Homing paused. Send resume to continue.");
        reportEvent(STATUS_PREFIX_DONE, "pause");
    } else if (m_state == STATE_MOVING) {
        if (m_moveState == MOVE_ACTIVE || m_moveState == MOVE_STARTING || 
            m_moveState == MOVE_TO_HOME || m_moveState == MOVE_TO_RETRACT) {
            abortMove();
            m_moveState = MOVE_PAUSED;
            reportEvent(STATUS_PREFIX_INFO, "Move paused. Send resume to continue.");
            reportEvent(STATUS_PREFIX_DONE, "pause");
        } else {
            reportEvent(STATUS_PREFIX_INFO, "No active move to pause.");
            reportEvent(STATUS_PREFIX_DONE, "pause");
        }
    } else {
        reportEvent(STATUS_PREFIX_INFO, "No active operation to pause.");
        reportEvent(STATUS_PREFIX_DONE, "pause");
    }
}

/**
 * @brief Handles resume request from GUI (script run event).
 * Resumes paused homing or feeding operations.
 */
void MotorController::resumeOperation() {
    if (m_state == STATE_HOMING) {
        // Resume homing from current phase
        reportEvent(STATUS_PREFIX_INFO, "Homing resumed.");
        reportEvent(STATUS_PREFIX_DONE, "resume");
        // The homing state machine will automatically continue from its current phase
        // No need to restart - it will pick up in the next updateState() call
    } else if (m_state == STATE_MOVING) {
        if (m_moveState == MOVE_PAUSED) {
            // Calculate remaining steps NOW (at resume time) based on current position
            long current_pos = m_motorA->PositionRefCommanded();
            long steps_moved_so_far = current_pos - m_active_op_initial_axis_steps;
            long remaining_steps = m_active_op_total_target_steps - std::abs(steps_moved_so_far);
            
            // Resume paused moves
            if (remaining_steps > 0) {
                m_active_op_remaining_steps = remaining_steps;
                m_active_op_segment_initial_axis_steps = current_pos;
                m_moveState = MOVE_RESUMING;
                m_torqueLimit = (float)m_active_op_torque_percent;
                m_moveStartTime = Milliseconds();  // Reset start time for timeout tracking
                startMove(m_active_op_remaining_steps, m_active_op_velocity_sps, m_active_op_accel_sps2);
                reportEvent(STATUS_PREFIX_INFO, "Move resumed.");
                reportEvent(STATUS_PREFIX_DONE, "resume");
            } else {
                // Move already complete
                reportEvent(STATUS_PREFIX_INFO, "Move already complete.");
                fullyResetActiveMove();
                m_state = STATE_STANDBY;
                reportEvent(STATUS_PREFIX_DONE, "resume");
            }
        } else {
            reportEvent(STATUS_PREFIX_INFO, "No paused move to resume.");
            reportEvent(STATUS_PREFIX_DONE, "resume");
        }
    } else {
        reportEvent(STATUS_PREFIX_INFO, "No paused operation to resume.");
        reportEvent(STATUS_PREFIX_DONE, "resume");
    }
}

/**
 * @brief Handles cancel/reset request from GUI (script reset event).
 * Cancels any active operation and returns to standby.
 */
void MotorController::cancelOperation() {
    if (m_state == STATE_HOMING) {
        abortMove();
        m_homingPhase = HOMING_PHASE_IDLE;
        m_homingState = HOMING_NONE;
        m_state = STATE_STANDBY;
        reportEvent(STATUS_PREFIX_INFO, "Homing cancelled. Returning to standby.");
        reportEvent(STATUS_PREFIX_DONE, "cancel");
    } else if (m_state == STATE_MOVING) {
        abortMove();
        finalizeAndResetActiveMove(false);
        m_state = STATE_STANDBY;
        reportEvent(STATUS_PREFIX_INFO, "Move cancelled. Returning to standby.");
        reportEvent(STATUS_PREFIX_DONE, "cancel");
    } else {
        reportEvent(STATUS_PREFIX_INFO, "No active operation to cancel.");
        reportEvent(STATUS_PREFIX_DONE, "cancel");
    }
}

/**
 * @brief Commands a synchronized move on both motors.
 */
void MotorController::startMove(long steps, int velSps, int accelSps2) {
    m_firstTorqueReading0 = true;
    m_firstTorqueReading1 = true;

    char logMsg[128];
    snprintf(logMsg, sizeof(logMsg), "startMove called: steps=%ld, vel=%d, accel=%d, torque=%.1f", steps, velSps, accelSps2, m_torqueLimit);
    reportEvent(STATUS_PREFIX_INFO, logMsg);

    if (steps == 0) {
        reportEvent(STATUS_PREFIX_INFO, "startMove called with 0 steps. No move will occur.");
        return;
    }

    m_motorA->VelMax(velSps);
    m_motorA->AccelMax(accelSps2);
    m_motorB->VelMax(velSps);
    m_motorB->AccelMax(accelSps2);

    m_motorA->Move(steps);
    m_motorB->Move(steps);
}

/**
 * @brief Checks if either of the motors are currently active.
 */
bool MotorController::isMoving() {
    if (!m_isEnabled) return false;
    bool m0_moving = m_motorA->StatusReg().bit.StepsActive;
    bool m1_moving = m_motorB->StatusReg().bit.StepsActive;
    return (m0_moving || m1_moving);
}

/**
 * @brief Gets a smoothed torque value from a motor using an EWMA filter.
 */
float MotorController::getSmoothedTorque(MotorDriver *motor, float *smoothedValue, bool *firstRead) {
    // If the motor is not actively moving, torque is effectively zero.
    if (!motor->StatusReg().bit.StepsActive) {
        *firstRead = true; // Reset for the next move
        return 0.0f;
    }

    float currentRawTorque = motor->HlfbPercent();

    // The driver may return the sentinel value if a reading is not available yet (e.g., at the start of a move).
    // Treat it as "no data" and return 0 to avoid false torque limit trips.
    if (currentRawTorque == TORQUE_HLFB_AT_POSITION) {
        return 0.0f;
    }

    if (*firstRead) {
        *smoothedValue = currentRawTorque;
        *firstRead = false;
    } else {
        *smoothedValue = EWMA_ALPHA_TORQUE * currentRawTorque + (1.0f - EWMA_ALPHA_TORQUE) * (*smoothedValue);
    }
    return *smoothedValue + m_torqueOffset;
}

/**
 * @brief Checks if the torque on either motor has exceeded the current limit.
 */
bool MotorController::checkTorqueLimit() {
    if (isMoving()) {
        float torque0 = getSmoothedTorque(m_motorA, &m_smoothedTorqueValue0, &m_firstTorqueReading0);
        float torque1 = getSmoothedTorque(m_motorB, &m_smoothedTorqueValue1, &m_firstTorqueReading1);

        bool m0_over_limit = (torque0 != TORQUE_HLFB_AT_POSITION && std::abs(torque0) > m_torqueLimit);
        bool m1_over_limit = (torque1 != TORQUE_HLFB_AT_POSITION && std::abs(torque1) > m_torqueLimit);

        if (m0_over_limit || m1_over_limit) {
            abortMove();
            char torque_msg[STATUS_MESSAGE_BUFFER_SIZE];
            std::snprintf(torque_msg, sizeof(torque_msg), "TORQUE LIMIT REACHED (%.1f%%)", m_torqueLimit);
            reportEvent(STATUS_PREFIX_INFO, torque_msg);
            return true;
        }
    }
    return false;
}

/**
 * @brief Checks force sensor status for errors.
 * @param errorMsg Output parameter for error message (if any)
 * @return true if there's an error, false if force sensor is OK
 */
bool MotorController::checkForceSensorStatus(const char** errorMsg) {
    // Skip force sensor checks if disabled in config
    #if !FORCE_SENSOR_ENABLED
        return false;
    #endif
    
    // Check if force sensor is connected
    if (!m_controller->m_forceSensor.isConnected()) {
        *errorMsg = "Force sensor disconnected";
        return true;
    }
    
    // Check force reading validity
    float force = m_controller->m_forceSensor.getForce();
    
    if (force < FORCE_SENSOR_MIN_KG) {
        *errorMsg = "Force sensor error: reading below minimum (-10 kg)";
        return true;
    }
    
    if (force > FORCE_SENSOR_MAX_LIMIT_KG) {
        *errorMsg = "Force sensor error: reading above maximum (1440 kg)";
        return true;
    }
    
    return false;
}

/**
 * @brief Finalizes a move operation, updating cumulative distance.
 */
void MotorController::finalizeAndResetActiveMove(bool success) {
    if (success) {
        // With the new polling logic in updateState, m_active_op_total_distance_mm should be up-to-date.
        m_last_completed_distance_mm = m_active_op_total_distance_mm;
        m_cumulative_distance_mm += m_active_op_total_distance_mm;
    }
    fullyResetActiveMove();
}

/**
 * @brief Resets all variables related to an active move operation.
 */
void MotorController::fullyResetActiveMove() {
    m_active_op_force_limit_kg = 0.0f;
    m_active_op_force_action[0] = '\0';
    m_active_op_total_distance_mm = 0.0f;
    m_last_completed_distance_mm = 0.0f;
    m_active_op_total_target_steps = 0;
    // DON'T reset m_active_op_target_position_steps - retain last target for telemetry
    m_active_op_remaining_steps = 0;
    m_active_op_segment_initial_axis_steps = 0;
    m_active_op_initial_axis_steps = 0;
    m_activeMoveCommand = nullptr;
}

void MotorController::reportEvent(const char* statusType, const char* message) {
    char fullMsg[STATUS_MESSAGE_BUFFER_SIZE];
    snprintf(fullMsg, sizeof(fullMsg), "Motor: %s", message);
    m_controller->reportEvent(statusType, fullMsg);
}

/**
 * @brief Updates the telemetry data structure with current motor state.
 */
void MotorController::updateTelemetry(TelemetryData* data) {
    if (data == NULL) return;
    
    float displayTorque0 = getSmoothedTorque(m_motorA, &m_smoothedTorqueValue0, &m_firstTorqueReading0);
    float displayTorque1 = getSmoothedTorque(m_motorB, &m_smoothedTorqueValue1, &m_firstTorqueReading1);
    
    long current_pos_steps_m0 = m_motorA->PositionRefCommanded();
    float current_pos_mm = (float)(current_pos_steps_m0 - m_machineHomeReferenceSteps) / STEPS_PER_MM;

    int enabled0 = m_motorA->StatusReg().bit.Enabled;
    int enabled1 = m_motorB->StatusReg().bit.Enabled;

    // Update telemetry structure
    // Note: force is now read from ForceSensor and updated in pressboi.cpp
    data->force_limit = m_torqueLimit;
    data->enabled0 = enabled0;
    data->enabled1 = enabled1;
    data->current_pos = current_pos_mm;
    data->retract_pos = (float)(m_retractReferenceSteps - m_machineHomeReferenceSteps) / STEPS_PER_MM;
    // Calculate target position in mm from stored target steps
    float target_pos_mm = (float)(m_active_op_target_position_steps - m_machineHomeReferenceSteps) / STEPS_PER_MM;
    data->target_pos = (m_state == STATE_MOVING || m_state == STATE_HOMING) ? target_pos_mm : 0.0f;
    data->torque_m1 = displayTorque0;
    data->torque_m2 = displayTorque1;
    data->homed = m_homingDone ? 1 : 0;
}

bool MotorController::isBusy() const {
    return m_state != STATE_STANDBY;
}

const char* MotorController::getState() const {
    switch (m_state) {
        case STATE_STANDBY:     return "Standby";
        case STATE_HOMING:      return "Homing";
        case STATE_MOVING:      return "Moving";
        case STATE_MOTOR_FAULT: return "Fault";
        default:                return "Unknown";
    }
}

bool MotorController::isInFault() const {
    return m_motorA->StatusReg().bit.MotorInFault || m_motorB->StatusReg().bit.MotorInFault;
}
