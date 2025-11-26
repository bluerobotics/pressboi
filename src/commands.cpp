/**
 * @file commands.cpp
 * @brief Command parsing implementation for the Pressboi controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from commands.json on 2025-11-18 11:43:06
 * 
 * This file contains the command parser integrated into commands.cpp
 */

#include "commands.h"
#include <string.h>

//==================================================================================================
// Command Parser Implementation
//==================================================================================================

Command parseCommand(const char* cmdStr) {
    // IMPORTANT: Check longer commands BEFORE shorter ones that are prefixes!
    // e.g., check "reset_nvm" before "reset", "set_retract" before "retract", etc.
    
    if (strncmp(cmdStr, CMD_STR_DISCOVER_DEVICE, strlen(CMD_STR_DISCOVER_DEVICE)) == 0) return CMD_DISCOVER_DEVICE;
    if (strncmp(cmdStr, CMD_STR_REBOOT_BOOTLOADER, strlen(CMD_STR_REBOOT_BOOTLOADER)) == 0) return CMD_REBOOT_BOOTLOADER;
    if (strncmp(cmdStr, CMD_STR_TEST_WATCHDOG, strlen(CMD_STR_TEST_WATCHDOG)) == 0) return CMD_TEST_WATCHDOG;
    if (strncmp(cmdStr, CMD_STR_SET_FORCE_OFFSET, strlen(CMD_STR_SET_FORCE_OFFSET)) == 0) return CMD_SET_FORCE_OFFSET;
    if (strncmp(cmdStr, CMD_STR_SET_FORCE_ZERO, strlen(CMD_STR_SET_FORCE_ZERO)) == 0) return CMD_SET_FORCE_ZERO;
    if (strncmp(cmdStr, CMD_STR_SET_FORCE_SCALE, strlen(CMD_STR_SET_FORCE_SCALE)) == 0) return CMD_SET_FORCE_SCALE;
    if (strncmp(cmdStr, CMD_STR_SET_FORCE_MODE, strlen(CMD_STR_SET_FORCE_MODE)) == 0) return CMD_SET_FORCE_MODE;
    if (strncmp(cmdStr, CMD_STR_SET_STRAIN_CAL, strlen(CMD_STR_SET_STRAIN_CAL)) == 0) return CMD_SET_STRAIN_CAL;
    if (strncmp(cmdStr, CMD_STR_SET_POLARITY, strlen(CMD_STR_SET_POLARITY)) == 0) return CMD_SET_POLARITY;
    if (strncmp(cmdStr, CMD_STR_HOME_ON_BOOT, strlen(CMD_STR_HOME_ON_BOOT)) == 0) return CMD_HOME_ON_BOOT;
    if (strncmp(cmdStr, CMD_STR_SET_RETRACT, strlen(CMD_STR_SET_RETRACT)) == 0) return CMD_SET_RETRACT;
    if (strncmp(cmdStr, CMD_STR_DUMP_ERROR_LOG, strlen(CMD_STR_DUMP_ERROR_LOG)) == 0) return CMD_DUMP_ERROR_LOG;
    if (strncmp(cmdStr, CMD_STR_RESET_NVM, strlen(CMD_STR_RESET_NVM)) == 0) return CMD_RESET_NVM;
    if (strncmp(cmdStr, CMD_STR_DUMP_NVM, strlen(CMD_STR_DUMP_NVM)) == 0) return CMD_DUMP_NVM;
    if (strncmp(cmdStr, CMD_STR_MOVE_ABS, strlen(CMD_STR_MOVE_ABS)) == 0) return CMD_MOVE_ABS;
    if (strncmp(cmdStr, CMD_STR_MOVE_INC, strlen(CMD_STR_MOVE_INC)) == 0) return CMD_MOVE_INC;
    if (strncmp(cmdStr, CMD_STR_RETRACT, strlen(CMD_STR_RETRACT)) == 0) return CMD_RETRACT;
    if (strncmp(cmdStr, CMD_STR_RESET, strlen(CMD_STR_RESET)) == 0) return CMD_RESET;
    if (strncmp(cmdStr, CMD_STR_HOME, strlen(CMD_STR_HOME)) == 0) return CMD_HOME;
    if (strncmp(cmdStr, CMD_STR_PAUSE, strlen(CMD_STR_PAUSE)) == 0) return CMD_PAUSE;
    if (strncmp(cmdStr, CMD_STR_RESUME, strlen(CMD_STR_RESUME)) == 0) return CMD_RESUME;
    if (strncmp(cmdStr, CMD_STR_CANCEL, strlen(CMD_STR_CANCEL)) == 0) return CMD_CANCEL;
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
        case CMD_SET_FORCE_MODE:
            return cmdStr + strlen(CMD_STR_SET_FORCE_MODE);
        case CMD_SET_RETRACT:
            return cmdStr + strlen(CMD_STR_SET_RETRACT);
        case CMD_RETRACT:
            return cmdStr + strlen(CMD_STR_RETRACT);
        case CMD_SET_FORCE_OFFSET:
            return cmdStr + strlen(CMD_STR_SET_FORCE_OFFSET);
        case CMD_SET_FORCE_SCALE:
            return cmdStr + strlen(CMD_STR_SET_FORCE_SCALE);
        case CMD_SET_STRAIN_CAL:
            return cmdStr + strlen(CMD_STR_SET_STRAIN_CAL);
        case CMD_SET_POLARITY:
            return cmdStr + strlen(CMD_STR_SET_POLARITY);
        case CMD_HOME_ON_BOOT:
            return cmdStr + strlen(CMD_STR_HOME_ON_BOOT);
        default:
            return NULL;
    }
}