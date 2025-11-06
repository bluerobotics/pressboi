/**
 * @file commands.h
 * @brief Defines the command interface for the Pressboi controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from commands.json on 2025-11-05 20:42:41
 * 
 * This header file defines all commands that can be sent TO the Pressboi device.
 * For message prefixes and events, see events.h
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
#define CMD_STR_RESET                               "reset" ///< No description available.
#define CMD_STR_SET_RETRACT                         "set_retract " ///< No description available.
#define CMD_STR_RETRACT                             "retract" ///< No description available.
#define CMD_STR_PAUSE                               "pause" ///< No description available.
#define CMD_STR_RESUME                              "resume" ///< No description available.
#define CMD_STR_CANCEL                              "cancel" ///< No description available.
#define CMD_STR_ENABLE                              "enable" ///< No description available.
#define CMD_STR_DISABLE                             "disable" ///< No description available.
#define CMD_STR_TEST_WATCHDOG                       "test_watchdog" ///< No description available.
/** @} */

/**
 * @name Motion Commands
 * @{
 */
#define CMD_STR_HOME                                "home" ///< No description available.
#define CMD_STR_MOVE_ABS                            "move_abs " ///< No description available.
#define CMD_STR_MOVE_INC                            "move_inc " ///< No description available.
/** @} */

//==================================================================================================
// Response Message Prefixes (Device → Host)
//==================================================================================================

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
    CMD_DISCOVER_DEVICE,                                    ///< @see CMD_STR_DISCOVER_DEVICE
    CMD_RESET,                                    ///< @see CMD_STR_RESET
    CMD_SET_RETRACT,                                    ///< @see CMD_STR_SET_RETRACT
    CMD_RETRACT,                                    ///< @see CMD_STR_RETRACT
    CMD_PAUSE,                                    ///< @see CMD_STR_PAUSE
    CMD_RESUME,                                    ///< @see CMD_STR_RESUME
    CMD_CANCEL,                                    ///< @see CMD_STR_CANCEL
    CMD_ENABLE,                                    ///< @see CMD_STR_ENABLE
    CMD_DISABLE,                                    ///< @see CMD_STR_DISABLE
    CMD_TEST_WATCHDOG,                                    ///< @see CMD_STR_TEST_WATCHDOG

    // Motion Commands
    CMD_HOME,                                    ///< @see CMD_STR_HOME
    CMD_MOVE_ABS,                                    ///< @see CMD_STR_MOVE_ABS
    CMD_MOVE_INC                                     ///< @see CMD_STR_MOVE_INC
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