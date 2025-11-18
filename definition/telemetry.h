/**
 * @file telemetry.h
 * @brief Telemetry structure and construction interface for the Pressboi controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from telemetry.json on 2025-11-03 11:25:17
 * 
 * This header defines the complete telemetry data structure for the Pressboi.
 * All telemetry fields are assembled in one centralized location.
 * To modify telemetry fields, edit telemetry.json and regenerate this file.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

//==================================================================================================
// Telemetry Field Keys
//==================================================================================================

/**
 * @name Telemetry Field Identifiers
 * @brief String keys used in telemetry messages.
 * Format: "PRESSBOI_TELEM: field1:value1,field2:value2,..."
 * @{
 */
#define TELEM_KEY_MAIN_STATE                     "MAIN_STATE               "  ///< Overall press system state
#define TELEM_KEY_FORCE                          "force                    "  ///< Current force being applied by the press
#define TELEM_KEY_FORCE_LIMIT                    "force_limit              "  ///< Maximum force limit for current operation
#define TELEM_KEY_ENABLED0                       "enabled0                 "  ///< Power enable status for motor 1
#define TELEM_KEY_ENABLED1                       "enabled1                 "  ///< Power enable status for motor 2
#define TELEM_KEY_CURRENT_POS                    "current_pos              "  ///< Current position of press axis
#define TELEM_KEY_START_POS                      "start_pos                "  ///< Preset starting position for pressing routine
#define TELEM_KEY_TARGET_POS                     "target_pos               "  ///< Target position for current move operation
#define TELEM_KEY_TORQUE_M1                      "torque_m1                "  ///< Current motor torque percentage for motor 1
#define TELEM_KEY_TORQUE_M2                      "torque_m2                "  ///< Current motor torque percentage for motor 2
#define TELEM_KEY_HOMED                          "homed                    "  ///< Indicates if press has been homed to zero position
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
    int32_t      MAIN_STATE                    ; ///< Overall press system state
    float        force                         ; ///< Current force being applied by the press
    float        force_limit                   ; ///< Maximum force limit for current operation
    int32_t      enabled0                      ; ///< Power enable status for motor 1
    int32_t      enabled1                      ; ///< Power enable status for motor 2
    float        current_pos                   ; ///< Current position of press axis
    float        start_pos                     ; ///< Preset starting position for pressing routine
    float        target_pos                    ; ///< Target position for current move operation
    float        torque_m1                     ; ///< Current motor torque percentage for motor 1
    float        torque_m2                     ; ///< Current motor torque percentage for motor 2
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
 * @brief Send telemetry message via Serial.
 * @param data Pointer to TelemetryData structure containing current values
 * 
 * @details Builds and transmits the complete telemetry message.
 */
void telemetry_send(const TelemetryData* data);