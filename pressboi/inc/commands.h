/**
 * @file commands.h
 * @brief Defines the command interface for the Pressboi controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from commands.json on 2025-11-03 15:26:51
 * 
 * This header file defines all commands that can be sent TO the Pressboi device.
 * For response message formats, see responses.h
 * To modify commands, edit commands.json and regenerate this file.
 */
#pragma once

//==================================================================================================
// Command Strings (Host → Device)
//==================================================================================================

/**
 * @name General System Commands
 * @{
 */
#define CMD_STR_DISCOVER_DEVICE                     "DISCOVER_DEVICE" ///< Generic command for any device to respond to.
#define CMD_STR_ABORT                               "abort" ///< No description available.
#define CMD_STR_CLEAR_ERRORS                        "clear_errors" ///< No description available.
#define CMD_STR_SET_START_POS                       "set_start_pos " ///< No description available.
#define CMD_STR_PAUSE                               "pause" ///< No description available.
#define CMD_STR_RESUME                              "resume" ///< No description available.
#define CMD_STR_CANCEL                              "cancel" ///< No description available.
#define CMD_STR_ENABLE                              "enable" ///< No description available.
#define CMD_STR_DISABLE                             "disable" ///< No description available.
/** @} */

/**
 * @name Motion Commands
 * @{
 */
#define CMD_STR_HOME                                "home" ///< No description available.
#define CMD_STR_MOVE_ABS                            "move_abs " ///< No description available.
#define CMD_STR_MOVE_INC                            "move_inc " ///< No description available.
#define CMD_STR_MOVE_TO_START                       "move_to_start" ///< No description available.
/** @} */

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

/**
 * @name Event Prefix
 * @brief Prefix for event messages.
 * @{
 */
#define EVENT_PREFIX                        "PRESSBOI_EVENT: "         ///< Prefix for all event messages.
/** @} */

//==================================================================================================
// Command Enum
//==================================================================================================

/**
 * @enum Command
 * @brief Enumerates all possible commands that can be processed by the Pressboi.
 * @details This enum provides a type-safe way to handle incoming commands.
 */
typedef enum {
    CMD_UNKNOWN,                        ///< Represents an unrecognized or invalid command.

    // General System Commands
    CMD_DISCOVER_DEVICE                     ///< @see CMD_STR_DISCOVER_DEVICE,
    CMD_ABORT                               ///< @see CMD_STR_ABORT,
    CMD_CLEAR_ERRORS                        ///< @see CMD_STR_CLEAR_ERRORS,
    CMD_SET_START_POS                       ///< @see CMD_STR_SET_START_POS,
    CMD_PAUSE                               ///< @see CMD_STR_PAUSE,
    CMD_RESUME                              ///< @see CMD_STR_RESUME,
    CMD_CANCEL                              ///< @see CMD_STR_CANCEL,
    CMD_ENABLE                              ///< @see CMD_STR_ENABLE,
    CMD_DISABLE                             ///< @see CMD_STR_DISABLE,

    // Motion Commands
    CMD_HOME                                ///< @see CMD_STR_HOME,
    CMD_MOVE_ABS                            ///< @see CMD_STR_MOVE_ABS,
    CMD_MOVE_INC                            ///< @see CMD_STR_MOVE_INC,
    CMD_MOVE_TO_START                       ///< @see CMD_STR_MOVE_TO_START
} Command;

//==================================================================================================
// Command Parser Functions
//==================================================================================================

/**
 * @brief Parse a command string and return the corresponding Command enum.
 * @param cmdStr The command string to parse
 * @return The parsed Command enum value, or CMD_UNKNOWN if not recognized
 */
Command parseCommand(const char* cmdStr);

/**
 * @brief Extract parameter string from a command.
 * @param cmdStr The full command string
 * @param cmd The parsed command enum
 * @return Pointer to the parameter substring, or NULL if no parameters
 */
const char* getCommandParams(const char* cmdStr, Command cmd);