/**
 * @file variables.h
 * @brief Telemetry structure and construction interface for the Pressboi controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from telemetry.json on 2025-11-18 11:43:06
 * 
 * This header defines the complete telemetry data structure for the Pressboi.
 * All telemetry fields are assembled in one centralized location.
 * To modify telemetry fields, edit telemetry.json and regenerate this file.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

//==================================================================================================
// Telemetry Field Keys
//==================================================================================================

/**
 * @name Telemetry Field Identifiers
 * @brief String keys used in telemetry messages.
 * Format: "PRESSBOI_TELEM: field1:value1,field2:value2,..."
 * @{
 */
#define TELEM_KEY_MAIN_STATE                     "MAIN_STATE"  ///< Overall press system state
#define TELEM_KEY_FORCE_LOAD_CELL                "force_load_cell"  ///< Force from load cell sensor
#define TELEM_KEY_FORCE_MOTOR_TORQUE             "force_motor_torque"  ///< Force calculated from motor torque
#define TELEM_KEY_FORCE_LIMIT                    "force_limit"  ///< Maximum force limit for current operation
#define TELEM_KEY_FORCE_SOURCE                   "force_source"  ///< Source of force reading: load_cell or motor_torque
#define TELEM_KEY_FORCE_ADC_RAW                  "force_adc_raw"  ///< Raw ADC value from HX711 load cell amplifier (for calibration)
#define TELEM_KEY_JOULES                         "joules"  ///< Energy expended during current move (force Ã— distance integrated at 50Hz)
#define TELEM_KEY_ENABLED0                       "enabled0"  ///< Power enable status for motor 1
#define TELEM_KEY_ENABLED1                       "enabled1"  ///< Power enable status for motor 2
#define TELEM_KEY_CURRENT_POS                    "current_pos"  ///< Current position of press axis
#define TELEM_KEY_RETRACT_POS                    "retract_pos"  ///< Preset retract position for the press
#define TELEM_KEY_TARGET_POS                     "target_pos"  ///< Target position for current move operation
#define TELEM_KEY_ENDPOINT                       "endpoint"  ///< Actual position where last move ended (force trigger or completion)
#define TELEM_KEY_STARTPOINT                     "startpoint"  ///< Position where press threshold was crossed (press started)
#define TELEM_KEY_PRESS_THRESHOLD                "press_threshold"  ///< Force threshold (kg) for energy/startpoint recording
#define TELEM_KEY_TORQUE_AVG                     "torque_avg"  ///< Average motor torque percentage
#define TELEM_KEY_HOMED                          "homed"  ///< Indicates if press has been homed to zero position
/** @} */

//==================================================================================================
// Telemetry Data Structure
//==================================================================================================

/**
 * @struct TelemetryData
 * @brief Complete telemetry state for the Pressboi device.
 * @details This structure contains all telemetry values that are transmitted to the host.
 */
typedef struct {
    const char*  MAIN_STATE                    ; ///< Overall press system state
    float        force_load_cell               ; ///< Force from load cell sensor
    float        force_motor_torque            ; ///< Force calculated from motor torque
    float        force_limit                   ; ///< Maximum force limit for current operation
    const char*  force_source                  ; ///< Source of force reading: load_cell or motor_torque
    int32_t      force_adc_raw                 ; ///< Raw ADC value from HX711 load cell amplifier (for calibration)
    float        joules                        ; ///< Energy expended during current move (force Ã— distance integrated at 50Hz)
    int32_t      enabled0                      ; ///< Power enable status for motor 1
    int32_t      enabled1                      ; ///< Power enable status for motor 2
    float        current_pos                   ; ///< Current position of press axis
    float        retract_pos                   ; ///< Preset retract position for the press
    float        target_pos                    ; ///< Target position for current move operation
    float        endpoint                      ; ///< Actual position where last move ended (force trigger or completion)
    float        startpoint                    ; ///< Position where press threshold was crossed (press started)
    float        press_threshold               ; ///< Force threshold (kg) for energy/startpoint recording
    float        torque_avg                    ; ///< Average motor torque percentage
    int32_t      homed                         ; ///< Indicates if press has been homed to zero position
} TelemetryData;

//==================================================================================================
// Telemetry Construction Functions
//==================================================================================================

/**
 * @brief Initialize telemetry data structure with default values.
 * @param data Pointer to TelemetryData structure to initialize
 */
void telemetry_init(TelemetryData* data);

/**
 * @brief Build complete telemetry message string from data structure.
 * @param data Pointer to TelemetryData structure containing current values
 * @param buffer Output buffer to write telemetry message
 * @param buffer_size Size of output buffer
 * @return Number of characters written (excluding null terminator)
 * 
 * @details Constructs a message in the format: "PRESSBOI_TELEM: field1:value1,field2:value2,..."
 */
int telemetry_build_message(const TelemetryData* data, char* buffer, size_t buffer_size);

/**
 * @brief Send telemetry message via comms controller.
 * @param data Pointer to TelemetryData structure containing current values
 * 
 * @details Builds and transmits the complete telemetry message.
 */
void telemetry_send(const TelemetryData* data);