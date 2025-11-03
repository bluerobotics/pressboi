/**
 * @file responses.h
 * @brief Defines all response message formats for the Pressboi controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from telemetry.json on 2025-11-03 11:44:04
 * 
 * This header file defines all messages sent FROM the Pressboi device TO the host.
 * This includes status messages, telemetry data, and discovery responses.
 * For command definitions (host → device), see commands.h
 * For telemetry structure, see telemetry.h
 * To modify response fields, edit telemetry.json and regenerate this file.
 */
#pragma once

//==================================================================================================
// Response Message Prefixes (Device → Host)
//==================================================================================================

/**
 * @name Status Message Prefixes
 * @brief Prefixes used for different types of status messages from the device.
 * @{
 */
#define STATUS_PREFIX_INFO                  "PRESSBOI_INFO: "          ///< Prefix for informational status messages.
#define STATUS_PREFIX_START                 "PRESSBOI_START: "         ///< Prefix for messages indicating the start of an operation.
#define STATUS_PREFIX_DONE                  "PRESSBOI_DONE: "          ///< Prefix for messages indicating the successful completion of an operation.
#define STATUS_PREFIX_ERROR                 "PRESSBOI_ERROR: "         ///< Prefix for messages indicating an error or fault.
#define STATUS_PREFIX_DISCOVERY             "DISCOVERY_RESPONSE: "     ///< Prefix for the device discovery response.
/** @} */

/**
 * @name Telemetry Prefix
 * @brief Prefix for periodic telemetry data messages.
 * @{
 */
#define TELEM_PREFIX                        "PRESSBOI_TELEM: "         ///< Prefix for all telemetry messages.
/** @} */

//==================================================================================================
// Usage Examples
//==================================================================================================

/**
 * @section Status Message Example
 * @code
 * // Send an info message
 * Serial.print(STATUS_PREFIX_INFO);
 * Serial.println("System initialized");
 * 
 * // Send a completion message
 * Serial.print(STATUS_PREFIX_DONE);
 * Serial.println("HEATER_ON");
 * @endcode
 * 
 * @section Telemetry Message Example
 * @code
 * // Use the telemetry.h interface for sending telemetry
 * #include "telemetry.h"
 * 
 * TelemetryData telem;
 * telemetry_init(&telem);
 * // ... update telem fields ...
 * telemetry_send(&telem);
 * @endcode
 */