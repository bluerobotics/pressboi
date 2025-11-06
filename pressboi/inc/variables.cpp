/**
 * @file variables.cpp
 * @brief Telemetry construction implementation for the Pressboi controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from telemetry.json on 2025-11-05 17:20:01
 */

#include "variables.h"
#include "commands.h"
#include <stdio.h>
#include <string.h>

// Forward declaration - implemented in comms_controller
extern void sendMessage(const char* msg);

//==================================================================================================
// Telemetry Initialization
//==================================================================================================

void telemetry_init(TelemetryData* data) {
    if (data == NULL) return;
    
    data->MAIN_STATE = "STANDBY";
    data->force = 0.0f;
    data->force_limit = 1000.0f;
    data->enabled0 = 1;
    data->enabled1 = 1;
    data->current_pos = 0.0f;
    data->retract_pos = 0.0f;
    data->target_pos = 0.0f;
    data->torque_avg = 0.0f;
    data->homed = 0;
}

//==================================================================================================
// Telemetry Message Construction
//==================================================================================================

int telemetry_build_message(const TelemetryData* data, char* buffer, size_t buffer_size) {
    if (data == NULL || buffer == NULL || buffer_size == 0) return 0;
    
    size_t pos = 0;
    
    // Write prefix
    pos += snprintf(buffer + pos, buffer_size - pos, "%s", TELEM_PREFIX);
    
    // MAIN_STATE
    if (pos < buffer_size) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%s:%s,", TELEM_KEY_MAIN_STATE, data->MAIN_STATE);
    }
    
    // force
    if (pos < buffer_size) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%s:%.1f,", TELEM_KEY_FORCE, data->force);
    }
    
    // force_limit
    if (pos < buffer_size) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%s:%.1f,", TELEM_KEY_FORCE_LIMIT, data->force_limit);
    }
    
    // enabled0
    if (pos < buffer_size) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%s:%ld,", TELEM_KEY_ENABLED0, (long)data->enabled0);
    }
    
    // enabled1
    if (pos < buffer_size) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%s:%ld,", TELEM_KEY_ENABLED1, (long)data->enabled1);
    }
    
    // current_pos
    if (pos < buffer_size) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%s:%.2f,", TELEM_KEY_CURRENT_POS, data->current_pos);
    }
    
    // retract_pos
    if (pos < buffer_size) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%s:%.2f,", TELEM_KEY_RETRACT_POS, data->retract_pos);
    }
    
    // target_pos
    if (pos < buffer_size) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%s:%.2f,", TELEM_KEY_TARGET_POS, data->target_pos);
    }
    
    // torque_avg
    if (pos < buffer_size) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%s:%.1f,", TELEM_KEY_TORQUE_AVG, data->torque_avg);
    }
    
    // homed
    if (pos < buffer_size) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%s:%ld", TELEM_KEY_HOMED, (long)data->homed);
    }
    
    return (int)pos;
}

//==================================================================================================
// Telemetry Transmission
//==================================================================================================

// NOTE: You need to provide a sendMessage() implementation based on your comms setup
// For example:
// extern CommsController comms;
// #define sendMessage(msg) comms.enqueueTx(msg, comms.m_guiIp, comms.m_guiPort)

void telemetry_send(const TelemetryData* data) {
    char buffer[512];
    int len = telemetry_build_message(data, buffer, sizeof(buffer));
    
    if (len > 0) {
        sendMessage(buffer);
    }
}