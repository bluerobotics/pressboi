/**
 * @file motor_controller.cpp
 * @author Eldin Miller-Stead
 * @date November 3, 2025
 * @brief Implements the controller for the dual-motor press system.
 *
 * @details This file provides the concrete implementation for the `MotorController` class.
 * It contains the logic for the hierarchical state machines that manage homing,
 * feeding, and jogging operations. It also includes the command handlers, motion
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
    m_feedState = FEED_STANDBY;

    m_homingMachineDone = false;
    m_homingCartridgeDone = false;
    m_homingStartTime = 0;
    m_isEnabled = true;

    // Initialize from config file constants
    m_torqueLimit = DEFAULT_INJECTOR_TORQUE_LIMIT;
    m_torqueOffset = DEFAULT_INJECTOR_TORQUE_OFFSET;
    m_feedDefaultTorquePercent = FEED_DEFAULT_TORQUE_PERCENT;
    m_feedDefaultVelocitySPS = FEED_DEFAULT_VELOCITY_SPS;
    m_feedDefaultAccelSPS2 = FEED_DEFAULT_ACCEL_SPS2;

    m_smoothedTorqueValue0 = 0.0f;
    m_smoothedTorqueValue1 = 0.0f;
    m_firstTorqueReading0 = true;
    m_firstTorqueReading1 = true;
    m_machineHomeReferenceSteps = 0;
    m_cartridgeHomeReferenceSteps = 0;
    m_cumulative_dispensed_ml = 0.0f;
    m_feedStartTime = 0;

    fullyResetActiveDispenseOperation();
    m_activeFeedCommand = nullptr;
    m_activeJogCommand = nullptr;
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
                    m_torqueLimit = INJECTOR_HOMING_SEARCH_TORQUE_PERCENT;
                    long rapid_search_steps = m_homingDistanceSteps;
                    if (m_homingState == HOMING_MACHINE) {
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
                    m_torqueLimit = INJECTOR_HOMING_BACKOFF_TORQUE_PERCENT;
                    long backoff_steps = (m_homingState == HOMING_MACHINE) ? m_homingBackoffSteps : -m_homingBackoffSteps;
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
                    m_torqueLimit = INJECTOR_HOMING_SEARCH_TORQUE_PERCENT;
                    long slow_search_steps = (m_homingState == HOMING_MACHINE) ? -m_homingBackoffSteps * 2 : m_homingBackoffSteps * 2;
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
                    m_torqueLimit = INJECTOR_HOMING_BACKOFF_TORQUE_PERCENT;
                    long offset_steps = (m_homingState == HOMING_MACHINE) ? m_homingBackoffSteps : -m_homingBackoffSteps;
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
                    char doneMsg[STATUS_MESSAGE_BUFFER_SIZE];
                    const char* commandStr = (m_homingState == HOMING_MACHINE) ? "home" : "cartridge_home";
                    
                    if (m_homingState == HOMING_MACHINE) {
                        m_machineHomeReferenceSteps = m_motorA->PositionRefCommanded();
                        m_homingMachineDone = true;
                        } else { // HOMING_CARTRIDGE
                        m_cartridgeHomeReferenceSteps = m_motorA->PositionRefCommanded();
                        m_homingCartridgeDone = true;
                    }
                    
                    // Send standardized DONE message with just the command name
                    reportEvent(STATUS_PREFIX_DONE, commandStr);

                    m_state = STATE_STANDBY;
                    m_homingPhase = HOMING_PHASE_IDLE;
                    break;
                }
                case HOMING_PHASE_ERROR:
                reportEvent(STATUS_PREFIX_ERROR, "Injector homing sequence ended with error.");
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

        case STATE_FEEDING: {
            if (checkTorqueLimit()) {
                reportEvent(STATUS_PREFIX_ERROR,"FEED_MODE: Torque limit! Operation stopped.");
                finalizeAndResetActiveDispenseOperation(false);
                m_state = STATE_STANDBY;
                return;
            }

            if (!isMoving() && m_feedState != FEED_INJECT_PAUSED) {
                bool isStarting = (m_feedState == FEED_INJECT_STARTING);
                uint32_t elapsed = Milliseconds() - m_feedStartTime;
                
                if (!isStarting || (isStarting && elapsed > MOVE_START_TIMEOUT_MS)) {
                    if (m_activeFeedCommand) {
                        // Send standardized DONE message with just the command name
                        reportEvent(STATUS_PREFIX_DONE, m_activeFeedCommand);
                    }
                    finalizeAndResetActiveDispenseOperation(true);
                    m_state = STATE_STANDBY;
                }
            }

            if ((m_feedState == FEED_INJECT_STARTING || m_feedState == FEED_INJECT_RESUMING) && isMoving()) {
                m_feedState = FEED_INJECT_ACTIVE;
                m_active_op_segment_initial_axis_steps = m_motorA->PositionRefCommanded();
            }

            if (m_feedState == FEED_INJECT_ACTIVE) {
                if (m_active_op_steps_per_ml > 0.0001f) {
                    long current_pos = m_motorA->PositionRefCommanded();
                    long steps_moved_since_start = current_pos - m_active_op_initial_axis_steps;
                    m_active_op_total_dispensed_ml = (float)std::abs(steps_moved_since_start) / m_active_op_steps_per_ml;
                }
            }

            if (m_feedState == FEED_INJECT_PAUSED && !isMoving()) {
                if (m_active_op_steps_per_ml > 0.0001f) {
                    // m_active_op_total_dispensed_ml is now updated continuously, so we only need to calculate remaining steps here.
                    long total_steps_dispensed = (long)(m_active_op_total_dispensed_ml * m_active_op_steps_per_ml);
                    m_active_op_remaining_steps = m_active_op_total_target_steps - total_steps_dispensed;
                    if (m_active_op_remaining_steps < 0) m_active_op_remaining_steps = 0;
                }
                reportEvent(STATUS_PREFIX_INFO, "Feed Op: Operation Paused. Waiting for Resume/Cancel.");
            }
            break;
        }

        case STATE_JOGGING: {
            if (checkTorqueLimit()) {
                abortMove();
                reportEvent(STATUS_PREFIX_INFO, "JOG: Torque limit. Move stopped.");
                m_state = STATE_STANDBY;
                if (m_activeJogCommand) m_activeJogCommand = nullptr;
                } else if (!isMoving()) {
                if (m_activeJogCommand) {
                    // Send standardized DONE message with just the command name
                    reportEvent(STATUS_PREFIX_DONE, m_activeJogCommand);
                    m_activeJogCommand = nullptr;
                }
                m_state = STATE_STANDBY;
            }
            break;
        }
    }
}

/**
 * @brief Handles a command specifically for the injector system.
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
    (cmd == CMD_HOME || cmd == CMD_MOVE_ABS || cmd == CMD_MOVE_INC || cmd == CMD_MOVE_TO_START)) {
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
        case CMD_SET_START_POS:
            setStartPosition(args);
            break;
        case CMD_MOVE_TO_START:
            moveToStart();
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
    m_feedState = FEED_STANDBY;
    fullyResetActiveDispenseOperation();
}

/**
 * @brief Handles the HOME command.
 */
void MotorController::home() {
    m_homingDistanceSteps = (long)(fabs(INJECTOR_HOMING_STROKE_MM) * STEPS_PER_MM_INJECTOR);
    m_homingBackoffSteps = (long)(INJECTOR_HOMING_BACKOFF_MM * STEPS_PER_MM_INJECTOR);
    m_homingRapidSps = (int)fabs(INJECTOR_HOMING_RAPID_VEL_MMS * STEPS_PER_MM_INJECTOR);
    m_homingBackoffSps = (int)fabs(INJECTOR_HOMING_BACKOFF_VEL_MMS * STEPS_PER_MM_INJECTOR);
    m_homingTouchSps = (int)fabs(INJECTOR_HOMING_TOUCH_VEL_MMS * STEPS_PER_MM_INJECTOR);
    m_homingAccelSps2 = (int)fabs(INJECTOR_HOMING_ACCEL_MMSS * STEPS_PER_MM_INJECTOR);
    
    char logMsg[200];
    snprintf(logMsg, sizeof(logMsg), "Homing params: dist_steps=%ld, rapid_sps=%d, touch_sps=%d, accel_sps2=%d",
             m_homingDistanceSteps, m_homingRapidSps, m_homingTouchSps, m_homingAccelSps2);
    reportEvent(STATUS_PREFIX_INFO, logMsg);

    if (m_homingDistanceSteps == 0) {
        reportEvent(STATUS_PREFIX_ERROR, "Homing failed: Calculated distance is zero. Check config.");
        return;
    }

    m_state = STATE_HOMING;
    m_homingState = HOMING_MACHINE;
    m_homingPhase = RAPID_SEARCH_START;
    m_homingStartTime = Milliseconds();
    m_homingMachineDone = false;

    reportEvent(STATUS_PREFIX_START, "MACHINE_HOME_MOVE initiated.");
}

/**
 * @brief Handles the SET_START_POS command.
 */
void MotorController::setStartPosition(const char* args) {
    if (!m_homingMachineDone) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Must home before setting start position.");
        return;
    }
    
    float position_mm = 0.0f;
    if (std::sscanf(args, "%f", &position_mm) != 1) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Invalid position for SET_START_POS.");
        return;
    }
    
    // Store the start position as an offset from machine home
    long position_steps = (long)(position_mm * STEPS_PER_MM_INJECTOR);
    m_cartridgeHomeReferenceSteps = m_machineHomeReferenceSteps + position_steps;
    
    char msg[128];
    snprintf(msg, sizeof(msg), "Start position set to %.2f mm (%ld steps from home)", position_mm, position_steps);
    reportEvent(STATUS_PREFIX_INFO, msg);
    
    // Send standardized DONE message
    reportEvent(STATUS_PREFIX_DONE, "set_start_pos");
}

/**
 * @brief Handles the MOVE_TO_START command.
 */
void MotorController::moveToStart() {
    if (!m_homingMachineDone) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Must home before moving to start position.");
        return;
    }
    
    if (m_cartridgeHomeReferenceSteps == 0) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Start position not set. Use SET_START_POS first.");
        return;
    }
    
    fullyResetActiveDispenseOperation();
    m_state = STATE_FEEDING;
    m_feedState = FEED_MOVING_TO_HOME;
    m_activeFeedCommand = "move_to_start";
    
    long current_pos = m_motorA->PositionRefCommanded();
    long steps_to_move = m_cartridgeHomeReferenceSteps - current_pos;
    
    m_torqueLimit = (float)m_feedDefaultTorquePercent;
    startMove(steps_to_move, m_feedDefaultVelocitySPS, m_feedDefaultAccelSPS2);
    
    reportEvent(STATUS_PREFIX_START, "move_to_start initiated.");
}

/**
 * @brief Handles the MOVE_ABS command - move to absolute position.
 */
void MotorController::moveAbsolute(const char* args) {
    if (!m_homingMachineDone) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Must home before absolute moves.");
        return;
    }
    
    float position_mm = 0.0f;
    float speed_mms = FEED_DEFAULT_VELOCITY_MMS;
    float force_kg = m_torqueLimit;  // Use current torque limit as default
    
    // Parse: position, speed, force, [force_action]
    int parsed = std::sscanf(args, "%f %f %f", &position_mm, &speed_mms, &force_kg);
    if (parsed < 1) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Invalid parameters for MOVE_ABS. Need at least position.");
        return;
    }
    
    fullyResetActiveDispenseOperation();
    m_state = STATE_FEEDING;
    m_feedState = FEED_INJECT_STARTING;
    m_activeFeedCommand = "move_abs";
    
    long target_steps = m_machineHomeReferenceSteps + (long)(position_mm * STEPS_PER_MM_INJECTOR);
    long current_pos = m_motorA->PositionRefCommanded();
    long steps_to_move = target_steps - current_pos;
    
    int velocity_sps = (int)(speed_mms * STEPS_PER_MM_INJECTOR);
    m_torqueLimit = force_kg;  // Use force as torque limit
    
    startMove(steps_to_move, velocity_sps, m_feedDefaultAccelSPS2);
    
    char msg[128];
    snprintf(msg, sizeof(msg), "move_abs to %.2f mm initiated", position_mm);
    reportEvent(STATUS_PREFIX_START, msg);
}

/**
 * @brief Handles the MOVE_INC command - move by incremental distance.
 */
void MotorController::moveIncremental(const char* args) {
    if (!m_homingMachineDone) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Must home before incremental moves.");
        return;
    }
    
    float distance_mm = 0.0f;
    float speed_mms = FEED_DEFAULT_VELOCITY_MMS;
    float force_kg = m_torqueLimit;  // Use current torque limit as default
    
    // Parse: distance, speed, force, [force_action]
    int parsed = std::sscanf(args, "%f %f %f", &distance_mm, &speed_mms, &force_kg);
    if (parsed < 1) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Invalid parameters for MOVE_INC. Need at least distance.");
        return;
    }
    
    fullyResetActiveDispenseOperation();
    m_state = STATE_FEEDING;
    m_feedState = FEED_INJECT_STARTING;
    m_activeFeedCommand = "move_inc";
    
    long steps_to_move = (long)(distance_mm * STEPS_PER_MM_INJECTOR);
    int velocity_sps = (int)(speed_mms * STEPS_PER_MM_INJECTOR);
    m_torqueLimit = force_kg;  // Use force as torque limit
    
    startMove(steps_to_move, velocity_sps, m_feedDefaultAccelSPS2);
    
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
    } else if (m_state == STATE_FEEDING) {
        if (m_feedState == FEED_INJECT_ACTIVE || m_feedState == FEED_INJECT_STARTING || 
            m_feedState == FEED_MOVING_TO_HOME || m_feedState == FEED_MOVING_TO_RETRACT) {
            abortMove();
            m_feedState = FEED_INJECT_PAUSED;
            reportEvent(STATUS_PREFIX_INFO, "Move paused. Send resume to continue.");
        } else {
            reportEvent(STATUS_PREFIX_INFO, "No active move to pause.");
        }
    } else {
        reportEvent(STATUS_PREFIX_INFO, "No active operation to pause.");
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
        // The homing state machine will automatically continue from its current phase
        // No need to restart - it will pick up in the next updateState() call
    } else if (m_state == STATE_FEEDING) {
        if (m_feedState == FEED_INJECT_PAUSED) {
            // For moves with target tracking (injection moves)
            if (m_active_op_steps_per_ml > 0.0001f) {
                if (m_active_op_remaining_steps > 0) {
                    m_active_op_segment_initial_axis_steps = m_motorA->PositionRefCommanded();
                    m_feedState = FEED_INJECT_RESUMING;
                    m_torqueLimit = (float)m_active_op_torque_percent;
                    startMove(m_active_op_remaining_steps, m_active_op_velocity_sps, m_active_op_accel_sps2);
                    reportEvent(STATUS_PREFIX_INFO, "Move resumed.");
                } else {
                    reportEvent(STATUS_PREFIX_INFO, "Move already complete.");
                    fullyResetActiveDispenseOperation();
                    m_state = STATE_STANDBY;
                }
            } else {
                // For simple position moves (move_abs, move_inc, move_to_start)
                // Just transition back to active state - move is already done
                reportEvent(STATUS_PREFIX_INFO, "Move already stopped. Returning to standby.");
                fullyResetActiveDispenseOperation();
                m_state = STATE_STANDBY;
            }
        } else {
            reportEvent(STATUS_PREFIX_INFO, "No paused move to resume.");
        }
    } else {
        reportEvent(STATUS_PREFIX_INFO, "No paused operation to resume.");
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
    } else if (m_state == STATE_FEEDING) {
        abortMove();
        finalizeAndResetActiveDispenseOperation(false);
        m_state = STATE_STANDBY;
        reportEvent(STATUS_PREFIX_INFO, "Move cancelled. Returning to standby.");
    } else {
        reportEvent(STATUS_PREFIX_INFO, "No active operation to cancel.");
    }
}

/**
 * @brief Commands a synchronized move on both injector motors.
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
 * @brief Checks if either of the injector motors are currently active.
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
 * @brief Finalizes a dispense operation, calculating the total dispensed volume.
 */
void MotorController::finalizeAndResetActiveDispenseOperation(bool success) {
    if (success) {
        // With the new polling logic in updateState, m_active_op_total_dispensed_ml should be up-to-date.
        m_last_completed_dispense_ml = m_active_op_total_dispensed_ml;
        m_cumulative_dispensed_ml += m_active_op_total_dispensed_ml;
    }
    fullyResetActiveDispenseOperation();
}

/**
 * @brief Resets all variables related to an active dispense operation.
 */
void MotorController::fullyResetActiveDispenseOperation() {
    m_active_op_target_ml = 0.0f;
    m_active_op_total_dispensed_ml = 0.0f;
    m_last_completed_dispense_ml = 0.0f;
    m_active_op_total_target_steps = 0;
    m_active_op_remaining_steps = 0;
    m_active_op_segment_initial_axis_steps = 0;
    m_active_op_initial_axis_steps = 0;
    m_active_op_steps_per_ml = 0.0f;
    m_activeFeedCommand = nullptr;
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
    float current_pos_mm = (float)(current_pos_steps_m0 - m_machineHomeReferenceSteps) / STEPS_PER_MM_INJECTOR;

    int enabled0 = m_motorA->StatusReg().bit.Enabled;
    int enabled1 = m_motorB->StatusReg().bit.Enabled;

    // Update telemetry structure
    data->force = 0.0f;  // TODO: Calculate from torque if needed
    data->force_limit = m_torqueLimit;
    data->enabled0 = enabled0;
    data->enabled1 = enabled1;
    data->current_pos = current_pos_mm;
    data->start_pos = (float)(m_cartridgeHomeReferenceSteps - m_machineHomeReferenceSteps) / STEPS_PER_MM_INJECTOR;
    data->target_pos = 0.0f;  // TODO: Set to actual target if in move
    data->torque_m1 = displayTorque0;
    data->torque_m2 = displayTorque1;
    data->homed = m_homingMachineDone ? 1 : 0;
}

bool MotorController::isBusy() const {
    return m_state != STATE_STANDBY;
}

const char* MotorController::getState() const {
    switch (m_state) {
        case STATE_STANDBY:     return "Standby";
        case STATE_HOMING:      return "Homing";
        case STATE_JOGGING:     return "Jogging";
        case STATE_FEEDING:     return "Feeding";
        case STATE_MOTOR_FAULT: return "Fault";
        default:                return "Unknown";
    }
}

bool MotorController::isInFault() const {
    return m_motorA->StatusReg().bit.MotorInFault || m_motorB->StatusReg().bit.MotorInFault;
}
