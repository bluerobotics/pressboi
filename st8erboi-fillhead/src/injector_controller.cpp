/**
 * @file injector_controller.cpp
 * @author Eldin Miller-Stead
 * @date September 10, 2025
 * @brief Implements the controller for the dual-motor injector system.
 *
 * @details This file provides the concrete implementation for the `Injector` class.
 * It contains the logic for the hierarchical state machines that manage homing,
 * feeding, and jogging operations. It also includes the command handlers, motion
 * control logic, and telemetry reporting for the injector system.
 */

//==================================================================================================
// --- Includes ---
//==================================================================================================
#include "injector_controller.h"
#include "fillhead.h" // Include full header for Fillhead
#include <cmath>
#include <cstdio>
#include <cstdlib>

//==================================================================================================
// --- Class Implementation ---
//==================================================================================================

/**
 * @brief Constructs the Injector controller.
 */
Injector::Injector(MotorDriver* motorA, MotorDriver* motorB, Fillhead* controller) {
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
 * @brief Performs one-time setup and configuration of the injector motors.
 */
void Injector::setup() {
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
 * @brief The main update loop for the injector's state machines.
 */
void Injector::updateState() {
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
                    const char* commandStr = (m_homingState == HOMING_MACHINE) ? CMD_STR_MACHINE_HOME_MOVE : CMD_STR_CARTRIDGE_HOME_MOVE;
                    
                    if (m_homingState == HOMING_MACHINE) {
                        m_machineHomeReferenceSteps = m_motorA->PositionRefCommanded();
                        m_homingMachineDone = true;
                        } else { // HOMING_CARTRIDGE
                        m_cartridgeHomeReferenceSteps = m_motorA->PositionRefCommanded();
                        m_homingCartridgeDone = true;
                    }
                    
                    snprintf(doneMsg, sizeof(doneMsg), "%s complete.", commandStr);
                    reportEvent(STATUS_PREFIX_DONE, doneMsg);

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
                        char doneMsg[STATUS_MESSAGE_BUFFER_SIZE];
                        std::snprintf(doneMsg, sizeof(doneMsg), "%s complete.", m_activeFeedCommand);
                        reportEvent(STATUS_PREFIX_DONE, doneMsg);
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
                    char doneMsg[STATUS_MESSAGE_BUFFER_SIZE];
                    std::snprintf(doneMsg, sizeof(doneMsg), "%s complete.", m_activeJogCommand);
                    reportEvent(STATUS_PREFIX_DONE, doneMsg);
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
void Injector::handleCommand(Command cmd, const char* args) {
    if (!m_isEnabled) {
        reportEvent(STATUS_PREFIX_ERROR, "Injector command ignored: Motors are disabled.");
        return;
    }
	
	if (m_motorA->StatusReg().bit.MotorInFault || m_motorB->StatusReg().bit.MotorInFault) {
        char errorMsg[200];
        snprintf(errorMsg, sizeof(errorMsg), "Injector command ignored: Motor in fault. M0 Status=0x%04X, M1 Status=0x%04X",
                 (unsigned int)m_motorA->StatusReg().reg, (unsigned int)m_motorB->StatusReg().reg);
        reportEvent(STATUS_PREFIX_ERROR, errorMsg);
        return;
    }
	
    if (m_state != STATE_STANDBY &&
    (cmd == CMD_JOG_MOVE || cmd == CMD_MACHINE_HOME_MOVE || cmd == CMD_CARTRIDGE_HOME_MOVE || cmd == CMD_INJECT_STATOR || cmd == CMD_INJECT_ROTOR)) {
        reportEvent(STATUS_PREFIX_ERROR, "Injector command ignored: Another operation is in progress.");
        return;
    }

    switch(cmd) {
        case CMD_JOG_MOVE:                  jogMove(args); break;
        case CMD_MACHINE_HOME_MOVE:         machineHome(); break;
        case CMD_CARTRIDGE_HOME_MOVE:       cartridgeHome(); break;
        case CMD_MOVE_TO_CARTRIDGE_HOME:    moveToCartridgeHome(); break;
        case CMD_MOVE_TO_CARTRIDGE_RETRACT: moveToCartridgeRetract(args); break;
        case CMD_INJECT_STATOR:
            initiateInjectMove(args, STATOR_PISTON_A_DIAMETER_MM, STATOR_PISTON_B_DIAMETER_MM, CMD_STR_INJECT_STATOR);
            break;
        case CMD_INJECT_ROTOR:
            initiateInjectMove(args, ROTOR_PISTON_A_DIAMETER_MM, ROTOR_PISTON_B_DIAMETER_MM, CMD_STR_INJECT_ROTOR);
            break;
        case CMD_PAUSE_INJECTION:           pauseOperation(); break;
        case CMD_RESUME_INJECTION:          resumeOperation(); break;
        case CMD_CANCEL_INJECTION:          cancelOperation(); break;
        default:
        break;
    }
}

/**
 * @brief Enables both injector motors.
 */
void Injector::enable() {
    m_motorA->EnableRequest(true);
    m_motorB->EnableRequest(true);

    // Always set motor parameters on enable to ensure a known good state after a fault,
    // as the ClearCore driver may reset them to zero.
    m_motorA->VelMax(MOTOR_DEFAULT_VEL_MAX_SPS);
    m_motorA->AccelMax(MOTOR_DEFAULT_ACCEL_MAX_SPS2);
    m_motorB->VelMax(MOTOR_DEFAULT_VEL_MAX_SPS);
    m_motorB->AccelMax(MOTOR_DEFAULT_ACCEL_MAX_SPS2);
    
    m_isEnabled = true;
    reportEvent(STATUS_PREFIX_INFO, "Injector motors enabled.");
}

/**
 * @brief Disables both injector motors.
 */
void Injector::disable() {
    m_motorA->EnableRequest(false);
    m_motorB->EnableRequest(false);
    m_isEnabled = false;
    reportEvent(STATUS_PREFIX_INFO, "Injector motors disabled.");
}

/**
 * @brief Decelerates any ongoing motion to a stop and resets the state machines.
 */
void Injector::abortMove() {
    m_motorA->MoveStopDecel();
    m_motorB->MoveStopDecel();
    Delay_ms(POST_ABORT_DELAY_MS);
}

/**
 * @brief Resets all state machines to their idle state.
 */
void Injector::reset() {
    m_state = STATE_STANDBY;
    m_homingState = HOMING_NONE;
    m_homingPhase = HOMING_PHASE_IDLE;
    m_feedState = FEED_STANDBY;
    fullyResetActiveDispenseOperation();
}

/**
 * @brief Handles the JOG_MOVE command.
 */
void Injector::jogMove(const char* args) {
    float dist_mm1 = 0, dist_mm2 = 0, vel_mms = 0, accel_mms2 = 0;
    int torque_percent = 0;

    int parsed_count = std::sscanf(args, "%f %f %f %f %d", &dist_mm1, &dist_mm2, &vel_mms, &accel_mms2, &torque_percent);

    if (parsed_count == 5) {
        if (torque_percent <= 0 || torque_percent > 100) torque_percent = JOG_DEFAULT_TORQUE_PERCENT;
        if (vel_mms <= 0) vel_mms = JOG_DEFAULT_VEL_MMS;
        if (accel_mms2 <= 0) accel_mms2 = JOG_DEFAULT_ACCEL_MMSS;

        long steps1 = (long)(dist_mm1 * STEPS_PER_MM_INJECTOR);
        int velocity_sps = (int)(vel_mms * STEPS_PER_MM_INJECTOR);
        int accel_sps2_val = (int)(accel_mms2 * STEPS_PER_MM_INJECTOR);
        
        m_activeJogCommand = CMD_STR_JOG_MOVE;
        m_state = STATE_JOGGING;
        m_torqueLimit = (float)torque_percent;
        startMove(steps1, velocity_sps, accel_sps2_val);
        } else {
        char errorMsg[STATUS_MESSAGE_BUFFER_SIZE];
        std::snprintf(errorMsg, sizeof(errorMsg), "Invalid JOG_MOVE format. Expected 5 params, got %d.", parsed_count);
        reportEvent(STATUS_PREFIX_ERROR, errorMsg);
    }
}

/**
 * @brief Handles the MACHINE_HOME_MOVE command.
 */
void Injector::machineHome() {
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
 * @brief Handles the CARTRIDGE_HOME_MOVE command.
 */
void Injector::cartridgeHome() {
    m_cumulative_dispensed_ml = 0.0f; // Reset cumulative dispensed volume on cartridge home
    m_homingDistanceSteps = (long)(fabs(INJECTOR_HOMING_STROKE_MM) * STEPS_PER_MM_INJECTOR);
    m_homingBackoffSteps = (long)(INJECTOR_HOMING_BACKOFF_MM * STEPS_PER_MM_INJECTOR);
    m_homingRapidSps = (int)fabs(INJECTOR_HOMING_RAPID_VEL_MMS * STEPS_PER_MM_INJECTOR);
    m_homingBackoffSps = (int)fabs(INJECTOR_HOMING_BACKOFF_VEL_MMS * STEPS_PER_MM_INJECTOR);
    m_homingTouchSps = (int)fabs(INJECTOR_HOMING_TOUCH_VEL_MMS * STEPS_PER_MM_INJECTOR);
    m_homingAccelSps2 = (int)fabs(INJECTOR_HOMING_ACCEL_MMSS * STEPS_PER_MM_INJECTOR);
    
    m_state = STATE_HOMING;
    m_homingState = HOMING_CARTRIDGE;
    m_homingPhase = RAPID_SEARCH_START;
    m_homingStartTime = Milliseconds();
    m_homingCartridgeDone = false;

    reportEvent(STATUS_PREFIX_START, "CARTRIDGE_HOME_MOVE initiated.");
}

/**
 * @brief Handles the MOVE_TO_CARTRIDGE_HOME command.
 */
void Injector::moveToCartridgeHome() {
    if (!m_homingCartridgeDone) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Cartridge not homed.");
        return;
    }
    
    fullyResetActiveDispenseOperation();
    m_state = STATE_FEEDING;
    m_feedState = FEED_MOVING_TO_HOME;
    m_activeFeedCommand = CMD_STR_MOVE_TO_CARTRIDGE_HOME;
    
    long current_pos = m_motorA->PositionRefCommanded();
    long steps_to_move = m_cartridgeHomeReferenceSteps - current_pos;
    
    m_torqueLimit = (float)m_feedDefaultTorquePercent;
    startMove(steps_to_move, m_feedDefaultVelocitySPS, m_feedDefaultAccelSPS2);
}

/**
 * @brief Handles the MOVE_TO_CARTRIDGE_RETRACT command.
 */
void Injector::moveToCartridgeRetract(const char* args) {
    if (!m_homingCartridgeDone) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Cartridge not homed.");
        return;
    }
    
    float offset_mm = 0.0f;
    if (std::sscanf(args, "%f", &offset_mm) != 1 || offset_mm < 0) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Invalid offset for MOVE_TO_CARTRIDGE_RETRACT.");
        return;
    }
    
    fullyResetActiveDispenseOperation();
    m_state = STATE_FEEDING;
    m_feedState = FEED_MOVING_TO_RETRACT;
    m_activeFeedCommand = CMD_STR_MOVE_TO_CARTRIDGE_RETRACT;

    long offset_steps = (long)(offset_mm * STEPS_PER_MM_INJECTOR);
    long target_pos = m_cartridgeHomeReferenceSteps - offset_steps;
    long current_pos = m_motorA->PositionRefCommanded();
    long steps_to_move = target_pos - current_pos;

    m_torqueLimit = (float)m_feedDefaultTorquePercent;
    startMove(steps_to_move, m_feedDefaultVelocitySPS, m_feedDefaultAccelSPS2);
}

/**
 * @brief Initiates an injection move with dynamically calculated parameters.
 * @param args The command arguments, containing volume and optional overrides.
 * @param piston_a_diam The diameter of the 'A' side piston in mm.
 * @param piston_b_diam The diameter of the 'B' side piston in mm.
 * @param command_str The string name of the command being executed, for logging.
 */
void Injector::initiateInjectMove(const char* args, float piston_a_diam, float piston_b_diam, const char* command_str) {
    float volume_ml = 0.0f;
    // Optional speed parameter, with firmware-defined default
    float speed_ml_s = INJECT_DEFAULT_SPEED_MLS;

    // We no longer need accel and torque as optional params from the GUI,
    // as they are not exposed in the simplified command.
    float accel_sps2 = (float)m_feedDefaultAccelSPS2;
    int torque_percent = m_feedDefaultTorquePercent;

    int parsed_count = std::sscanf(args, "%f %f", &volume_ml, &speed_ml_s);

    if (parsed_count >= 1) { // Only volume is required
        // --- Calculate steps/ml based on piston geometry ---
        float radius_a = piston_a_diam / 2.0f;
        float radius_b = piston_b_diam / 2.0f;
        float area_a = M_PI * radius_a * radius_a;
        float area_b = M_PI * radius_b * radius_b;
        float total_area_mm2 = area_a + area_b;
        float ml_per_mm = total_area_mm2 / 1000.0f; // 1ml = 1000mm^3
        float steps_per_ml = STEPS_PER_MM_INJECTOR / ml_per_mm;

        // Basic validation
        if (torque_percent <= 0 || torque_percent > 100) torque_percent = m_feedDefaultTorquePercent;
        if (volume_ml <= 0) { reportEvent(STATUS_PREFIX_ERROR, "Error: Inject volume must be positive."); return; }
        // If speed is not provided or invalid, use the default.
        if (speed_ml_s <= 0) speed_ml_s = INJECT_DEFAULT_SPEED_MLS; 
        
        // Setup and execute the move
        fullyResetActiveDispenseOperation();
        m_state = STATE_FEEDING;
        m_feedState = FEED_INJECT_STARTING;
        m_active_op_target_ml = volume_ml;
        m_active_op_steps_per_ml = steps_per_ml;
        m_active_op_total_target_steps = (long)(volume_ml * steps_per_ml);
        m_active_op_remaining_steps = m_active_op_total_target_steps;
        m_active_op_initial_axis_steps = m_motorA->PositionRefCommanded();
        m_active_op_velocity_sps = (int)(speed_ml_s * steps_per_ml);
        m_active_op_accel_sps2 = (int)accel_sps2;
        m_active_op_torque_percent = torque_percent;
        m_activeFeedCommand = command_str;
        m_feedStartTime = Milliseconds();

        char start_msg[128];
        snprintf(start_msg, sizeof(start_msg), "%s initiated. (steps/ml: %.2f)", command_str, steps_per_ml);
        reportEvent(STATUS_PREFIX_START, start_msg);

        m_torqueLimit = (float)m_active_op_torque_percent;
        startMove(m_active_op_remaining_steps, m_active_op_velocity_sps, m_active_op_accel_sps2);
    } else {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Invalid %s format. At least 1 parameter (volume) is required.", command_str);
        reportEvent(STATUS_PREFIX_ERROR, error_msg);
    }
}


/**
 * @brief Handles the PAUSE_INJECTION command.
 */
void Injector::pauseOperation() {
    if (m_state != STATE_FEEDING || m_feedState != FEED_INJECT_ACTIVE) {
        reportEvent(STATUS_PREFIX_INFO, "PAUSE ignored: No active injection to pause.");
        return;
    }
    abortMove();
    m_feedState = FEED_INJECT_PAUSED;
    reportEvent(STATUS_PREFIX_DONE, "PAUSE_INJECTION complete.");
}

/**
 * @brief Handles the RESUME_INJECTION command.
 */
void Injector::resumeOperation() {
    if (m_state != STATE_FEEDING || m_feedState != FEED_INJECT_PAUSED) {
        reportEvent(STATUS_PREFIX_INFO, "RESUME ignored: No operation was paused.");
        return;
    }
    if (m_active_op_remaining_steps <= 0) {
        reportEvent(STATUS_PREFIX_INFO, "RESUME ignored: No remaining volume to dispense.");
        fullyResetActiveDispenseOperation();
        m_state = STATE_STANDBY;
        return;
    }
    m_active_op_segment_initial_axis_steps = m_motorA->PositionRefCommanded();
    m_feedState = FEED_INJECT_RESUMING;
    m_torqueLimit = (float)m_active_op_torque_percent;
    startMove(m_active_op_remaining_steps, m_active_op_velocity_sps, m_active_op_accel_sps2);
    reportEvent(STATUS_PREFIX_DONE, "RESUME_INJECTION complete.");
}

/**
 * @brief Handles the CANCEL_INJECTION command.
 */
void Injector::cancelOperation() {
    if (m_state != STATE_FEEDING) {
        reportEvent(STATUS_PREFIX_INFO, "CANCEL ignored: No active operation to cancel.");
        return;
    }
    abortMove();
    finalizeAndResetActiveDispenseOperation(false);
    m_state = STATE_STANDBY;
    reportEvent(STATUS_PREFIX_DONE, "CANCEL_INJECTION complete.");
}

/**
 * @brief Commands a synchronized move on both injector motors.
 */
void Injector::startMove(long steps, int velSps, int accelSps2) {
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
bool Injector::isMoving() {
    if (!m_isEnabled) return false;
    bool m0_moving = m_motorA->StatusReg().bit.StepsActive;
    bool m1_moving = m_motorB->StatusReg().bit.StepsActive;
    return (m0_moving || m1_moving);
}

/**
 * @brief Gets a smoothed torque value from a motor using an EWMA filter.
 */
float Injector::getSmoothedTorque(MotorDriver *motor, float *smoothedValue, bool *firstRead) {
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
bool Injector::checkTorqueLimit() {
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
void Injector::finalizeAndResetActiveDispenseOperation(bool success) {
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
void Injector::fullyResetActiveDispenseOperation() {
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

void Injector::reportEvent(const char* statusType, const char* message) {
    char fullMsg[STATUS_MESSAGE_BUFFER_SIZE];
    snprintf(fullMsg, sizeof(fullMsg), "Injector: %s", message);
    m_controller->reportEvent(statusType, fullMsg);
}

/**
 * @brief Assembles the injector-specific portion of the telemetry string.
 */
const char* Injector::getTelemetryString() {
    float displayTorque0 = getSmoothedTorque(m_motorA, &m_smoothedTorqueValue0, &m_firstTorqueReading0);
    float displayTorque1 = getSmoothedTorque(m_motorB, &m_smoothedTorqueValue1, &m_firstTorqueReading1);
    
    long current_pos_steps_m0 = m_motorA->PositionRefCommanded();
    float machine_pos_mm = (float)(current_pos_steps_m0 - m_machineHomeReferenceSteps) / STEPS_PER_MM_INJECTOR;
    float cartridge_pos_mm = (float)(current_pos_steps_m0 - m_cartridgeHomeReferenceSteps) / STEPS_PER_MM_INJECTOR;

    int enabled0 = m_motorA->StatusReg().bit.Enabled;
    int enabled1 = m_motorB->StatusReg().bit.Enabled;

    float live_cumulative_ml = m_cumulative_dispensed_ml + m_active_op_total_dispensed_ml;

    std::snprintf(m_telemetryBuffer, sizeof(m_telemetryBuffer),
    "inj_t0:%.1f,inj_t1:%.1f,"
    "inj_h_mach:%d,inj_h_cart:%d,"
    "inj_mach_mm:%.2f,inj_cart_mm:%.2f,"
    "inj_cumulative_ml:%.2f,inj_active_ml:%.2f,inj_tgt_ml:%.2f,"
    "enabled0:%d,enabled1:%d",
    displayTorque0, displayTorque1,
    (int)m_homingMachineDone, (int)m_homingCartridgeDone,
    machine_pos_mm, cartridge_pos_mm,
    live_cumulative_ml, m_active_op_total_dispensed_ml, m_active_op_target_ml,
    enabled0, enabled1
    );
    return m_telemetryBuffer;
}

bool Injector::isBusy() const {
    return m_state != STATE_STANDBY;
}

const char* Injector::getState() const {
    switch (m_state) {
        case STATE_STANDBY:     return "Standby";
        case STATE_HOMING:      return "Homing";
        case STATE_JOGGING:     return "Jogging";
        case STATE_FEEDING:     return "Feeding";
        case STATE_MOTOR_FAULT: return "Fault";
        default:                return "Unknown";
    }
}

bool Injector::isInFault() const {
    return m_motorA->StatusReg().bit.MotorInFault || m_motorB->StatusReg().bit.MotorInFault;
}
