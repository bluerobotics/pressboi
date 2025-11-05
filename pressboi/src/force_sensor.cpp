/**
 * @file force_sensor.cpp
 * @author Eldin Miller-Stead
 * @date November 3, 2025
 * @brief Implementation of force sensor communication with Rugeduino/HX711.
 */

#include "force_sensor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ForceSensor::ForceSensor() {
    m_force_kg = 0.0f;
    m_raw_value = 0;
    m_last_reading_time = 0;
    m_buffer_index = 0;
    memset(m_serial_buffer, 0, sizeof(m_serial_buffer));
}

void ForceSensor::setup() {
    // Configure COM-0 for TTL UART communication
    ConnectorCOM0.Mode(Connector::TTL);
    ConnectorCOM0.Speed(115200);  // Match Rugeduino baud rate
    ConnectorCOM0.PortOpen();
    
    // Wait for port to stabilize
    Delay_ms(100);
}

void ForceSensor::update() {
    // Read any available data from COM-0
    int c;
    while ((c = ConnectorCOM0.CharGet()) != -1) {
        // Process character
        if (c == '\n' || c == '\r') {
            // End of line - parse the accumulated data
            if (m_buffer_index > 0) {
                m_serial_buffer[m_buffer_index] = '\0';
                parseSerialData();
                m_buffer_index = 0;
            }
        } else if (m_buffer_index < sizeof(m_serial_buffer) - 1) {
            // Add character to buffer
            m_serial_buffer[m_buffer_index++] = (char)c;
        } else {
            // Buffer overflow - reset
            m_buffer_index = 0;
        }
    }
}

void ForceSensor::parseSerialData() {
    // Expected format: "12.34" (force in kg as floating point)
    float force = 0.0f;
    if (sscanf(m_serial_buffer, "%f", &force) == 1) {
        m_force_kg = force;
        m_last_reading_time = Milliseconds();
    }
    // Could also parse: "RAW:123456,FORCE:12.34" for more detail
}

bool ForceSensor::isConnected() const {
    // Consider connected if we received data in the last second
    return (Milliseconds() - m_last_reading_time) < 1000;
}

void ForceSensor::tare() {
    // Send 'T' command to Rugeduino to tare/zero the scale
    ConnectorCOM0.Send("T\n");
}

