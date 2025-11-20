/**
 * @file error_log.cpp
 * @author Eldin Miller-Stead
 * @date November 20, 2025
 * @brief Implements the internal error/debug logging system.
 */

#include "error_log.h"
#include "ClearCore.h"
#include <cstdarg>
#include <cstdio>

// Global log instances
ErrorLog g_errorLog;
HeartbeatLog g_heartbeatLog;

ErrorLog::ErrorLog() {
    m_head = 0;
    m_count = 0;
}

void ErrorLog::log(LogLevel level, const char* message) {
    // Get timestamp
    uint32_t timestamp = Milliseconds();
    
    // Add entry at head position
    m_buffer[m_head].timestamp = timestamp;
    m_buffer[m_head].level = level;
    strncpy(m_buffer[m_head].message, message, ERROR_LOG_MSG_LENGTH - 1);
    m_buffer[m_head].message[ERROR_LOG_MSG_LENGTH - 1] = '\0';
    
    // Advance head (circular)
    m_head = (m_head + 1) % ERROR_LOG_SIZE;
    
    // Update count (saturate at ERROR_LOG_SIZE)
    if (m_count < ERROR_LOG_SIZE) {
        m_count++;
    }
}

void ErrorLog::logf(LogLevel level, const char* format, ...) {
    char buffer[ERROR_LOG_MSG_LENGTH];
    
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, ERROR_LOG_MSG_LENGTH, format, args);
    va_end(args);
    
    log(level, buffer);
}

int ErrorLog::getEntryCount() const {
    return m_count;
}

bool ErrorLog::getEntry(int index, LogEntry* entry) const {
    if (index < 0 || index >= m_count) {
        return false;
    }
    
    // Calculate actual buffer position
    // If buffer is not full, oldest entry is at index 0
    // If buffer is full, oldest entry is at m_head
    int bufferIndex;
    if (m_count < ERROR_LOG_SIZE) {
        bufferIndex = index;
    } else {
        bufferIndex = (m_head + index) % ERROR_LOG_SIZE;
    }
    
    *entry = m_buffer[bufferIndex];
    return true;
}

void ErrorLog::clear() {
    m_head = 0;
    m_count = 0;
}

//==================================================================================================
// HeartbeatLog Implementation
//==================================================================================================

HeartbeatLog::HeartbeatLog() {
    m_head = 0;
    m_count = 0;
}

void HeartbeatLog::log(bool usbConnected, bool networkActive, uint8_t usbAvailable) {
    // Add compact entry at head position (only 8 bytes!)
    m_buffer[m_head].timestamp = Milliseconds();
    m_buffer[m_head].usbConnected = usbConnected ? 1 : 0;
    m_buffer[m_head].networkActive = networkActive ? 1 : 0;
    m_buffer[m_head].usbAvailable = usbAvailable;
    m_buffer[m_head].reserved = 0;
    
    // Advance head (circular)
    m_head = (m_head + 1) % HEARTBEAT_LOG_SIZE;
    
    // Update count (saturate at HEARTBEAT_LOG_SIZE)
    if (m_count < HEARTBEAT_LOG_SIZE) {
        m_count++;
    }
}

int HeartbeatLog::getEntryCount() const {
    return m_count;
}

bool HeartbeatLog::getEntry(int index, HeartbeatEntry* entry) const {
    if (index < 0 || index >= m_count) {
        return false;
    }
    
    // Calculate actual buffer position
    int bufferIndex;
    if (m_count < HEARTBEAT_LOG_SIZE) {
        bufferIndex = index;
    } else {
        bufferIndex = (m_head + index) % HEARTBEAT_LOG_SIZE;
    }
    
    *entry = m_buffer[bufferIndex];
    return true;
}

void HeartbeatLog::clear() {
    m_head = 0;
    m_count = 0;
}

