#pragma once

#include <cstdint>
#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// --- System Parameters & Conversions ---
#define INJECTOR_PITCH_MM_PER_REV 5.0f
#define PINCH_PITCH_MM_PER_REV 2.0f
#define PULSES_PER_REV 800
#define STEPS_PER_MM_INJECTOR (PULSES_PER_REV / INJECTOR_PITCH_MM_PER_REV) // 160.0f
#define STEPS_PER_MM_PINCH (PULSES_PER_REV / PINCH_PITCH_MM_PER_REV)     // 400.0f
#define MAX_HOMING_DURATION_MS 30000 // 30 seconds

// --- Queue Configuration ---
#define RX_QUEUE_SIZE 16
#define TX_QUEUE_SIZE 32
#define MAX_MESSAGE_LENGTH 1024 // Increased buffer size

// --- Hardware Pin Definitions (Corrected from user table) ---
#define PIN_THERMOCOUPLE ConnectorA12  // Analog input for thermocouple
#define PIN_HEATER_RELAY ConnectorIO1  // CORRECTED: Digital output for heater relay
#define PIN_VACUUM_RELAY ConnectorIO0  // CORRECTED: Digital output for vacuum relay
#define PIN_VACUUM_TRANSDUCER ConnectorA11
#define PIN_VACUUM_VALVE_RELAY ConnectorIO5

// --- Thermocouple Conversion Coefficients (Corrected for 0-10V Range) ---
#define TC_V_REF 10.0f          // ADC reference voltage is 10V for ClearCore analog inputs.
#define TC_V_OFFSET 1.25f       // The sensor's voltage at 0 degrees C (from user spec).
#define TC_GAIN 200.0f          // Gain (e.g., degrees C per Volt).

// --- NEW: PID Control Parameters ---
#define PID_UPDATE_INTERVAL_MS 100 // How often to run the PID calculation
#define PID_PWM_PERIOD_MS 1000     // Time window for time-proportioned relay control
#define SENSOR_SAMPLE_INTERVAL_MS 100 // 10Hz sampling for temp/vacuum

// --- NEW: Vacuum Transducer Coefficients ---
#define VAC_V_OUT_MIN 1.0f      // Sensor voltage at min pressure
#define VAC_V_OUT_MAX 5.0f      // Sensor voltage at max pressure
#define VAC_PRESSURE_MIN -14.7f // Min pressure in PSIG
#define VAC_PRESSURE_MAX 15.0f  // Max pressure in PSIG
#define VACUUM_PSIG_OFFSET 0.0f

// --- Command Strings & Prefixes ---
#define CMD_STR_REQUEST_TELEM "REQUEST_TELEM"
#define CMD_STR_DISCOVER "DISCOVER_INJECTOR"
#define CMD_STR_SET_PEER_IP "SET_PEER_IP "
#define CMD_STR_CLEAR_PEER_IP "CLEAR_PEER_IP"
#define CMD_STR_ENABLE "ENABLE"
#define CMD_STR_DISABLE "DISABLE"
#define CMD_STR_ABORT "ABORT"
#define CMD_STR_CLEAR_ERRORS "CLEAR_ERRORS"
#define CMD_STR_SET_INJECTOR_TORQUE_OFFSET "SET_INJECTOR_TORQUE_OFFSET "
#define CMD_STR_SET_PINCH_TORQUE_OFFSET "SET_PINCH_TORQUE_OFFSET "
#define CMD_STR_JOG_MOVE "JOG_MOVE "
#define CMD_STR_MACHINE_HOME_MOVE "MACHINE_HOME_MOVE"
#define CMD_STR_CARTRIDGE_HOME_MOVE "CARTRIDGE_HOME_MOVE"
#define CMD_STR_PINCH_HOME_MOVE "PINCH_HOME_MOVE"
#define CMD_STR_INJECT_MOVE "INJECT_MOVE "
#define CMD_STR_PURGE_MOVE "PURGE_MOVE "
#define CMD_STR_PINCH_OPEN "PINCH_OPEN"
#define CMD_STR_PINCH_CLOSE "PINCH_CLOSE"
#define CMD_STR_PINCH_JOG_MOVE "PINCH_JOG_MOVE "
#define CMD_STR_ENABLE_PINCH "ENABLE_PINCH"
#define CMD_STR_DISABLE_PINCH "DISABLE_PINCH"
#define CMD_STR_MOVE_TO_CARTRIDGE_HOME "MOVE_TO_CARTRIDGE_HOME"
#define CMD_STR_MOVE_TO_CARTRIDGE_RETRACT "MOVE_TO_CARTRIDGE_RETRACT "
#define CMD_STR_PAUSE_INJECTION "PAUSE_INJECTION"
#define CMD_STR_RESUME_INJECTION "RESUME_INJECTION"
#define CMD_STR_CANCEL_INJECTION "CANCEL_INJECTION"

// --- Vacuum controls ---
#define CMD_STR_VACUUM_ON "VACUUM_ON"
#define CMD_STR_VACUUM_OFF "VACUUM_OFF"

// --- Environmental controls ---
#define CMD_STR_HEATER_ON "HEATER_ON"
#define CMD_STR_HEATER_OFF "HEATER_OFF"
#define CMD_STR_SET_HEATER_GAINS "SET_HEATER_GAINS "
#define CMD_STR_SET_HEATER_SETPOINT "SET_HEATER_SETPOINT "
#define CMD_STR_HEATER_PID_ON "HEATER_PID_ON"
#define CMD_STR_HEATER_PID_OFF "HEATER_PID_OFF"

#define TELEM_PREFIX_GUI "INJ_TELEM_GUI:"
#define STATUS_PREFIX_INFO "INJ_INFO: "
#define STATUS_PREFIX_START "INJ_START: "
#define STATUS_PREFIX_DONE "INJ_DONE: "
#define STATUS_PREFIX_ERROR "INJ_ERROR: "
#define STATUS_PREFIX_DISCOVERY "DISCOVERY: "

#define EWMA_ALPHA_SENSORS 0.5f // For heavily smoothed temp/vacuum
#define EWMA_ALPHA_TORQUE 0.2f   // For responsive torque readings

#define TORQUE_SENTINEL_INVALID_VALUE -9999.0f
#define MAX_PACKET_LENGTH 1024 // Increased buffer size
#define LOCAL_PORT 8888

// --- State Enums ---
enum MainState : uint8_t { STANDBY_MODE, DISABLED_MODE, MAIN_STATE_COUNT };
enum HomingState : uint8_t { HOMING_NONE, HOMING_MACHINE, HOMING_CARTRIDGE, HOMING_PINCH, HOMING_STATE_COUNT };
enum HomingPhase : uint8_t { HOMING_PHASE_IDLE, HOMING_PHASE_STARTING_MOVE, HOMING_PHASE_RAPID_MOVE, HOMING_PHASE_BACK_OFF, HOMING_PHASE_TOUCH_OFF, HOMING_PHASE_RETRACT, HOMING_PHASE_COMPLETE, HOMING_PHASE_ERROR, HOMING_PHASE_COUNT };
enum FeedState : uint8_t { FEED_NONE, FEED_STANDBY, FEED_INJECT_STARTING, FEED_INJECT_ACTIVE, FEED_INJECT_PAUSED, FEED_INJECT_RESUMING, FEED_PURGE_STARTING, FEED_PURGE_ACTIVE, FEED_PURGE_PAUSED, FEED_PURGE_RESUMING, FEED_MOVING_TO_HOME, FEED_MOVING_TO_RETRACT, FEED_INJECTION_CANCELLED, FEED_INJECTION_COMPLETED, FEED_STATE_COUNT };
enum ErrorState : uint8_t { ERROR_NONE, ERROR_MANUAL_ABORT, ERROR_TORQUE_ABORT, ERROR_MOTION_EXCEEDED_ABORT, ERROR_NO_CARTRIDGE_HOME, ERROR_NO_MACHINE_HOME, ERROR_HOMING_TIMEOUT, ERROR_HOMING_NO_TORQUE_RAPID, ERROR_HOMING_NO_TORQUE_TOUCH, ERROR_INVALID_INJECTION, ERROR_NOT_HOMED, ERROR_INVALID_PARAMETERS, ERROR_MOTORS_DISABLED, ERROR_STATE_COUNT };
enum HeaterState: uint8_t { HEATER_OFF, HEATER_MANUAL_ON, HEATER_PID_ACTIVE, HEATER_STATE_COUNT };

typedef enum {
    CMD_UNKNOWN,
    CMD_ENABLE,
    CMD_DISABLE,
    CMD_ENABLE_PINCH,
    CMD_DISABLE_PINCH,
    CMD_JOG_MOVE,
    CMD_PINCH_JOG_MOVE,
    CMD_MACHINE_HOME_MOVE,
    CMD_CARTRIDGE_HOME_MOVE,
    CMD_PINCH_HOME_MOVE,
    CMD_MOVE_TO_CARTRIDGE_HOME,
    CMD_MOVE_TO_CARTRIDGE_RETRACT,
    CMD_INJECT_MOVE,
    CMD_PURGE_MOVE,
    CMD_PINCH_OPEN,
    CMD_PINCH_CLOSE,
    CMD_PAUSE_INJECTION,
    CMD_RESUME_INJECTION,
    CMD_CANCEL_INJECTION,
    CMD_SET_TORQUE_OFFSET,
    CMD_DISCOVER,
    CMD_REQUEST_TELEM,
    CMD_ABORT,
    CMD_CLEAR_ERRORS,
    CMD_SET_PEER_IP,
    CMD_CLEAR_PEER_IP,
    // --- Vacuum ---
    CMD_VACUUM_ON,
    CMD_VACUUM_OFF,
    // --- Heater ---
    CMD_HEATER_ON,
    CMD_HEATER_OFF,
    CMD_SET_HEATER_GAINS,
    CMD_SET_HEATER_SETPOINT,
    CMD_HEATER_PID_ON,
    CMD_HEATER_PID_OFF
} UserCommand;

// Structure to hold a single message for the queues
struct Message {
    char buffer[MAX_MESSAGE_LENGTH];
    IpAddress remoteIp;
    uint16_t remotePort;
};

class Injector {
    public:
    MainState mainState;
    HomingState homingState;
    HomingPhase currentHomingPhase;
    FeedState feedState;
    ErrorState errorState;
    HeaterState heaterState;

    EthernetUdp udp;
    IpAddress guiIp;
    uint16_t guiPort;
    bool guiDiscovered;
    IpAddress peerIp;
    bool peerDiscovered;

    bool homingMachineDone;
    bool homingCartridgeDone;
    bool homingPinchDone;
    // NEW: Independent flags for each pinch motor's homing status
    bool homingPinch2Done;
    bool homingPinch3Done;
    bool feedingDone;
    bool jogDone;
    uint32_t homingStartTime;
    bool motorsAreEnabled;
    
    bool heaterOn;
    bool vacuumOn;
    float temperatureCelsius;
    float vacuumPressurePsig;
    float smoothedVacuumPsig;
    bool firstVacuumReading;
    float smoothedTemperatureCelsius;
    bool firstTempReading;
    uint32_t lastSensorSampleTime;
    bool vacuumValveOn;
    
    float pid_setpoint;
    float pid_kp, pid_ki, pid_kd;
    float pid_integral;
    float pid_last_error;
    uint32_t pid_last_time;
    float pid_output;

    float homing_torque_percent_param;
    long homing_actual_stroke_steps;
    long homing_actual_retract_steps;
    int homing_actual_rapid_sps;
    int homing_actual_touch_sps;
    int homing_actual_accel_sps2;
    long homingDefaultBackoffSteps;
    
    int feedDefaultTorquePercent;
    long feedDefaultVelocitySPS;
    long feedDefaultAccelSPS2;
    
    // Constructor
    Injector();

    // Core Functions
    void setup();
    void loop();
    void updateState();
    
    void updatePid();
    void resetPid();
    void updateVacuum();

    // Communication
    void sendGuiTelemetry(void);
    void setupSerial();
    void setupEthernet();
    void processUdp();
    void sendStatus(const char* statusType, const char* message);
    UserCommand parseCommand(const char* msg);
    
    // Command Handlers
    void handleMessage(const Message& msg);
    void handleEnable();
    void handleDisable();
    void handleAbort();
    void handleClearErrors();
    void handleStandbyMode();
    void handleSetinjectorMotorsTorqueOffset(const char* msg);
    void handleJogMove(const char* msg);
    void handleMachineHomeMove();
    void handleCartridgeHomeMove();
    void handlePinchHomeMove();
    void handlePinchJogMove(const char* msg);
    void handleEnablePinch();
    void handleDisablePinch();
    void handleMoveToCartridgeHome();
    void handleMoveToCartridgeRetract(const char* msg);
    void handleInjectMove(const char* msg);
    void handlePurgeMove(const char* msg);
    void handlePauseOperation();
    void handleResumeOperation();
    void handleCancelOperation();
    void handleSetPeerIp(const char* msg);
    void handleClearPeerIp();
    void handlePinchOpen();
    void handlePinchClose();
    void handleVacuumOn();
    void handleVacuumOff();
    void handleHeaterOn();
    void handleHeaterOff();
    void handleSetHeaterGains(const char* msg);
    void handleSetHeaterSetpoint(const char* msg);
    void handleHeaterPidOn();
    void handleHeaterPidOff();
    
    //--- Hardware & Peripheral Functions ---//
    void setupMotors(void);
    void setupPeripherals(void);
    void enableInjectorMotors(const char* reason_message);
    void disableInjectorMotors(const char* reason_message);
    void moveInjectorMotors(int stepsM0, int stepsM1, int torque_limit, int velocity, int accel);
    void movePinchMotor(int stepsM2, int torque_limit, int velocity, int accel);
    bool checkInjectorMoving(void);
    float getSmoothedTorqueEWMA(MotorDriver *motor, float *smoothedValue, bool *firstRead);
    bool checkInjectorTorqueLimit(void);
    void abortInjectorMove(void);
    void updateTemperature(void);

    //--- State Trigger Functions ---//
    void onHomingMachineDone(void);
    void onHomingCartridgeDone(void);
    void onHomingPinchDone(void);
    void onFeedingDone(void);
    void onJogDone(void);
    
    //--- Reset Functions ---//
    void resetMotors(void);
    void finalizeAndResetActiveDispenseOperation(bool operationCompletedSuccessfully);
    void fullyResetActiveDispenseOperation(void);
    void resetActiveDispenseOp(void);

    private:
    // --- NEW: Queue Processing ---
    void processRxQueue();
    void processTxQueue();

    // --- NEW: Queue Management ---
    bool enqueueRx(const char* msg, const IpAddress& ip, uint16_t port);
    bool dequeueRx(Message& msg);
    bool enqueueTx(const char* msg, const IpAddress& ip, uint16_t port);
    bool dequeueTx(Message& msg);

    const char* mainStateStr() const;
    const char* homingStateStr() const;
    const char* homingPhaseStr() const;
    const char* feedStateStr() const;
    const char* errorStateStr() const;
    const char* heaterStateStr() const;

    uint32_t lastGuiTelemetryTime;
    uint32_t lastPidUpdateTime;

    char telemetryBuffer[1024]; // Increased buffer size
    unsigned char packetBuffer[MAX_PACKET_LENGTH];

    float injectorMotorsTorqueLimit;
    float injectorMotorsTorqueOffset;
    float smoothedTorqueValue0, smoothedTorqueValue1, smoothedTorqueValue2, smoothedTorqueValue3;
    bool firstTorqueReading0, firstTorqueReading1, firstTorqueReading2, firstTorqueReading3;
    int32_t machineHomeReferenceSteps, cartridgeHomeReferenceSteps, pinchHomeReferenceSteps;
    
    const char* activeFeedCommand;
    const char* activeJogCommand;

    float active_op_target_ml, active_op_total_dispensed_ml, active_op_steps_per_ml, last_completed_dispense_ml;
    long active_op_total_target_steps, active_op_remaining_steps, active_op_segment_initial_axis_steps, active_op_initial_axis_steps;
    bool active_dispense_INJECTION_ongoing;
    int active_op_velocity_sps, active_op_accel_sps2, active_op_torque_percent;

    // --- NEW: Message Queues ---
    Message m_rxQueue[RX_QUEUE_SIZE];
    volatile int m_rxQueueHead;
    volatile int m_rxQueueTail;

    Message m_txQueue[TX_QUEUE_SIZE];
    volatile int m_txQueueHead;
    volatile int m_txQueueTail;

    static const char *MainStateNames[MAIN_STATE_COUNT];
    static const char *HomingStateNames[HOMING_STATE_COUNT];
    static const char *HomingPhaseNames[HOMING_PHASE_COUNT];
    static const char *FeedStateNames[FEED_STATE_COUNT];
    static const char *ErrorStateNames[ERROR_STATE_COUNT];
    static const char *HeaterStateNames[HEATER_STATE_COUNT];
};
