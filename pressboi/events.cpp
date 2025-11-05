/**
 * @file events.cpp
 * @brief Event sending implementation for the Pressboi controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from events.json on 2025-11-04 11:36:14
 */

#include "events.h"
#include "commands.h"
#include <stdio.h>
#include <string.h>

//==================================================================================================
// Event Sending Implementation
//==================================================================================================

// Forward declaration - implemented in comms_controller
extern void sendMessage(const char* msg);

void sendEvent(Event event) {
    switch (event) {
        case EVENT_UNKNOWN:
        default:
            // Do nothing for unknown events
            break;
    }
}

void sendEventInt(Event event, int32_t param) {
    switch (event) {
        default:
            // Fall back to simple event for events without int params
            sendEvent(event);
            break;
    }
}

void sendEventString(Event event, const char* param) {
    switch (event) {
        default:
            // Fall back to simple event for events without string params
            sendEvent(event);
            break;
    }
}

void sendEventMulti(Event event, int32_t param1, int32_t param2) {
    switch (event) {
        // Add specific cases for events that need multiple parameters
        default:
            // Fall back to single int parameter
            sendEventInt(event, param1);
            break;
    }
}