/**
 * @file config.h
 * @author Eldin Miller-Stead
 * @date November 3, 2025
 * @brief Central configuration file for the Pressboi press firmware.
 *
 * @details This file consolidates all compile-time constants, hardware pin definitions,
 * and default operational parameters for the entire Pressboi system. By centralizing these
 * values, it simplifies tuning and maintenance, and ensures consistency across all
 * controller modules. The parameters are organized into logical sections for clarity.
 */
#pragma once
#include "ClearCore.h"

//==================================================================================================
// Device Identity
//==================================================================================================
/**
 * @name Device Identity
 * @{
 */
#define DEVICE_NAME_UPPER               "PRESSBOI"  ///< Device name in uppercase for message prefixes
#define DEVICE_NAME_LOWER               "pressboi"  ///< Device name in lowercase for identifiers
/** @} */

//==================================================================================================
// Network Configuration
//==================================================================================================
/**
 * @name Network and Communication Settings
 * @{
 */
#define LOCAL_PORT                      8888      ///< The UDP port this device listens on for incoming commands.
#define CLIENT_PORT                     6272      ///< The UDP port the GUI client listens on.
#define MAX_PACKET_LENGTH               1024      ///< Maximum size in bytes for a single UDP packet. Must be large enough for the longest telemetry string.
#define RX_QUEUE_SIZE                   32        ///< Number of incoming messages that can be buffered before processing.
#define TX_QUEUE_SIZE                   32        ///< Number of outgoing messages that can be buffered before sending.
#define MAX_MESSAGE_LENGTH              MAX_PACKET_LENGTH ///< Maximum size of a single message in the Rx/Tx queues.
#define TELEMETRY_INTERVAL_MS			100       ///< How often (in milliseconds) telemetry data is published to the GUI.
/** @} */

//==================================================================================================
// System Behavior
//==================================================================================================
/**
 * @name General System Behavior
 * @{
 */
#define FIRMWARE_VERSION                "1.11.1"   ///< Pressboi firmware version
#define STATUS_MESSAGE_BUFFER_SIZE      256       ///< Standard buffer size for composing status and error messages.
/** @} */

//==================================================================================================
// System Parameters & Conversions
//==================================================================================================
/**
 * @name Core System Parameters and Unit Conversions
 * @{
 */
#define PITCH_MM_PER_REV                5.0f      ///< Linear travel (in mm) of the press for one full motor revolution.
#define PULSES_PER_REV                  800       ///< Number of step pulses required for one full motor revolution (dependent on microstepping settings).
#define STEPS_PER_MM                    (PULSES_PER_REV / PITCH_MM_PER_REV)   ///< Calculated steps per millimeter for the press drive.
#define MAX_HOMING_DURATION_MS          100000    ///< Maximum time (in milliseconds) a homing operation is allowed to run before timing out.
/** @} */

//==================================================================================================
// Hardware Pin Definitions
//==================================================================================================
/**
 * @name Hardware Pin and Connector Assignments
 * @{
 */
// --- Motors ---
#define MOTOR_A                         ConnectorM0 ///< Primary press motor.
#define MOTOR_B                         ConnectorM1 ///< Secondary, ganged press motor.
/** @} */

//==================================================================================================
// Sensor & Control Parameters
//==================================================================================================
/**
 * @name Sensor and Control Loop Parameters
 * @{
 */
#define EWMA_ALPHA_TORQUE               0.2f      ///< Smoothing factor (alpha) for the EWMA filter on motor torque readings.
/** @} */

//==================================================================================================
// Motion & Operation Defaults
//==================================================================================================
/**
 * @name Motion and Operation Parameters
 * @{
 */

/**
 * @name Motor Setup Defaults
 * @{
 */
#define TORQUE_HLFB_AT_POSITION		-9999.0f  ///< Special value from ClearCore HLFB when a move is complete and the motor is at position.
#define MOTOR_DEFAULT_VEL_MAX_MMS           156.25f   ///< Default maximum velocity for motors in mm/s.
#define MOTOR_DEFAULT_ACCEL_MAX_MMSS        625.0f    ///< Default maximum acceleration for motors in mm/s^2.
#define MOTOR_DEFAULT_VEL_MAX_SPS           (int)(MOTOR_DEFAULT_VEL_MAX_MMS * STEPS_PER_MM) ///< Default max velocity in steps/sec, derived from mm/s.
#define MOTOR_DEFAULT_ACCEL_MAX_SPS2        (int)(MOTOR_DEFAULT_ACCEL_MAX_MMSS * STEPS_PER_MM) ///< Default max acceleration in steps/sec^2, derived from mm/s^2.
/** @} */

/**
 * @name Timeouts
 * @{
 */
#define MOVE_START_TIMEOUT_MS           (250)     ///< Time (in ms) to wait for a move to start before flagging a motor error.
/** @} */

/**
 * @name Initialization Defaults
 * @{
 */
#define DEFAULT_TORQUE_LIMIT       80.0f     ///< Default torque limit (%) for general motor operations.
#define DEFAULT_TORQUE_OFFSET      -2.4f     ///< Default torque offset (%) to account for sensor bias or no-load friction.
/** @} */

/**
 * @name Homing Defaults
 * @{
 */
#define HOMING_STROKE_MM           500.0f    ///< Maximum travel distance (mm) during a homing sequence.
#define HOMING_RAPID_VEL_MMS       5.0f      ///< Velocity (mm/s) for the initial high-speed search for the hard stop.
#define HOMING_TOUCH_VEL_MMS       1.0f      ///< Velocity (mm/s) for the final, slow-speed precise touch-off.
#define HOMING_BACKOFF_VEL_MMS     1.0f      ///< Velocity (mm/s) for backing off the hard stop.
#define HOMING_ACCEL_MMSS          100.0f    ///< Acceleration (mm/s^2) for all homing moves.
#define HOMING_SEARCH_TORQUE_PERCENT 10.0f   ///< Torque limit (%) used to detect the hard stop.
#define HOMING_BACKOFF_TORQUE_PERCENT 40.0f  ///< Higher torque limit (%) for the back-off move to prevent stalling.
#define HOMING_BACKOFF_MM          1.0f      ///< Distance (mm) to back off from the hard stop.
/** @} */

/**
 * @name Move Defaults
 * @{
 */
#define MOVE_DEFAULT_TORQUE_PERCENT         30        ///< Default torque limit (%) for moves.
#define MOVE_DEFAULT_VELOCITY_MMS           6.25f     ///< Default velocity (mm/s) for moves.
#define MOVE_DEFAULT_ACCEL_MMSS             62.5f     ///< Default acceleration (mm/s^2) for moves.
#define MOVE_DEFAULT_VELOCITY_SPS           (int)(MOVE_DEFAULT_VELOCITY_MMS * STEPS_PER_MM) ///< Default velocity in steps/sec.
#define MOVE_DEFAULT_ACCEL_SPS2             (int)(MOVE_DEFAULT_ACCEL_MMSS * STEPS_PER_MM)   ///< Default acceleration in steps/sec^2.
/** @} */

/**
 * @name Force Sensor Configuration
 * @{
 */
#define FORCE_SENSOR_ENABLED                true      ///< Enable/disable force sensor functionality. Set to false if no force transducer is connected.
#define FORCE_SENSOR_SCALE_FACTOR           -0.00023076f  ///< Default scale factor: kg = raw_adc × scale. Calibrated: -52000 raw = 12.0 kg → 1/-4333.33
#define FORCE_SENSOR_OFFSET_KG              6.5f     ///< Default offset (kg) to add to all force readings for calibration.

/** Machine strain compensation coefficients (force vs. distance polynomial) */
#define MACHINE_STRAIN_COEFF_X4            -143.0f         ///< x^4 coefficient (force in kg, displacement in millimeters)
#define MACHINE_STRAIN_COEFF_X3             592.0f         ///< x^3 coefficient
#define MACHINE_STRAIN_COEFF_X2            -365.0f         ///< x^2 coefficient
#define MACHINE_STRAIN_COEFF_X1             127.0f         ///< x coefficient
#define MACHINE_STRAIN_COEFF_C              -2.15f         ///< Constant term
#define MACHINE_STRAIN_MAX_DEFLECTION_MM     2.0f          ///< Max expected machine flex deflection used for inverse lookup
#define MACHINE_STRAIN_CONTACT_FORCE_KG      3.0f          ///< Force threshold to declare contact and start flex compensation
#define RETRACT_DEFAULT_SPEED_MMS           25.0f          ///< Default retract speed when none specified
#define FORCE_SENSOR_MIN_KG                 -10.0f    ///< Minimum valid force reading (kg). Below this triggers an error.
#define FORCE_SENSOR_MAX_KG                 1200.0f   ///< Maximum expected force (kg).
#define FORCE_SENSOR_MAX_SAFETY_FACTOR      1.2f      ///< Safety factor for maximum force (1.2x = 20% over max).
#define FORCE_SENSOR_MAX_LIMIT_KG           (FORCE_SENSOR_MAX_KG * FORCE_SENSOR_MAX_SAFETY_FACTOR) ///< Calculated max limit (1440 kg).
#define FORCE_SENSOR_TIMEOUT_MS             1000      ///< Time (ms) without readings before sensor is considered disconnected.
/** @} */

/**
 * @name Watchdog Timer Configuration
 * @{
 */
#define WATCHDOG_ENABLED                    true      ///< Enable/disable watchdog timer. When enabled, system must call safety check regularly or motors will be disabled.
#define WATCHDOG_TIMEOUT_MS                 100       ///< Watchdog timeout period in milliseconds. System will reset if not fed within this time.
#define WATCHDOG_RECOVERY_FLAG              0xDEADBEEF ///< Magic number written to backup register to indicate watchdog recovery.

// Breadcrumb codes to identify where the watchdog timeout occurred
#define WD_BREADCRUMB_SAFETY_CHECK          0x01      ///< Watchdog timeout in safety check
#define WD_BREADCRUMB_COMMS_UPDATE          0x02      ///< Watchdog timeout in communications update
#define WD_BREADCRUMB_RX_DEQUEUE            0x03      ///< Watchdog timeout in RX message dequeue
#define WD_BREADCRUMB_UPDATE_STATE          0x04      ///< Watchdog timeout in state machine update
#define WD_BREADCRUMB_FORCE_UPDATE          0x05      ///< Watchdog timeout in force sensor update
#define WD_BREADCRUMB_MOTOR_UPDATE          0x06      ///< Watchdog timeout in motor update
#define WD_BREADCRUMB_TELEMETRY             0x07      ///< Watchdog timeout in telemetry publishing
#define WD_BREADCRUMB_UDP_PROCESS           0x08      ///< Watchdog timeout in UDP packet processing
#define WD_BREADCRUMB_USB_PROCESS           0x09      ///< Watchdog timeout in USB serial processing
#define WD_BREADCRUMB_TX_QUEUE              0x0A      ///< Watchdog timeout in TX queue processing
#define WD_BREADCRUMB_UDP_SEND              0x0B      ///< Watchdog timeout in UDP packet send
#define WD_BREADCRUMB_NETWORK_REFRESH       0x0C      ///< Watchdog timeout in network packet refresh
#define WD_BREADCRUMB_USB_SEND              0x0D      ///< Watchdog timeout in USB send operation
#define WD_BREADCRUMB_USB_RECONNECT         0x0E      ///< Watchdog timeout in USB reconnection handling
#define WD_BREADCRUMB_USB_RECOVERY          0x0F      ///< Watchdog timeout in USB recovery operation
#define WD_BREADCRUMB_REPORT_EVENT          0x10      ///< Watchdog timeout in reportEvent (start)
#define WD_BREADCRUMB_ENQUEUE_TX            0x11      ///< Watchdog timeout in enqueueTx
#define WD_BREADCRUMB_MOTOR_IS_FAULT        0x12      ///< Watchdog timeout in motor fault check
#define WD_BREADCRUMB_MOTOR_STATE_SWITCH    0x13      ///< Watchdog timeout in state machine switch
#define WD_BREADCRUMB_PROCESS_TX_QUEUE      0x14      ///< Watchdog timeout in processTxQueue start
#define WD_BREADCRUMB_TX_QUEUE_DEQUEUE      0x15      ///< Watchdog timeout in TX queue dequeue
#define WD_BREADCRUMB_TX_QUEUE_UDP          0x16      ///< Watchdog timeout in TX queue UDP send
#define WD_BREADCRUMB_TX_QUEUE_USB          0x17      ///< Watchdog timeout in TX queue USB send
#define WD_BREADCRUMB_DISPATCH_CMD          0x18      ///< Watchdog timeout in dispatchCommand
#define WD_BREADCRUMB_PARSE_CMD             0x19      ///< Watchdog timeout in parseCommand
#define WD_BREADCRUMB_MOTOR_FAULT_REPORT    0x1A      ///< Watchdog timeout in motor fault report
#define WD_BREADCRUMB_STATE_BUSY_CHECK      0x1B      ///< Watchdog timeout in motor busy check
#define WD_BREADCRUMB_UDP_PACKET_READ       0x1C      ///< Watchdog timeout in UDP packet read
#define WD_BREADCRUMB_RX_ENQUEUE            0x1D      ///< Watchdog timeout in RX enqueue
#define WD_BREADCRUMB_USB_AVAILABLE         0x1E      ///< Watchdog timeout checking USB available
#define WD_BREADCRUMB_USB_READ              0x1F      ///< Watchdog timeout in USB read
#define WD_BREADCRUMB_NETWORK_INPUT         0x20      ///< Watchdog timeout in network low_level_input
#define WD_BREADCRUMB_LWIP_INPUT            0x21      ///< Watchdog timeout in ethernetif_input (lwIP)
#define WD_BREADCRUMB_LWIP_TIMEOUT          0x22      ///< Watchdog timeout in sys_check_timeouts (lwIP)
#define WD_BREADCRUMB_UNKNOWN               0xFF      ///< Watchdog timeout in unknown location
/** @} */
/** @} */

