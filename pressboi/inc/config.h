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
// Network Configuration
//==================================================================================================
/**
 * @name Network and Communication Settings
 * @{
 */
#define LOCAL_PORT                      8888      ///< The UDP port this device listens on for incoming commands.
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
#define FIRMWARE_VERSION                "1.3.0"   ///< Pressboi firmware version
#define STATUS_MESSAGE_BUFFER_SIZE      256       ///< Standard buffer size for composing status and error messages.
#define POST_ABORT_DELAY_MS             100       ///< Delay in milliseconds after an abort command to allow motors to come to a complete stop.
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
/** @} */
/** @} */

