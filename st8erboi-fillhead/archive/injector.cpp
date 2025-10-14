#include "injector.h"

// String representations for enums
const char *Injector::MainStateNames[MAIN_STATE_COUNT] = { "STANDBY_MODE", "DISABLED_MODE" };
const char *Injector::HomingStateNames[HOMING_STATE_COUNT] = { "HOMING_NONE", "HOMING_MACHINE", "HOMING_CARTRIDGE", "HOMING_PINCH" };
const char *Injector::HomingPhaseNames[HOMING_PHASE_COUNT] = { "IDLE", "STARTING_MOVE", "RAPID_MOVE", "BACK_OFF", "TOUCH_OFF", "RETRACT", "COMPLETE", "ERROR" };
const char *Injector::FeedStateNames[FEED_STATE_COUNT] = { "FEED_NONE", "FEED_STANDBY", "FEED_INJECT_STARTING", "FEED_INJECT_ACTIVE", "FEED_INJECT_PAUSED", "FEED_INJECT_RESUMING", "FEED_PURGE_STARTING", "FEED_PURGE_ACTIVE", "FEED_PURGE_PAUSED", "FEED_PURGE_RESUMING", "FEED_MOVING_TO_HOME", "FEED_MOVING_TO_RETRACT", "FEED_INJECTION_CANCELLED", "FEED_INJECTION_COMPLETED" };
const char *Injector::ErrorStateNames[ERROR_STATE_COUNT] = { "ERROR_NONE", "ERROR_MANUAL_ABORT", "ERROR_TORQUE_ABORT", "ERROR_MOTION_EXCEEDED_ABORT", "ERROR_NO_CARTRIDGE_HOME", "ERROR_NO_MACHINE_HOME", "ERROR_HOMING_TIMEOUT", "ERROR_HOMING_NO_TORQUE_RAPID", "ERROR_HOMING_NO_TORQUE_TOUCH", "ERROR_INVALID_INJECTION", "ERROR_NOT_HOMED", "ERROR_INVALID_PARAMETERS", "ERROR_MOTORS_DISABLED" };
const char *Injector::HeaterStateNames[HEATER_STATE_COUNT] = { "OFF", "MANUAL", "PID" };

// Constructor
Injector::Injector() {
	mainState = STANDBY_MODE;
	homingState = HOMING_NONE;
	currentHomingPhase = HOMING_PHASE_IDLE;
	feedState = FEED_STANDBY;
	errorState = ERROR_NONE;
	homingMachineDone = false;
	homingCartridgeDone = false;
	homingPinchDone = false;
	homingPinch2Done = false;
	homingPinch3Done = false;
	feedingDone = true;
	jogDone = true;
	homingStartTime = 0;
	
	peerIp = IpAddress();
	peerDiscovered = false;
	guiDiscovered = false;
	guiPort = 0;
	
	lastGuiTelemetryTime = 0;
	
	injectorMotorsTorqueLimit   = 20.0f;
	injectorMotorsTorqueOffset = -2.4f;
	smoothedTorqueValue0 = 0.0f;
	smoothedTorqueValue1 = 0.0f;
	smoothedTorqueValue2 = 0.0f;
	smoothedTorqueValue3 = 0.0f;
	firstTorqueReading0 = true;
	firstTorqueReading1 = true;
	firstTorqueReading2 = true;
	firstTorqueReading3 = true;
	motorsAreEnabled = false;

	heaterOn = false;
	vacuumOn = false;
	temperatureCelsius = 0.0f;
	vacuumPressurePsig = 0.0f;
	smoothedVacuumPsig = 0.0f;
	firstVacuumReading = true;
	smoothedTemperatureCelsius = 0.0f;
	firstTempReading = true;
	lastSensorSampleTime = 0;

	machineHomeReferenceSteps = 0;
	cartridgeHomeReferenceSteps = 0;
	pinchHomeReferenceSteps = 0;
	homingDefaultBackoffSteps = 200;
	
	feedDefaultTorquePercent = 30;
	feedDefaultVelocitySPS = 1000;
	feedDefaultAccelSPS2 = 10000;
	
	vacuumValveOn = false;
	
	pid_setpoint = 70.0f;
	pid_kp = 60;
	pid_ki = 2.5;
	pid_kd = 40;
	resetPid();

	fullyResetActiveDispenseOperation();
	
	activeFeedCommand = nullptr;
	activeJogCommand = nullptr;

	// Initialize queue pointers
	m_rxQueueHead = 0;
	m_rxQueueTail = 0;
	m_txQueueHead = 0;
	m_txQueueTail = 0;
}

void Injector::setup() {
	setupSerial();
	setupEthernet();
	setupMotors();
	setupPeripherals();
}

void Injector::setupPeripherals(void) {
	PIN_THERMOCOUPLE.Mode(Connector::INPUT_ANALOG);
	PIN_HEATER_RELAY.Mode(Connector::OUTPUT_DIGITAL);
	PIN_VACUUM_RELAY.Mode(Connector::OUTPUT_DIGITAL);
	PIN_VACUUM_TRANSDUCER.Mode(Connector::INPUT_ANALOG);
	PIN_VACUUM_VALVE_RELAY.Mode(Connector::OUTPUT_DIGITAL);
	
	PIN_HEATER_RELAY.State(false);
	PIN_VACUUM_RELAY.State(false);
	PIN_VACUUM_VALVE_RELAY.State(false);
}

void Injector::updateTemperature(void) {
	uint16_t adc_val = PIN_THERMOCOUPLE.State();
	float voltage_from_sensor = (float)adc_val * (TC_V_REF / 4095.0f);
	
	float raw_celsius = (voltage_from_sensor - TC_V_OFFSET) * TC_GAIN;

	if (firstTempReading) {
		smoothedTemperatureCelsius = raw_celsius;
		firstTempReading = false;
		} else {
		smoothedTemperatureCelsius = (EWMA_ALPHA_SENSORS * raw_celsius) + ((1.0f - EWMA_ALPHA_SENSORS) * smoothedTemperatureCelsius);
	}

	temperatureCelsius = smoothedTemperatureCelsius;
}

void Injector::updatePid() {
	if (heaterState != HEATER_PID_ACTIVE) {
		if (pid_output != 0.0f) {
			pid_output = 0.0f;
		}
		return;
	}

	uint32_t now = Milliseconds();
	float time_change_ms = (float)(now - pid_last_time);

	if (time_change_ms < 10.0f) {
		return;
	}

	float error = pid_setpoint - temperatureCelsius;
	pid_integral += error * (time_change_ms / 1000.0f);
	float derivative = ((error - pid_last_error) * 1000.0f) / time_change_ms;
	
	pid_output = (pid_kp * error) + (pid_ki * pid_integral) + (pid_kd * derivative);

	if (pid_output > 100.0f) pid_output = 100.0f;
	if (pid_output < 0.0f) pid_output = 0.0f;
	if (pid_integral > 100.0f / pid_ki) pid_integral = 100.0f / pid_ki;
	if (pid_integral < 0.0f) pid_integral = 0.0f;

	pid_last_error = error;
	pid_last_time = now;

	uint32_t on_duration_ms = (uint32_t)(PID_PWM_PERIOD_MS * (pid_output / 100.0f));
	PIN_HEATER_RELAY.State((now % PID_PWM_PERIOD_MS) < on_duration_ms);
}

void Injector::updateVacuum(void) {
	uint16_t adc_val = PIN_VACUUM_TRANSDUCER.State();
	float measured_voltage = (float)adc_val * (TC_V_REF / 4095.0f);

	float voltage_span = VAC_V_OUT_MAX - VAC_V_OUT_MIN;
	float pressure_span = VAC_PRESSURE_MAX - VAC_PRESSURE_MIN;
	float voltage_percent = (measured_voltage - VAC_V_OUT_MIN) / voltage_span;

	float raw_psig = (voltage_percent * pressure_span) + VAC_PRESSURE_MIN;

	if (firstVacuumReading) {
		smoothedVacuumPsig = raw_psig;
		firstVacuumReading = false;
		} else {
		smoothedVacuumPsig = (EWMA_ALPHA_SENSORS * raw_psig) + ((1.0f - EWMA_ALPHA_SENSORS) * smoothedVacuumPsig);
	}

	vacuumPressurePsig = smoothedVacuumPsig + VACUUM_PSIG_OFFSET;

	if (vacuumPressurePsig < VAC_PRESSURE_MIN) vacuumPressurePsig = VAC_PRESSURE_MIN;
	if (vacuumPressurePsig > VAC_PRESSURE_MAX) vacuumPressurePsig = VAC_PRESSURE_MAX;
}



void Injector::onHomingMachineDone(){
	homingMachineDone = true;
}

void Injector::onHomingCartridgeDone(){
	homingCartridgeDone = true;
}

void Injector::onHomingPinchDone(){
	homingPinchDone = true;
}

void Injector::onFeedingDone(){
	feedingDone = true;
}

void Injector::onJogDone(){
	jogDone = true;
}

void Injector::resetPid() {
	pid_integral = 0.0f;
	pid_last_error = 0.0f;
	pid_output = 0.0f;
	pid_last_time = Milliseconds();
}

void Injector::updateState() {
	if (mainState == DISABLED_MODE) {
		return;
	}

	// --- Homing State Logic ---
	if (homingState != HOMING_NONE) {
		if (currentHomingPhase == HOMING_PHASE_COMPLETE || currentHomingPhase == HOMING_PHASE_ERROR) {
			if (currentHomingPhase == HOMING_PHASE_COMPLETE) {
				char doneMsg[64];
				const char* commandStr = "UNKNOWN_HOME";
				if (homingState == HOMING_MACHINE) commandStr = CMD_STR_MACHINE_HOME_MOVE;
				else if (homingState == HOMING_CARTRIDGE) commandStr = CMD_STR_CARTRIDGE_HOME_MOVE;
				else if (homingState == HOMING_PINCH) commandStr = CMD_STR_PINCH_HOME_MOVE;
				snprintf(doneMsg, sizeof(doneMsg), "%s complete.", commandStr);
				sendStatus(STATUS_PREFIX_DONE, doneMsg);
				} else {
				sendStatus(STATUS_PREFIX_ERROR,"Homing sequence ended with error.");
			}
			homingState = HOMING_NONE;
			currentHomingPhase = HOMING_PHASE_IDLE;
			} else {
			uint32_t current_time = Milliseconds();
			if (current_time - homingStartTime > MAX_HOMING_DURATION_MS) {
				sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Timeout. Aborting.");
				abortInjectorMove();
				errorState = ERROR_HOMING_TIMEOUT;
				currentHomingPhase = HOMING_PHASE_ERROR;
			}
		}

		// REFACTORED: State machine for independent pinch homing
		if (homingState == HOMING_PINCH) {
			// Check motor M2 (Pinch 1)
			if (!homingPinch2Done) {
				float torque2 = getSmoothedTorqueEWMA(&ConnectorM2, &smoothedTorqueValue2, &firstTorqueReading2);
				bool stalled_on_torque = (torque2 != TORQUE_SENTINEL_INVALID_VALUE && torque2 > injectorMotorsTorqueLimit);
				bool finished_move = !ConnectorM2.StatusReg().bit.StepsActive;

				if (stalled_on_torque || finished_move) {
					ConnectorM2.MoveStopAbrupt();
					ConnectorM2.PositionRefSet(0);
					homingPinch2Done = true;
					sendStatus(STATUS_PREFIX_INFO, "Pinch motor 2 (M2) has homed.");
				}
			}

			// Check motor M3 (Pinch 2)
			if (!homingPinch3Done) {
				float torque3 = getSmoothedTorqueEWMA(&ConnectorM3, &smoothedTorqueValue3, &firstTorqueReading3);
				bool stalled_on_torque = (torque3 != TORQUE_SENTINEL_INVALID_VALUE && torque3 > injectorMotorsTorqueLimit);
				bool finished_move = !ConnectorM3.StatusReg().bit.StepsActive;

				if (stalled_on_torque || finished_move) {
					ConnectorM3.MoveStopAbrupt();
					ConnectorM3.PositionRefSet(0);
					homingPinch3Done = true;
					sendStatus(STATUS_PREFIX_INFO, "Pinch motor 3 (M3) has homed.");
				}
			}

			// If both are done, complete the overall homing operation
			if (homingPinch2Done && homingPinch3Done) {
				onHomingPinchDone();
				errorState = ERROR_NONE;
				currentHomingPhase = HOMING_PHASE_COMPLETE;
			}
		}
		// State machine for the original multi-phase homing (Machine and Cartridge)
		else if (homingState == HOMING_MACHINE || homingState == HOMING_CARTRIDGE) {
			int direction = (homingState == HOMING_MACHINE) ? -1 : 1;

			switch (currentHomingPhase) {
				case HOMING_PHASE_STARTING_MOVE: {
					uint32_t current_time = Milliseconds();
					if (current_time - homingStartTime > 50) {
						if (checkInjectorMoving()) {
							sendStatus(STATUS_PREFIX_START, "Homing: Move started. Entering RAPID_MOVE phase.");
							currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
							} else {
							sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Motor failed to start moving.");
							errorState = ERROR_HOMING_NO_TORQUE_RAPID;
							currentHomingPhase = HOMING_PHASE_ERROR;
						}
					}
					break;
				}
				case HOMING_PHASE_RAPID_MOVE:
				case HOMING_PHASE_TOUCH_OFF: {
					if (checkInjectorTorqueLimit()) {
						if (currentHomingPhase == HOMING_PHASE_RAPID_MOVE) {
							sendStatus(STATUS_PREFIX_INFO, "Homing: Torque (RAPID). Backing off.");
							currentHomingPhase = HOMING_PHASE_BACK_OFF;
							long back_off_s = -direction * homingDefaultBackoffSteps;
							moveInjectorMotors(back_off_s, back_off_s, (int)homing_torque_percent_param, homing_actual_touch_sps, homing_actual_accel_sps2);
							} else { // TOUCH_OFF
							sendStatus(STATUS_PREFIX_INFO, "Homing: Torque (TOUCH_OFF). Zeroing & Retracting.");
							if (homingState == HOMING_MACHINE) {
								machineHomeReferenceSteps = ConnectorM0.PositionRefCommanded();
								sendStatus(STATUS_PREFIX_INFO, "Machine home reference point set.");
								} else if (homingState == HOMING_CARTRIDGE) {
								cartridgeHomeReferenceSteps = ConnectorM0.PositionRefCommanded();
								sendStatus(STATUS_PREFIX_INFO, "Cartridge home reference point set.");
							}
							currentHomingPhase = HOMING_PHASE_RETRACT;
							long retract_s = -direction * homing_actual_retract_steps;
							moveInjectorMotors(retract_s, retract_s, (int)homing_torque_percent_param, homing_actual_rapid_sps, homing_actual_accel_sps2);
						}
						} else if (!checkInjectorMoving()) {
						const char* msg = currentHomingPhase == HOMING_PHASE_RAPID_MOVE ? "Homing Err: No torque in RAPID." : "Homing Err: No torque in TOUCH_OFF.";
						sendStatus(STATUS_PREFIX_ERROR, msg);
						errorState = currentHomingPhase == HOMING_PHASE_RAPID_MOVE ? ERROR_HOMING_NO_TORQUE_RAPID : ERROR_HOMING_NO_TORQUE_TOUCH;
						currentHomingPhase = HOMING_PHASE_ERROR;
					}
					break;
				}
				case HOMING_PHASE_BACK_OFF: {
					if (!checkInjectorMoving()) {
						sendStatus(STATUS_PREFIX_INFO, "Homing: Back-off done. Touching off.");
						currentHomingPhase = HOMING_PHASE_TOUCH_OFF;
						long touch_off_move_length = homingDefaultBackoffSteps * 2;
						long final_touch_off_move_steps = direction * touch_off_move_length;
						moveInjectorMotors(final_touch_off_move_steps, final_touch_off_move_steps, (int)homing_torque_percent_param, homing_actual_touch_sps, homing_actual_accel_sps2);
					}
					break;
				}
				case HOMING_PHASE_RETRACT: {
					if (!checkInjectorMoving()) {
						sendStatus(STATUS_PREFIX_INFO, "Homing: RETRACT complete. Success.");
						if (homingState == HOMING_MACHINE) onHomingMachineDone();
						else if (homingState == HOMING_CARTRIDGE) onHomingCartridgeDone();
						errorState = ERROR_NONE;
						currentHomingPhase = HOMING_PHASE_COMPLETE;
					}
					break;
				}
				default: break;
			}
		}
	}

	// --- Feed State Logic ---
	if (active_dispense_INJECTION_ongoing) {
		if (feedState == FEED_INJECT_ACTIVE || feedState == FEED_PURGE_ACTIVE ||
		feedState == FEED_MOVING_TO_HOME || feedState == FEED_MOVING_TO_RETRACT ||
		feedState == FEED_INJECT_RESUMING || feedState == FEED_PURGE_RESUMING) {
			if (checkInjectorTorqueLimit()) {
				errorState = ERROR_TORQUE_ABORT;
				sendStatus(STATUS_PREFIX_ERROR,"FEED_MODE: Torque limit! Operation stopped.");
				finalizeAndResetActiveDispenseOperation(false);
				feedState = FEED_STANDBY;
				feedingDone = true;
			}
		}

		if (!checkInjectorMoving()) {
			if (!feedingDone) {
				if (feedState == FEED_INJECT_ACTIVE || feedState == FEED_PURGE_ACTIVE) {
					if (active_op_steps_per_ml > 0.0001f) {
						long steps_moved_this_segment = ConnectorM0.PositionRefCommanded() - active_op_segment_initial_axis_steps;
						float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / active_op_steps_per_ml;
						active_op_total_dispensed_ml += segment_dispensed_ml;
					}
					last_completed_dispense_ml = active_op_total_dispensed_ml;
					feedState = FEED_INJECTION_COMPLETED;
					} else if (feedState == FEED_MOVING_TO_HOME || feedState == FEED_MOVING_TO_RETRACT) {
					feedState = FEED_INJECTION_COMPLETED;
				}
				
				if (feedState == FEED_INJECTION_COMPLETED) {
					if (activeFeedCommand != nullptr) {
						char doneMsg[64];
						snprintf(doneMsg, sizeof(doneMsg), "%s complete.", activeFeedCommand);
						sendStatus(STATUS_PREFIX_DONE, doneMsg);
					}
					feedingDone = true;
					onFeedingDone();
					finalizeAndResetActiveDispenseOperation(true);
					feedState = FEED_STANDBY;
				}
			}
			} else {
			if (feedState == FEED_INJECT_STARTING) {
				feedState = FEED_INJECT_ACTIVE;
				active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
				sendStatus(STATUS_PREFIX_INFO, "Feed Op: Inject Active.");
				} else if (feedState == FEED_PURGE_STARTING) {
				feedState = FEED_PURGE_ACTIVE;
				active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
				sendStatus(STATUS_PREFIX_INFO, "Feed Op: Purge Active.");
				} else if (feedState == FEED_INJECT_RESUMING) {
				feedState = FEED_INJECT_ACTIVE;
				active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
				sendStatus(STATUS_PREFIX_INFO, "Feed Op: Inject Resumed, Active.");
				} else if (feedState == FEED_PURGE_RESUMING) {
				feedState = FEED_PURGE_ACTIVE;
				active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
				sendStatus(STATUS_PREFIX_INFO, "Feed Op: Purge Resumed, Active.");
			}
		}

		if ((feedState == FEED_INJECT_PAUSED || feedState == FEED_PURGE_PAUSED) &&
		!feedingDone && !checkInjectorMoving()) {
			long current_pos = ConnectorM0.PositionRefCommanded();
			long steps_moved_this_segment = current_pos - active_op_segment_initial_axis_steps;
			if (active_op_steps_per_ml > 0.0001f) {
				float segment_dispensed_ml = fabs(steps_moved_this_segment) / active_op_steps_per_ml;
				active_op_total_dispensed_ml += segment_dispensed_ml;
				long total_steps_dispensed = (long)(active_op_total_dispensed_ml * active_op_steps_per_ml);
				active_op_remaining_steps = active_op_total_target_steps - total_steps_dispensed;
				if (active_op_remaining_steps < 0) active_op_remaining_steps = 0;
			}
			feedingDone = true;
			sendStatus(STATUS_PREFIX_INFO, "Feed Op: Operation Paused. Waiting for Resume/Cancel.");
		}
	}

	// --- Jog State Logic ---
	if (!jogDone) {
		if (checkInjectorTorqueLimit()) {
			abortInjectorMove();
			errorState = ERROR_TORQUE_ABORT;
			sendStatus(STATUS_PREFIX_INFO, "JOG: Torque limit, returning to STANDBY.");
			onJogDone();
			if (activeJogCommand != nullptr) {
				activeJogCommand = nullptr;
			}
			} else if (!checkInjectorMoving()) {
			onJogDone();
			if (activeJogCommand != nullptr) {
				char doneMsg[64];
				snprintf(doneMsg, sizeof(doneMsg), "%s complete.", activeJogCommand);
				sendStatus(STATUS_PREFIX_DONE, doneMsg);
				activeJogCommand = nullptr;
			}
		}
	}
}

void Injector::loop() {
	// 1. Read incoming UDP packets and place them in the Rx queue
	processUdp();

	// 2. Process ONE message from the Rx queue
	processRxQueue();

	// 3. Send ONE message from the Tx queue
	processTxQueue();

	// 4. Update state machine
	updateState();

	// 5. Handle periodic tasks
	uint32_t now = Milliseconds();
	if (now - lastPidUpdateTime >= PID_UPDATE_INTERVAL_MS) {
		lastPidUpdateTime = now;
		updatePid();
	}

	if (now - lastGuiTelemetryTime >= 100 && guiDiscovered) {
		lastGuiTelemetryTime = now;
		sendGuiTelemetry();
	}

	if (now - lastSensorSampleTime >= SENSOR_SAMPLE_INTERVAL_MS) {
		lastSensorSampleTime = now;
		updateTemperature();
		updateVacuum();
	}
}

Injector injector;

int main(void) {
	injector.setup();

	while (true) {
		injector.loop();
	}
}


// Implementations for string converters
const char* Injector::mainStateStr() const { return MainStateNames[mainState]; }
const char* Injector::homingStateStr() const { return HomingStateNames[homingState]; }
const char* Injector::homingPhaseStr() const { return HomingPhaseNames[currentHomingPhase]; }
const char* Injector::feedStateStr() const { return FeedStateNames[feedState]; }
const char* Injector::errorStateStr() const { return ErrorStateNames[errorState]; }
const char* Injector::heaterStateStr() const { return HeaterStateNames[heaterState]; }
