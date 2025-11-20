/**
 * @file error_log.h
 * @author Eldin Miller-Stead
 * @date November 20, 2025
 * @brief Defines an internal error/debug logging system for firmware diagnostics.
 *
 * @details This module provides a circular buffer for logging internal events,
 * errors, and debug information. The log can be dumped via command over the
 * network, which is crucial for diagnosing intermittent USB communication issues.
 * The log is designed to be lightweight and non-blocking to avoid interfering
 * with real-time operations or triggering the watchdog.
 */
#pragma once

#include <stdint.h>
#include <cstring>

// Log configuration
#define ERROR_LOG_SIZE 100           ///< Maximum number of log entries (circular buffer)
#define ERROR_LOG_MSG_LENGTH 80      ///< Maximum length of each log message
#define HEARTBEAT_LOG_SIZE 2880      ///< 24 hours of heartbeats at 30-second intervals

/**
 * @enum LogLevel
 * @brief Defines the severity level of log entries.
 */
enum LogLevel : uint8_t {
    LOG_DEBUG,      ///< Debug information (verbose)
    LOG_INFO,       ///< General information
    LOG_WARNING,    ///< Warning - something unexpected but not critical
    LOG_ERROR,      ///< Error - something went wrong
    LOG_CRITICAL    ///< Critical error - system may be in bad state
};

/**
 * @struct LogEntry
 * @brief Represents a single log entry in the circular buffer.
 */
struct LogEntry {
    uint32_t timestamp;                  ///< Millisecond timestamp when entry was created
    LogLevel level;                      ///< Severity level of the entry
    char message[ERROR_LOG_MSG_LENGTH];  ///< The log message
};

/**
 * @struct HeartbeatEntry
 * @brief Compact heartbeat status entry (only 8 bytes vs 88 bytes for LogEntry).
 */
struct HeartbeatEntry {
    uint32_t timestamp;      ///< Millisecond timestamp
    uint8_t usbConnected;    ///< 1 if USB host connected, 0 if disconnected
    uint8_t networkActive;   ///< 1 if network link up, 0 if down
    uint8_t usbAvailable;    ///< Bytes available in USB TX buffer (0-255)
    uint8_t reserved;        ///< Reserved for future use
};

/**
 * @class ErrorLog
 * @brief Manages a circular buffer of log entries for firmware diagnostics.
 */
class ErrorLog {
public:
    /**
     * @brief Constructs the error log.
     */
    ErrorLog();

    /**
     * @brief Adds a log entry to the circular buffer.
     * @param level The severity level of the entry.
     * @param message The log message (will be truncated if too long).
     */
    void log(LogLevel level, const char* message);

    /**
     * @brief Adds a formatted log entry to the circular buffer.
     * @param level The severity level of the entry.
     * @param format Printf-style format string.
     * @param ... Format arguments.
     */
    void logf(LogLevel level, const char* format, ...);

    /**
     * @brief Gets the total number of log entries in the buffer.
     * @return The number of valid entries (up to ERROR_LOG_SIZE).
     */
    int getEntryCount() const;

    /**
     * @brief Gets a log entry by index (0 = oldest, getEntryCount()-1 = newest).
     * @param index The index of the entry to retrieve.
     * @param[out] entry Pointer to a LogEntry struct to populate.
     * @return true if the entry was retrieved successfully, false if index out of range.
     */
    bool getEntry(int index, LogEntry* entry) const;

    /**
     * @brief Clears all log entries.
     */
    void clear();

private:
    LogEntry m_buffer[ERROR_LOG_SIZE];  ///< Circular buffer of log entries
    int m_head;                          ///< Index of next write position
    int m_count;                         ///< Number of valid entries in buffer
};

/**
 * @class HeartbeatLog
 * @brief Manages a compact circular buffer for system health heartbeats.
 * @details Uses only 8 bytes per entry (vs 88 for LogEntry) to store 24 hours of data.
 * At 30-second intervals, 2880 entries = 24 hours, using only ~23KB of RAM.
 */
class HeartbeatLog {
public:
    /**
     * @brief Constructs the heartbeat log.
     */
    HeartbeatLog();

    /**
     * @brief Adds a heartbeat entry to the circular buffer.
     * @param usbConnected True if USB host is connected.
     * @param networkActive True if network link is up.
     * @param usbAvailable Bytes available in USB TX buffer.
     */
    void log(bool usbConnected, bool networkActive, uint8_t usbAvailable);

    /**
     * @brief Gets the total number of heartbeat entries in the buffer.
     * @return The number of valid entries (up to HEARTBEAT_LOG_SIZE).
     */
    int getEntryCount() const;

    /**
     * @brief Gets a heartbeat entry by index (0 = oldest, getEntryCount()-1 = newest).
     * @param index The index of the entry to retrieve.
     * @param[out] entry Pointer to a HeartbeatEntry struct to populate.
     * @return true if the entry was retrieved successfully, false if index out of range.
     */
    bool getEntry(int index, HeartbeatEntry* entry) const;

    /**
     * @brief Clears all heartbeat entries.
     */
    void clear();

private:
    HeartbeatEntry m_buffer[HEARTBEAT_LOG_SIZE];  ///< Circular buffer (only 8 bytes per entry!)
    int m_head;                                    ///< Index of next write position
    int m_count;                                   ///< Number of valid entries in buffer
};

// Global log instances
extern ErrorLog g_errorLog;
extern HeartbeatLog g_heartbeatLog;

