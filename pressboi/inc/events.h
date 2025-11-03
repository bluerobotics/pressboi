/**
 * @file events.h
 * @brief Defines all event types that can be sent from the Pressboi controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from events.json on 2025-11-03 15:23:43
 * 
 * This header file defines all events sent FROM the Pressboi device TO the host.
 * Events are asynchronous notifications that can trigger host-side actions.
 * For command definitions (host → device), see commands.h
 * For response definitions, see responses.h
 * To modify events, edit events.json and regenerate this file.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

//==================================================================================================
// Event String Definitions
//==================================================================================================

/**
 * @name Event String Identifiers
 * @brief String identifiers used in event messages.
 * Format: "PRESSBOI_EVENT: event_name [param1] [param2] ..."
 * @{
 */
/** @} */

//==================================================================================================
// Event Enum
//==================================================================================================

/**
 * @enum Event
 * @brief Enumerates all possible events that can be sent by the Pressboi.
 * @details This enum provides a type-safe way to handle outgoing events.
 */
typedef enum {
    EVENT_UNKNOWN,                        ///< Represents an unrecognized or invalid event.

} Event;

//==================================================================================================
// Event Sending Functions
//==================================================================================================

/**
 * @brief Send an event message with no parameters.
 * @param event The event enum to send
 */
void sendEvent(Event event);

/**
 * @brief Send an event message with a single integer parameter.
 * @param event The event enum to send
 * @param param The integer parameter
 */
void sendEventInt(Event event, int32_t param);

/**
 * @brief Send an event message with a single string parameter.
 * @param event The event enum to send
 * @param param The string parameter
 */
void sendEventString(Event event, const char* param);

/**
 * @brief Send an event message with multiple parameters.
 * @param event The event enum to send
 * @param param1 First parameter (integer)
 * @param param2 Second parameter (integer)
 */
void sendEventMulti(Event event, int32_t param1, int32_t param2);

//==================================================================================================
// Usage Examples
//==================================================================================================

/**
 * @section Event Sending Example
 * @code
 * // Send a simple event
 * sendEvent(EVENT_SCRIPT_RUN);
 * 
 * // Send an event with a reason
 * sendEventString(EVENT_SCRIPT_HOLD, "Light curtain triggered");
 * 
 * // Send an event with numeric data
 * sendEventInt(EVENT_SCRIPT_RESET, 1); // zone 1
 * @endcode
 */