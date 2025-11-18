/**
 * @file commands.h
 * @brief Defines the command interface for the Pressboi controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from commands.json on 2025-11-03 11:25:17
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
#define CMD_STR_SET_START_POS                       "set_start_pos                 " ///< No description available.
#define CMD_STR_ENABLE                              "enable                        " ///< No description available.
#define CMD_STR_DISABLE                             "disable                       " ///< No description available.
/** @} */

/**
 * @name Motion Commands
 * @{
 */
#define CMD_STR_HOME                                "home                          " ///< No description available.
#define CMD_STR_MOVE_ABS                            "move_abs                      " ///< No description available.
#define CMD_STR_MOVE_INC                            "move_inc                      " ///< No description available.
#define CMD_STR_MOVE_TO_START                       "move_to_start                 " ///< No description available.
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
    CMD_SET_START_POS                       ///< @see CMD_STR_SET_START_POS,
    CMD_ENABLE                              ///< @see CMD_STR_ENABLE,
    CMD_DISABLE                             ///< @see CMD_STR_DISABLE,

    // Motion Commands
    CMD_HOME                                ///< @see CMD_STR_HOME,
    CMD_MOVE_ABS                            ///< @see CMD_STR_MOVE_ABS,
    CMD_MOVE_INC                            ///< @see CMD_STR_MOVE_INC,
    CMD_MOVE_TO_START                       ///< @see CMD_STR_MOVE_TO_START,
} Command;