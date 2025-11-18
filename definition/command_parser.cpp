/**
 * @file command_parser.cpp
 * @brief Command parsing and dispatching implementations for the Pressboi controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from commands.json on 2025-11-03 11:25:17
 */

#include "command_parser.h"
#include <string.h>

//==================================================================================================
// Command Parser Implementation
//==================================================================================================

Command parseCommand(const char* cmdStr) {
    if (strncmp(cmdStr, CMD_STR_HOME, strlen(CMD_STR_HOME)) == 0) return CMD_HOME;
    if (strncmp(cmdStr, CMD_STR_MOVE_ABS, strlen(CMD_STR_MOVE_ABS)) == 0) return CMD_MOVE_ABS;
    if (strncmp(cmdStr, CMD_STR_MOVE_INC, strlen(CMD_STR_MOVE_INC)) == 0) return CMD_MOVE_INC;
    if (strncmp(cmdStr, CMD_STR_SET_START_POS, strlen(CMD_STR_SET_START_POS)) == 0) return CMD_SET_START_POS;
    if (strncmp(cmdStr, CMD_STR_MOVE_TO_START, strlen(CMD_STR_MOVE_TO_START)) == 0) return CMD_MOVE_TO_START;
    if (strncmp(cmdStr, CMD_STR_ENABLE, strlen(CMD_STR_ENABLE)) == 0) return CMD_ENABLE;
    if (strncmp(cmdStr, CMD_STR_DISABLE, strlen(CMD_STR_DISABLE)) == 0) return CMD_DISABLE;
    return CMD_UNKNOWN;
}

const char* getCommandParams(const char* cmdStr, Command cmd) {
    switch (cmd) {
        case CMD_MOVE_ABS:
            return cmdStr + strlen(CMD_STR_MOVE_ABS);
        case CMD_MOVE_INC:
            return cmdStr + strlen(CMD_STR_MOVE_INC);
        case CMD_SET_START_POS:
            return cmdStr + strlen(CMD_STR_SET_START_POS);
        default:
            return NULL;
    }
}

//==================================================================================================
// Command Dispatcher Template
//==================================================================================================

bool dispatchCommand(Command cmd, const char* params) {
    switch (cmd) {
        case CMD_HOME:
            // TODO: Implement handler
            // handle_home();
            return true;

        case CMD_MOVE_ABS:
            // TODO: Implement handler with parameters
            // handle_move_abs(params);
            return true;

        case CMD_MOVE_INC:
            // TODO: Implement handler with parameters
            // handle_move_inc(params);
            return true;

        case CMD_SET_START_POS:
            // TODO: Implement handler with parameters
            // handle_set_start_pos(params);
            return true;

        case CMD_MOVE_TO_START:
            // TODO: Implement handler
            // handle_move_to_start();
            return true;

        case CMD_ENABLE:
            // TODO: Implement handler
            // handle_enable();
            return true;

        case CMD_DISABLE:
            // TODO: Implement handler
            // handle_disable();
            return true;

        case CMD_UNKNOWN:
        default:
            return false;
    }
}