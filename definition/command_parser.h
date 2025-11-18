/**
 * @file command_parser.h
 * @brief Command parsing and dispatching declarations for the Pressboi controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from commands.json on 2025-11-03 11:25:17
 * 
 * This header declares utility functions to parse and dispatch commands for the Pressboi.
 * @see commands.h for command definitions
 * @see command_parser.cpp for implementations
 */
#pragma once

#include "commands.h"

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

/**
 * @brief Dispatch a parsed command to its handler (template - implement your handlers).
 * @param cmd The parsed command enum
 * @param params The parameter string (if any)
 * @return true if command was handled successfully, false otherwise
 * 
 * @note This is a template. Implement your actual command handlers and call them here.
 */
bool dispatchCommand(Command cmd, const char* params);