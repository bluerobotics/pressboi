/**
 * @file commands.cpp
 * @brief Command parsing implementation for the Pressboi controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from commands.json on 2025-11-05 17:20:01
 * 
 * This file contains the command parser integrated into commands.cpp
 */

#include "commands.h"
#include <string.h>

//==================================================================================================
// Command Parser Implementation
//==================================================================================================

Command parseCommand(const char* cmdStr) {
    if (strncmp(cmdStr, CMD_STR_DISCOVER_DEVICE, strlen(CMD_STR_DISCOVER_DEVICE)) == 0) return CMD_DISCOVER_DEVICE;
    if (strncmp(cmdStr, CMD_STR_RESET, strlen(CMD_STR_RESET)) == 0) return CMD_RESET;
    if (strncmp(cmdStr, CMD_STR_HOME, strlen(CMD_STR_HOME)) == 0) return CMD_HOME;
    if (strncmp(cmdStr, CMD_STR_MOVE_ABS, strlen(CMD_STR_MOVE_ABS)) == 0) return CMD_MOVE_ABS;
    if (strncmp(cmdStr, CMD_STR_MOVE_INC, strlen(CMD_STR_MOVE_INC)) == 0) return CMD_MOVE_INC;
    if (strncmp(cmdStr, CMD_STR_SET_RETRACT, strlen(CMD_STR_SET_RETRACT)) == 0) return CMD_SET_RETRACT;
    if (strncmp(cmdStr, CMD_STR_RETRACT, strlen(CMD_STR_RETRACT)) == 0) return CMD_RETRACT;
    if (strncmp(cmdStr, CMD_STR_PAUSE, strlen(CMD_STR_PAUSE)) == 0) return CMD_PAUSE;
    if (strncmp(cmdStr, CMD_STR_RESUME, strlen(CMD_STR_RESUME)) == 0) return CMD_RESUME;
    if (strncmp(cmdStr, CMD_STR_CANCEL, strlen(CMD_STR_CANCEL)) == 0) return CMD_CANCEL;
    if (strncmp(cmdStr, CMD_STR_ENABLE, strlen(CMD_STR_ENABLE)) == 0) return CMD_ENABLE;
    if (strncmp(cmdStr, CMD_STR_DISABLE, strlen(CMD_STR_DISABLE)) == 0) return CMD_DISABLE;
    if (strncmp(cmdStr, CMD_STR_TEST_WATCHDOG, strlen(CMD_STR_TEST_WATCHDOG)) == 0) return CMD_TEST_WATCHDOG;
    return CMD_UNKNOWN;
}

const char* getCommandParams(const char* cmdStr, Command cmd) {
    switch (cmd) {
        case CMD_MOVE_ABS:
            return cmdStr + strlen(CMD_STR_MOVE_ABS);
        case CMD_MOVE_INC:
            return cmdStr + strlen(CMD_STR_MOVE_INC);
        case CMD_SET_RETRACT:
            return cmdStr + strlen(CMD_STR_SET_RETRACT);
        case CMD_RETRACT:
            return cmdStr + strlen(CMD_STR_RETRACT);
        default:
            return NULL;
    }
}