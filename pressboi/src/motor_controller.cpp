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
#include "events.h"
#include "NvmManager.h"
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
    m_joules = 0.0;
    m_prev_position_mm = 0.0;
    m_machineStrainBaselinePosMm = 0.0;
    m_prevMachineDeflectionMm = 0.0;
    m_prevTotalDeflectionMm = 0.0;
    m_machineEnergyJ = 0.0;
    m_machineStrainContactActive = false;
    m_jouleIntegrationActive = false;
    m_forceLimitTriggered = false;
    m_prevForceValid = false;
    
    // Default machine strain compensation coefficients (x^3, x^2, x, constant)
    m_machineStrainCoeffs[0] = MACHINE_STRAIN_COEFF_X4;
    m_machineStrainCoeffs[1] = MACHINE_STRAIN_COEFF_X3;
    m_machineStrainCoeffs[2] = MACHINE_STRAIN_COEFF_X2;
    m_machineStrainCoeffs[3] = MACHINE_STRAIN_COEFF_X1;
    m_machineStrainCoeffs[4] = MACHINE_STRAIN_COEFF_C;
    
    // Default to load_cell mode (will be overwritten by NVM in setup)
    strcpy(m_force_mode, "load_cell");
    
    // Default motor torque calibration: Torque% = 0.0335 * kg + 1.04
    m_motor_torque_scale = 0.0335f;
    m_motor_torque_offset = 1.04f;

    m_retractSpeedMms = RETRACT_DEFAULT_SPEED_MMS;

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
    
    // Check for first boot and initialize NVM with defaults
    NvmManager &nvmMgr = NvmManager::Instance();
    const int32_t NVM_MAGIC_NUMBER = 0x50425231; // "PBR1" = PressBoi Release 1
    int32_t magicValue = nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(7 * 4));  // Byte offset 28
    
    if (magicValue != NVM_MAGIC_NUMBER) {
        // First boot detected - initialize all NVM locations with defaults
        
        // Location 4 (byte 16): Force mode (1 = load_cell default)
        nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(4 * 4), 1);
        
        // Location 5 (byte 20): Motor torque scale (0.0335 × 100000)
        nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(5 * 4), (int32_t)(0.0335f * 100000.0f));
        
        // Location 6 (byte 24): Motor torque offset (1.04 × 10000)
        nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(6 * 4), (int32_t)(1.04f * 10000.0f));
        
        // Locations 8-12 (bytes 32-48): Machine strain compensation coefficients (floats)
        for (int i = 0; i < 5; ++i) {
            int32_t coeffBits;
            memcpy(&coeffBits, &m_machineStrainCoeffs[i], sizeof(float));
            nvmMgr.Int32(static_cast<NvmManager::NvmLocations>((8 + i) * 4), coeffBits);
        }
        
        // Write magic number to indicate NVM is initialized (byte 28)
        nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(7 * 4), NVM_MAGIC_NUMBER);
    }
    
    // Load force mode from NVM (0 = motor_torque, 1 = load_cell)
    int32_t forceModeValue = nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(4 * 4));  // Byte offset 16
    if (forceModeValue == 0) {
        strcpy(m_force_mode, "motor_torque");
    } else {
        strcpy(m_force_mode, "load_cell"); // Default if NVM empty or set to 1
    }
    
    // Load motor torque calibration from NVM
    // NVM locations 5 and 6 store scale and offset as fixed-point (value * 100000 and * 10000)
    int32_t scaleValue = nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(5 * 4));   // Byte offset 20
    int32_t offsetValue = nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(6 * 4));  // Byte offset 24
    
    // Validate scale: should be in reasonable range (0.01 to 0.1) → stored as 1000 to 10000
    if (scaleValue > 0 && scaleValue < 20000 && scaleValue != -1) {
        m_motor_torque_scale = (float)scaleValue / 100000.0f; // Stored as value * 100000
    } else {
        // Invalid NVM - initialize with default
        nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(5 * 4), (int32_t)(0.0335f * 100000.0f));
    }
    
    // Validate offset: should be in reasonable range (-10 to +10) → stored as -100000 to 100000
    if (offsetValue > -100000 && offsetValue < 100000 && offsetValue != 0 && offsetValue != -1) {
        m_motor_torque_offset = (float)offsetValue / 10000.0f; // Stored as value * 10000
    } else {
        // Invalid NVM - initialize with default
        nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(6 * 4), (int32_t)(1.04f * 10000.0f));
    }
    
    // Load machine strain compensation coefficients (locations 8-12, bytes 32-48)
    for (int i = 0; i < 5; ++i) {
        int32_t coeffBits = nvmMgr.Int32(static_cast<NvmManager::NvmLocations>((8 + i) * 4));
        if (coeffBits != 0 && coeffBits != -1) {
            float tempCoeff;
            memcpy(&tempCoeff, &coeffBits, sizeof(float));
            if (tempCoeff > -5e9f && tempCoeff < 5e9f && std::fabs(tempCoeff) < 1e4f) {
                m_machineStrainCoeffs[i] = tempCoeff;
                continue;
            }
        }
        // Invalid or empty - write default value
        float defaultCoeff;
        switch (i) {
            case 0: defaultCoeff = MACHINE_STRAIN_COEFF_X4; break;
            case 1: defaultCoeff = MACHINE_STRAIN_COEFF_X3; break;
            case 2: defaultCoeff = MACHINE_STRAIN_COEFF_X2; break;
            case 3: defaultCoeff = MACHINE_STRAIN_COEFF_X1; break;
            default: defaultCoeff = MACHINE_STRAIN_COEFF_C; break;
        }
        m_machineStrainCoeffs[i] = defaultCoeff;
        int32_t defaultBits;
        memcpy(&defaultBits, &defaultCoeff, sizeof(float));
        nvmMgr.Int32(static_cast<NvmManager::NvmLocations>((8 + i) * 4), defaultBits);
    }
}

float MotorController::evaluateMachineStrainForceFromDeflection(float deflection_mm) const {
    float x = deflection_mm;
    if (x < 0.0f) {
        x = 0.0f;
    }

    float force = (((m_machineStrainCoeffs[0] * x + m_machineStrainCoeffs[1]) * x + m_machineStrainCoeffs[2]) * x + m_machineStrainCoeffs[3]) * x + m_machineStrainCoeffs[4];
    if (force < 0.0f) {
        force = 0.0f;
    }
    return force;
}

float MotorController::estimateMachineDeflectionFromForce(float force_kg) const {
    if (force_kg <= 0.0f) {
        return 0.0f;
    }

    float min_force = evaluateMachineStrainForceFromDeflection(0.0f);
    if (force_kg <= min_force) {
        return 0.0f;
    }

    float low = 0.0f;
    float high = MACHINE_STRAIN_MAX_DEFLECTION_MM;

    // Expand high bound until polynomial exceeds target force or we hit a safety ceiling
    const float MAX_DEFLECTION = MACHINE_STRAIN_MAX_DEFLECTION_MM * 4.0f;
    while (evaluateMachineStrainForceFromDeflection(high) < force_kg && high < MAX_DEFLECTION) {
        high *= 1.5f;
        if (high > MAX_DEFLECTION) {
            high = MAX_DEFLECTION;
            break;
        }
    }

    // Binary search for deflection producing the given force
    for (int i = 0; i < 20; ++i) {
        float mid = 0.5f * (low + high);
        float f_mid = evaluateMachineStrainForceFromDeflection(mid);
        if (f_mid < force_kg) {
            low = mid;
        } else {
            high = mid;
        }
    }

    return high;
}

void MotorController::setMachineStrainCoeffs(float coeff_x4, float coeff_x3, float coeff_x2, float coeff_x1, float coeff_c) {
    m_machineStrainCoeffs[0] = coeff_x4;
    m_machineStrainCoeffs[1] = coeff_x3;
    m_machineStrainCoeffs[2] = coeff_x2;
    m_machineStrainCoeffs[3] = coeff_x1;
    m_machineStrainCoeffs[4] = coeff_c;
    m_prevForceValid = false;
    m_prevTotalDeflectionMm = 0.0;
    m_prevMachineDeflectionMm = 0.0;
    m_machineEnergyJ = 0.0;
    m_machineStrainContactActive = false;
    
    NvmManager &nvmMgr = NvmManager::Instance();
    for (int i = 0; i < 5; ++i) {
        int32_t bits;
        memcpy(&bits, &m_machineStrainCoeffs[i], sizeof(float));
        nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(8 + i), bits);
    }
}

/**
 * @brief The main update loop for the motor controller's state machines.
 */
void MotorController::updateState() {
    // Update joule integration for active moves (50Hz)
    updateJoules();
    
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
                        abortMove();
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
                        abortMove();
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
            // Check limits based on mode
            if (m_moveState == MOVE_ACTIVE) {
                if (strcmp(m_active_op_force_mode, "load_cell") == 0) {
                    // Load cell mode - check force sensor status and force limit
                    const char* errorMsg = nullptr;
                    if (checkForceSensorStatus(&errorMsg)) {
                        abortMove();
                        char fullMsg[STATUS_MESSAGE_BUFFER_SIZE];
                        snprintf(fullMsg, sizeof(fullMsg), "Move stopped: %s", errorMsg);
                        reportEvent(STATUS_PREFIX_ERROR, fullMsg);
                        m_moveState = MOVE_PAUSED;  // Hold the controller
                        return;
                    }
                    
                    // Check force limit (if set)
                    if (m_active_op_force_limit_kg > 0.1f) {
                        float current_force = m_controller->m_forceSensor.getForce();
                        if (current_force >= m_active_op_force_limit_kg) {
                            char limit_desc[STATUS_MESSAGE_BUFFER_SIZE];
                            snprintf(limit_desc, sizeof(limit_desc), "Force limit (%.1f kg, actual: %.1f kg)", 
                                     m_active_op_force_limit_kg, current_force);
                            handleLimitReached(limit_desc, current_force);
                            return;
                        }
                    }
                } else {
                    // Motor torque mode - check torque limit as primary stopping condition
                    if (checkTorqueLimit()) {
                        char limit_desc[STATUS_MESSAGE_BUFFER_SIZE];
                        snprintf(limit_desc, sizeof(limit_desc), "Torque limit (%.1f%%)", m_torqueLimit);
                        handleLimitReached(limit_desc, m_torqueLimit);
                        return;
                    }
                }
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
    
    // Reset joule tracking for new homing operation
    m_joules = 0.0;
    long current_pos_steps = m_motorA->PositionRefCommanded();
    m_prev_position_mm = static_cast<double>(current_pos_steps - m_machineHomeReferenceSteps) / STEPS_PER_MM;
    m_prevForceValid = false;
    m_forceLimitTriggered = false;
    m_jouleIntegrationActive = false; // Do not accumulate joules during homing

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
    float speed_mms = m_retractSpeedMms;
    int parsed = std::sscanf(args, "%f %f", &position_mm, &speed_mms);
    if (parsed < 1) {
        reportEvent(STATUS_PREFIX_ERROR, "Error: Invalid position for SET_RETRACT.");
        return;
    }

    bool speedProvided = (parsed >= 2);
    if (speedProvided) {
        if (speed_mms <= 0.0f) {
            reportEvent(STATUS_PREFIX_ERROR, "Error: Retract speed must be > 0.");
            return;
        }
        if (speed_mms > 100.0f) {
            speed_mms = 100.0f;
            reportEvent(STATUS_PREFIX_INFO, "Retract speed limited to 100 mm/s for safety.");
        }
        m_retractSpeedMms = speed_mms;
    } else if (m_retractSpeedMms <= 0.0f) {
        m_retractSpeedMms = RETRACT_DEFAULT_SPEED_MMS;
    }
    // Ensure stored speed obeys ceiling even if default changed
    if (m_retractSpeedMms > 100.0f) {
        m_retractSpeedMms = 100.0f;
    }
    
    // Store the retract position as an offset from home
    long position_steps = (long)(position_mm * STEPS_PER_MM);
    m_retractReferenceSteps = m_machineHomeReferenceSteps + position_steps;
    
    char msg[128];
    snprintf(msg, sizeof(msg), "Retract position set to %.2f mm (%ld steps from home) at %.2f mm/s", position_mm, position_steps, m_retractSpeedMms);
    reportEvent(STATUS_PREFIX_INFO, msg);
    char dbg[128];
    snprintf(dbg, sizeof(dbg), "Retract debug: home_steps=%ld, retract_steps=%ld", m_machineHomeReferenceSteps, m_retractReferenceSteps);
    reportEvent(STATUS_PREFIX_INFO, dbg);
    
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
        char dbg[128];
        snprintf(dbg, sizeof(dbg), "Retract debug: reference steps still zero (home=%ld)", m_machineHomeReferenceSteps);
        reportEvent(STATUS_PREFIX_INFO, dbg);
        reportEvent(STATUS_PREFIX_ERROR, "Error: Retract position not set. Use SET_RETRACT first.");
        return;
    }
    
    // Parse optional speed parameter
    float speed_mms = (m_retractSpeedMms > 0.0f) ? m_retractSpeedMms : RETRACT_DEFAULT_SPEED_MMS;
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
    long steps_to_retract = m_retractReferenceSteps - current_pos;
    
    int velocity_sps = (int)(speed_mms * STEPS_PER_MM);
    m_torqueLimit = DEFAULT_TORQUE_LIMIT;  // Use default motor torque limit (80%)
    
    // Store move parameters for pause/resume
    m_active_op_initial_axis_steps = current_pos;
    m_active_op_total_target_steps = std::abs(steps_to_retract);
    m_active_op_velocity_sps = velocity_sps;
    m_active_op_accel_sps2 = m_moveDefaultAccelSPS2;
    m_active_op_torque_percent = (int)m_torqueLimit;
    m_moveStartTime = Milliseconds();  // Set start time for timeout tracking
    
    startMove(steps_to_retract, velocity_sps, m_moveDefaultAccelSPS2);
    
    char msg[128];
    snprintf(msg, sizeof(msg), "retract to %.3f mm at %.2f mm/s initiated", 
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
    
    // Only check force sensor if we're in "load_cell" mode
    if (strcmp(m_force_mode, "load_cell") == 0) {
        const char* errorMsg = nullptr;
        if (checkForceSensorStatus(&errorMsg)) {
            char fullMsg[STATUS_MESSAGE_BUFFER_SIZE];
            snprintf(fullMsg, sizeof(fullMsg), "Move aborted: %s", errorMsg);
            reportEvent(STATUS_PREFIX_ERROR, fullMsg);
            return;
        }
        
        // Check if current force already exceeds target (for "hold" action)
        float current_force = m_controller->m_forceSensor.getForce();
        if (strcmp(force_action, "hold") == 0 && current_force >= force_kg) {
            char msg[STATUS_MESSAGE_BUFFER_SIZE];
            snprintf(msg, sizeof(msg), "Force limit (%.2f kg) already reached. Current force: %.2f kg", 
                     force_kg, current_force);
            reportEvent(STATUS_PREFIX_ERROR, msg);  // ERROR message will auto-hold script
            return;
        }
    }
    
    long target_steps = m_machineHomeReferenceSteps + (long)(position_mm * STEPS_PER_MM);
    long current_pos = m_motorA->PositionRefCommanded();
    long steps_to_move = target_steps - current_pos;
    
    int velocity_sps = (int)(speed_mms * STEPS_PER_MM);
    
    // Set torque limit based on mode
    if (force_kg > 0.0f) {
        // Validate kg range based on mode
        if (strcmp(m_force_mode, "motor_torque") == 0) {
            // Motor torque mode: 50-2000 kg range
            if (force_kg < 50.0f) {
                reportEvent(STATUS_PREFIX_ERROR, "Error: Force must be >= 50 kg in motor_torque mode.");
                return;
            }
            if (force_kg > 2000.0f) {
                reportEvent(STATUS_PREFIX_ERROR, "Error: Force must be <= 2000 kg in motor_torque mode.");
                return;
            }
            // Calculate torque limit using calibrated equation: Torque% = scale * kg + offset
            m_torqueLimit = m_motor_torque_scale * force_kg + m_motor_torque_offset;
            char torque_msg[128];
            snprintf(torque_msg, sizeof(torque_msg), "Torque limit set: %.1f%% (from %.0f kg) in %s mode", 
                     m_torqueLimit, force_kg, m_force_mode);
            reportEvent(STATUS_PREFIX_INFO, torque_msg);
        } else {
            // Load cell mode: just validate range and leave torque limit at default ceiling
            if (force_kg < 0.2f) {
                reportEvent(STATUS_PREFIX_ERROR, "Error: Force must be >= 0.2 kg in load_cell mode.");
                return;
            }
            if (force_kg > 1000.0f) {
                reportEvent(STATUS_PREFIX_ERROR, "Error: Force must be <= 1000 kg in load_cell mode.");
                return;
            }
            m_torqueLimit = DEFAULT_TORQUE_LIMIT;
        }
    } else {
        // No kg parameter specified: use default torque limit
        m_torqueLimit = DEFAULT_TORQUE_LIMIT;
    }
    
    // All validations passed - now update state
    fullyResetActiveMove();
    m_state = STATE_MOVING;
    m_moveState = MOVE_STARTING;
    m_activeMoveCommand = "move_abs";
    m_active_op_target_position_steps = target_steps;  // Store for telemetry
    
    // Store move parameters for pause/resume
    m_active_op_initial_axis_steps = current_pos;
    m_active_op_total_target_steps = std::abs(steps_to_move);
    m_active_op_velocity_sps = velocity_sps;
    m_active_op_accel_sps2 = m_moveDefaultAccelSPS2;
    m_active_op_torque_percent = (int)m_torqueLimit;
    m_moveStartTime = Milliseconds();  // Set start time for timeout tracking
    
    // Store force limit, action, and mode for use during move
    m_active_op_force_limit_kg = force_kg;
    strncpy(m_active_op_force_action, force_action, sizeof(m_active_op_force_action) - 1);
    m_active_op_force_action[sizeof(m_active_op_force_action) - 1] = '\0';
    strncpy(m_active_op_force_mode, m_force_mode, sizeof(m_active_op_force_mode) - 1);
    m_active_op_force_mode[sizeof(m_active_op_force_mode) - 1] = '\0';
    
    // Reset joule tracking for new move
    m_joules = 0.0;
    long current_pos_steps = m_motorA->PositionRefCommanded();
    m_prev_position_mm = static_cast<double>(current_pos_steps - m_machineHomeReferenceSteps) / STEPS_PER_MM;
    m_machineStrainBaselinePosMm = m_prev_position_mm;
    m_prevMachineDeflectionMm = 0.0;
    m_prevTotalDeflectionMm = 0.0;
    m_machineEnergyJ = 0.0;
    m_machineStrainContactActive = false;
    m_forceLimitTriggered = false;
    if (strcmp(m_active_op_force_mode, "motor_torque") == 0) {
        m_jouleIntegrationActive = false;
    } else {
        m_jouleIntegrationActive = true;
    }
    
    startMove(steps_to_move, velocity_sps, m_moveDefaultAccelSPS2);
    
    char msg[128];
    snprintf(msg, sizeof(msg), "move_abs to %.2f mm initiated (mode: %s)", position_mm, m_force_mode);
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
    
    // Only check force sensor if we're in "load_cell" mode
    if (strcmp(m_force_mode, "load_cell") == 0) {
        const char* errorMsg = nullptr;
        if (checkForceSensorStatus(&errorMsg)) {
            char fullMsg[STATUS_MESSAGE_BUFFER_SIZE];
            snprintf(fullMsg, sizeof(fullMsg), "Move aborted: %s", errorMsg);
            reportEvent(STATUS_PREFIX_ERROR, fullMsg);
            return;
        }
        
        // Check if current force already exceeds target (for "hold" action)
        float current_force = m_controller->m_forceSensor.getForce();
        if (strcmp(force_action, "hold") == 0 && current_force >= force_kg) {
            char msg[STATUS_MESSAGE_BUFFER_SIZE];
            snprintf(msg, sizeof(msg), "Force limit (%.2f kg) already reached. Current force: %.2f kg", 
                     force_kg, current_force);
            reportEvent(STATUS_PREFIX_ERROR, msg);  // ERROR message will auto-hold script
            return;
        }
    }
    
    long steps_to_move = (long)(distance_mm * STEPS_PER_MM);
    long current_pos = m_motorA->PositionRefCommanded();
    int velocity_sps = (int)(speed_mms * STEPS_PER_MM);
    
    // Set torque limit based on mode
    if (force_kg > 0.0f) {
        // Validate kg range based on mode
        if (strcmp(m_force_mode, "motor_torque") == 0) {
            // Motor torque mode: 50-2000 kg range
            if (force_kg < 50.0f) {
                reportEvent(STATUS_PREFIX_ERROR, "Error: Force must be >= 50 kg in motor_torque mode.");
                return;
            }
            if (force_kg > 2000.0f) {
                reportEvent(STATUS_PREFIX_ERROR, "Error: Force must be <= 2000 kg in motor_torque mode.");
                return;
            }
            // Calculate torque limit using calibrated equation: Torque% = scale * kg + offset
            m_torqueLimit = m_motor_torque_scale * force_kg + m_motor_torque_offset;
            char torque_msg[128];
            snprintf(torque_msg, sizeof(torque_msg), "Torque limit set: %.1f%% (from %.0f kg) in %s mode", 
                     m_torqueLimit, force_kg, m_force_mode);
            reportEvent(STATUS_PREFIX_INFO, torque_msg);
        } else {
            // Load cell mode: 0.2-1000 kg range
            if (force_kg < 0.2f) {
                reportEvent(STATUS_PREFIX_ERROR, "Error: Force must be >= 0.2 kg in load_cell mode.");
                return;
            }
            if (force_kg > 1000.0f) {
                reportEvent(STATUS_PREFIX_ERROR, "Error: Force must be <= 1000 kg in load_cell mode.");
                return;
            }
            m_torqueLimit = DEFAULT_TORQUE_LIMIT;
        }
    } else {
        // No kg parameter specified: use default torque limit
        m_torqueLimit = DEFAULT_TORQUE_LIMIT;
    }
    
    // All validations passed - now update state
    fullyResetActiveMove();
    m_state = STATE_MOVING;
    m_moveState = MOVE_STARTING;
    m_activeMoveCommand = "move_inc";
    m_active_op_target_position_steps = current_pos + steps_to_move;  // Store for telemetry
    
    // Store move parameters for pause/resume
    m_active_op_initial_axis_steps = current_pos;
    m_active_op_total_target_steps = std::abs(steps_to_move);
    m_active_op_velocity_sps = velocity_sps;
    m_active_op_accel_sps2 = m_moveDefaultAccelSPS2;
    m_active_op_torque_percent = (int)m_torqueLimit;
    m_moveStartTime = Milliseconds();  // Set start time for timeout tracking
    
    // Store force limit, action, and mode for use during move
    m_active_op_force_limit_kg = force_kg;
    strncpy(m_active_op_force_action, force_action, sizeof(m_active_op_force_action) - 1);
    m_active_op_force_action[sizeof(m_active_op_force_action) - 1] = '\0';
    strncpy(m_active_op_force_mode, m_force_mode, sizeof(m_active_op_force_mode) - 1);
    m_active_op_force_mode[sizeof(m_active_op_force_mode) - 1] = '\0';
    
    // Reset joule tracking for new move
    m_joules = 0.0;
    long current_pos_steps = m_motorA->PositionRefCommanded();
    m_prev_position_mm = static_cast<double>(current_pos_steps - m_machineHomeReferenceSteps) / STEPS_PER_MM;
    m_machineStrainBaselinePosMm = m_prev_position_mm;
    m_prevMachineDeflectionMm = 0.0;
    m_prevTotalDeflectionMm = 0.0;
    m_machineEnergyJ = 0.0;
    m_machineStrainContactActive = false;
    m_forceLimitTriggered = false;
    if (strcmp(m_active_op_force_mode, "motor_torque") == 0) {
        m_jouleIntegrationActive = false;
    } else {
        m_jouleIntegrationActive = true;
    }
    
    startMove(steps_to_move, velocity_sps, m_moveDefaultAccelSPS2);
    
    char msg[128];
    snprintf(msg, sizeof(msg), "move_inc by %.2f mm initiated (mode: %s)", distance_mm, m_force_mode);
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
                if (!m_forceLimitTriggered && strcmp(m_active_op_force_mode, "load_cell") == 0) {
                    m_jouleIntegrationActive = true;
                } else {
                    m_jouleIntegrationActive = false;
                }
                m_prevForceValid = false;
                m_prev_position_mm = static_cast<double>(current_pos - m_machineHomeReferenceSteps) / STEPS_PER_MM;
                m_machineStrainBaselinePosMm = m_prev_position_mm;
                m_prevMachineDeflectionMm = 0.0;
                m_prevTotalDeflectionMm = 0.0;
                m_machineEnergyJ = 0.0;
                m_machineStrainContactActive = false;
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
 * @brief Sets the force sensing mode and saves to NVM.
 */
bool MotorController::setForceMode(const char* mode) {
    NvmManager &nvmMgr = NvmManager::Instance();
    
    if (strcmp(mode, "motor_torque") == 0) {
        strcpy(m_force_mode, "motor_torque");
        // Save to NVM (0 = motor_torque, byte offset 16)
        nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(4 * 4), 0);
        return true;
    } else if (strcmp(mode, "load_cell") == 0) {
        strcpy(m_force_mode, "load_cell");
        // Save to NVM (1 = load_cell, byte offset 16)
        nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(4 * 4), 1);
        return true;
    }
    return false; // Invalid mode
}

/**
 * @brief Gets the current force sensing mode.
 */
const char* MotorController::getForceMode() const {
    return m_force_mode;
}

/**
 * @brief Sets the calibration offset for the current force mode.
 */
void MotorController::setForceCalibrationOffset(float offset) {
    if (strcmp(m_force_mode, "motor_torque") == 0) {
        // Motor torque offset (intercept of Torque% = scale * kg + offset)
        m_motor_torque_offset = offset;
        // Save to NVM location 6 (byte offset 24, scale by 10000 for fixed-point storage)
        NvmManager &nvmMgr = NvmManager::Instance();
        nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(6 * 4), (int32_t)(offset * 10000.0f));
    } else {
        // Load cell offset - delegate to force sensor
        // (force sensor handles its own NVM storage)
    }
}

/**
 * @brief Sets the calibration scale for the current force mode.
 */
void MotorController::setForceCalibrationScale(float scale) {
    if (strcmp(m_force_mode, "motor_torque") == 0) {
        // Motor torque scale (slope of Torque% = scale * kg + offset)
        m_motor_torque_scale = scale;
        // Save to NVM location 5 (byte offset 20, scale by 100000 for more precision on small values)
        NvmManager &nvmMgr = NvmManager::Instance();
        nvmMgr.Int32(static_cast<NvmManager::NvmLocations>(5 * 4), (int32_t)(scale * 100000.0f));
    } else {
        // Load cell scale - delegate to force sensor
        // (force sensor handles its own NVM storage)
    }
}

/**
 * @brief Gets the calibration offset for the current force mode.
 */
float MotorController::getForceCalibrationOffset() const {
    if (strcmp(m_force_mode, "motor_torque") == 0) {
        return m_motor_torque_offset;
    } else {
        // Return load cell offset (would need to add getter to ForceSensor)
        return 0.0f; // Placeholder
    }
}

/**
 * @brief Gets the calibration scale for the current force mode.
 */
float MotorController::getForceCalibrationScale() const {
    if (strcmp(m_force_mode, "motor_torque") == 0) {
        return m_motor_torque_scale;
    } else {
        // Return load cell scale (would need to add getter to ForceSensor)
        return 1.0f; // Placeholder
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
 * @details During active moves, holds the last non-zero value to prevent telemetry spikes when 
 * HLFB briefly reads at-position during slow moves.
 */
float MotorController::getSmoothedTorque(MotorDriver *motor, float *smoothedValue, bool *firstRead) {
    // If motor not moving AND we're not in an active move state, reset
    if (!motor->StatusReg().bit.StepsActive && m_moveState != MOVE_ACTIVE) {
        *firstRead = true; // Reset for the next move
        return 0.0f;
    }

    float currentRawTorque = motor->HlfbPercent();

    // The driver may return the sentinel value if a reading is not available yet
    // During active moves, hold the last good value instead of returning 0
    if (currentRawTorque == TORQUE_HLFB_AT_POSITION) {
        // If we're in an active move, return last smoothed value (hold)
        if (m_moveState == MOVE_ACTIVE && !*firstRead) {
            return *smoothedValue + m_torqueOffset;
        }
        // Otherwise return 0 (no torque available yet)
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
 * @details Just checks and returns true/false. Does NOT abort move or report.
 * The caller is responsible for handling the limit being reached.
 */
bool MotorController::checkTorqueLimit() {
    if (isMoving()) {
        float torque0 = getSmoothedTorque(m_motorA, &m_smoothedTorqueValue0, &m_firstTorqueReading0);
        float torque1 = getSmoothedTorque(m_motorB, &m_smoothedTorqueValue1, &m_firstTorqueReading1);

        bool m0_over_limit = (torque0 != TORQUE_HLFB_AT_POSITION && std::abs(torque0) > m_torqueLimit);
        bool m1_over_limit = (torque1 != TORQUE_HLFB_AT_POSITION && std::abs(torque1) > m_torqueLimit);

        if (m0_over_limit || m1_over_limit) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Handles the logic when a force or torque limit is reached during a move.
 * @param limit_type Description of what limit was reached (e.g., "Force limit (100 kg)" or "Torque limit (3.9%)")
 * @param limit_value The actual limit value reached
 */
void MotorController::handleLimitReached(const char* limit_type, float limit_value) {
    abortMove();
    m_jouleIntegrationActive = false;
    m_forceLimitTriggered = true;
    m_prevForceValid = false;
    
    char msg[STATUS_MESSAGE_BUFFER_SIZE];
    snprintf(msg, sizeof(msg), "%s reached.", limit_type);
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
            float speed_mms = (m_retractSpeedMms > 0.0f) ? m_retractSpeedMms : RETRACT_DEFAULT_SPEED_MMS;
            if (speed_mms > 100.0f) {
                speed_mms = 100.0f;
            }
            int velocity_sps = static_cast<int>(speed_mms * STEPS_PER_MM);
            m_active_op_velocity_sps = velocity_sps;
            m_active_op_accel_sps2 = m_moveDefaultAccelSPS2;
            startMove(steps_to_retract, velocity_sps, m_moveDefaultAccelSPS2);
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
}

/**
 * @brief Checks force sensor status for errors.
 * @param errorMsg Output parameter for error message (if any)
 * @return true if there's an error, false if force sensor is OK
 */
bool MotorController::checkForceSensorStatus(const char** errorMsg) {
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
    strncpy(m_active_op_force_mode, "motor_torque", sizeof(m_active_op_force_mode) - 1);
    m_active_op_force_mode[sizeof(m_active_op_force_mode) - 1] = '\0';
    m_active_op_total_distance_mm = 0.0f;
    m_last_completed_distance_mm = 0.0f;
    m_active_op_total_target_steps = 0;
    // DON'T reset m_active_op_target_position_steps - retain last target for telemetry
    m_active_op_remaining_steps = 0;
    m_active_op_segment_initial_axis_steps = 0;
    m_active_op_initial_axis_steps = 0;
    m_activeMoveCommand = nullptr;
    m_jouleIntegrationActive = false;
    m_forceLimitTriggered = false;
    m_prevForceValid = false;
    m_machineStrainBaselinePosMm = 0.0;
    m_prevMachineDeflectionMm = 0.0;
    m_prevTotalDeflectionMm = 0.0;
    m_machineEnergyJ = 0.0;
    m_machineStrainContactActive = false;
}

/**
 * @brief Updates joule counter by integrating force × distance.
 * @details Integrates at 50Hz (called every time force sensor updates).
 * Energy (Joules) = Force (N) × Distance (m)
 * Force in kg needs conversion: kg × 9.81 = Newtons
 * Distance in mm needs conversion: mm × 0.001 = meters
 * Therefore: joules += force_kg × distance_mm × 0.00981
 */
void MotorController::updateJoules() {
    if (!m_jouleIntegrationActive || m_state != STATE_MOVING) {
        m_prevForceValid = false;
        return;
    }

    if (strcmp(m_active_op_force_mode, "motor_torque") == 0) {
        m_jouleIntegrationActive = false;
        m_prevForceValid = false;
        return;
    }
    
    long current_pos_steps = m_motorA->PositionRefCommanded();
    double current_pos_mm = static_cast<double>(current_pos_steps - m_machineHomeReferenceSteps) / STEPS_PER_MM;
    double distance_mm = current_pos_mm - m_prev_position_mm;
    double abs_distance_mm = fabs(distance_mm);

    float raw_force_sample = m_controller->m_forceSensor.getForce();
    if (!m_prevForceValid) {
        m_prevForceKg = raw_force_sample;
        if (m_prevForceKg < 0.0f) {
            m_prevForceKg = 0.0f;
        }
        m_prevMachineDeflectionMm = static_cast<double>(estimateMachineDeflectionFromForce(m_prevForceKg));
        m_prevForceValid = true;
        m_prev_position_mm = current_pos_mm;
        return;
    }

    float raw_force_kg = raw_force_sample;
    if (raw_force_kg < 0.0f) {
        raw_force_kg = 0.0f;
    }

    float clamped_force_kg = raw_force_kg;
    if (m_active_op_force_limit_kg > 0.0f && clamped_force_kg > m_active_op_force_limit_kg) {
        clamped_force_kg = m_active_op_force_limit_kg;
    }

    if (abs_distance_mm <= 0.0) {
        m_prev_position_mm = current_pos_mm;
        m_prevForceKg = clamped_force_kg;
        if (m_machineStrainContactActive) {
            m_prevMachineDeflectionMm = static_cast<double>(estimateMachineDeflectionFromForce(clamped_force_kg));
        } else {
            m_prevMachineDeflectionMm = 0.0;
        }
        return;
    }

    if (!m_machineStrainContactActive) {
        if (clamped_force_kg >= MACHINE_STRAIN_CONTACT_FORCE_KG) {
            double contact_machine_def_mm = static_cast<double>(estimateMachineDeflectionFromForce(clamped_force_kg));
            if (contact_machine_def_mm < 0.0) {
                contact_machine_def_mm = 0.0;
            }
            m_machineStrainContactActive = true;
            m_machineStrainBaselinePosMm = current_pos_mm - contact_machine_def_mm;
            m_prevMachineDeflectionMm = contact_machine_def_mm;
            m_prevTotalDeflectionMm = contact_machine_def_mm;
            m_machineEnergyJ = 0.0;
            m_prev_position_mm = current_pos_mm;
            m_prevForceKg = clamped_force_kg;
            return;
        } else {
            m_machineStrainBaselinePosMm = current_pos_mm;
            m_prevMachineDeflectionMm = 0.0;
            m_prevTotalDeflectionMm = 0.0;
            m_machineEnergyJ = 0.0;
            m_prev_position_mm = current_pos_mm;
            m_prevForceKg = clamped_force_kg;
            return;
        }
    }

    double actual_force_avg = 0.5 * static_cast<double>(m_prevForceKg + clamped_force_kg);
    double total_deflection_mm = current_pos_mm - m_machineStrainBaselinePosMm;
    if (total_deflection_mm < 0.0) {
        total_deflection_mm = 0.0;
    }

    double machine_deflection_curr = static_cast<double>(estimateMachineDeflectionFromForce(clamped_force_kg));
    if (machine_deflection_curr > total_deflection_mm) {
        machine_deflection_curr = total_deflection_mm;
    }
    double delta_machine_mm = machine_deflection_curr - m_prevMachineDeflectionMm;
    if (delta_machine_mm < 0.0) {
        delta_machine_mm = 0.0;
    }
    if (delta_machine_mm > abs_distance_mm) {
        delta_machine_mm = abs_distance_mm;
    }

    double cumulative_deflection = machine_deflection_curr;
    double delta_total_deflection = cumulative_deflection - m_prevTotalDeflectionMm;
    if (delta_total_deflection < 0.0) {
        delta_total_deflection = 0.0;
    }
    double machine_increment = actual_force_avg * delta_total_deflection * 0.00981;
    double gross_increment = actual_force_avg * abs_distance_mm * 0.00981;
    double net_increment = gross_increment - machine_increment;
    if (net_increment < 0.0) {
        net_increment = 0.0;
    }
    if (machine_increment < 0.0) {
        machine_increment = 0.0;
    }
    m_joules += net_increment;
    m_machineEnergyJ += machine_increment;

    static int jouleLogCounter = 0;
    if (++jouleLogCounter >= 25) { // roughly every 0.5s at 50Hz
        jouleLogCounter = 0;
        char dbg[128];
        snprintf(dbg, sizeof(dbg),
                 "JDBG force=%.2fkg def=%.3fmm delta_def=%.3fmm travel=%.3fmm dist=%.3fmm eff=%.3fmm gross=%.4fJ machine=%.4fJ dE=%.4fJ total=%.4fJ",
                 actual_force_avg,
                 machine_deflection_curr,
                 delta_machine_mm,
                 total_deflection_mm,
                 abs_distance_mm,
                 abs_distance_mm - delta_machine_mm,
                 gross_increment,
                 machine_increment,
                 net_increment,
                 m_joules);
        reportEvent(STATUS_PREFIX_INFO, dbg);
    }
    
    m_prev_position_mm = current_pos_mm;
    m_prevForceKg = clamped_force_kg;
    m_prevMachineDeflectionMm = machine_deflection_curr;
    m_prevTotalDeflectionMm = cumulative_deflection;

    if (m_active_op_force_limit_kg > 0.0f && raw_force_kg >= m_active_op_force_limit_kg) {
        m_jouleIntegrationActive = false;
        m_forceLimitTriggered = true;
        m_prevForceValid = false;
    }
}

void MotorController::reportEvent(const char* statusType, const char* message) {
    char fullMsg[STATUS_MESSAGE_BUFFER_SIZE];
    snprintf(fullMsg, sizeof(fullMsg), "Motor: %s", message);
    m_controller->reportEvent(statusType, fullMsg);
}

/**
 * @brief Updates the telemetry data structure with current motor state.
 */
void MotorController::updateTelemetry(TelemetryData* data, ForceSensor* forceSensor) {
    if (data == NULL) return;
    
    float displayTorque0 = getSmoothedTorque(m_motorA, &m_smoothedTorqueValue0, &m_firstTorqueReading0);
    float displayTorque1 = getSmoothedTorque(m_motorB, &m_smoothedTorqueValue1, &m_firstTorqueReading1);
    
    long current_pos_steps_m0 = m_motorA->PositionRefCommanded();
    double current_pos_mm = static_cast<double>(current_pos_steps_m0 - m_machineHomeReferenceSteps) / STEPS_PER_MM;

    // Use the m_isEnabled flag for telemetry, not the hardware register
    // The hardware register may lag or not reflect our intended state
    // This ensures telemetry matches what the motor controller thinks its state is
    int enabled0 = m_isEnabled ? 1 : 0;
    int enabled1 = m_isEnabled ? 1 : 0;

    // Always calculate and send BOTH force values for logging
    float avg_torque = (displayTorque0 + displayTorque1) / 2.0f;
    
    // Calculate force from motor torque (always available)
    data->force_motor_torque = (avg_torque - 1.04f) / 0.0335f;
    if (data->force_motor_torque < 0.0f) data->force_motor_torque = 0.0f;
    if (data->force_motor_torque > 1000.0f) data->force_motor_torque = 1000.0f;
    
    // Get force from load cell (if available)
    if (forceSensor && forceSensor->isConnected()) {
        data->force_load_cell = forceSensor->getForce();
        data->force_adc_raw = (int32_t)forceSensor->getRawValue();
    } else {
        data->force_load_cell = 0.0f;
        data->force_adc_raw = 0;
    }
    
    // Set force_source based on persistent force mode setting
    data->force_source = m_force_mode;

    // Update telemetry structure
    // Force limit should be in kg, not torque%
    if (m_state == STATE_MOVING && m_active_op_force_limit_kg > 0.1f) {
        // Active move with commanded kg limit
        data->force_limit = m_active_op_force_limit_kg;
    } else {
        // Not moving - show max based on current force mode
        if (strcmp(m_force_mode, "load_cell") == 0) {
            data->force_limit = 1000.0f;  // Load cell mode max
        } else {
            data->force_limit = 2000.0f;  // Motor torque mode max
        }
    }
    data->enabled0 = enabled0;
    data->enabled1 = enabled1;
    data->current_pos = static_cast<float>(current_pos_mm);
    data->retract_pos = static_cast<float>(static_cast<double>(m_retractReferenceSteps - m_machineHomeReferenceSteps) / STEPS_PER_MM);
    // Calculate target position in mm from stored target steps (always show, don't reset)
    double target_pos_mm = static_cast<double>(m_active_op_target_position_steps - m_machineHomeReferenceSteps) / STEPS_PER_MM;
    data->target_pos = static_cast<float>(target_pos_mm);
    // Calculate average torque of both motors
    data->torque_avg = (displayTorque0 + displayTorque1) / 2.0f;
    data->homed = m_homingDone ? 1 : 0;
    
    // Update joules (energy expended during move)
    data->joules = static_cast<float>(m_joules);
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
