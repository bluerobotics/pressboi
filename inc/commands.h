/**
 * @file commands.h
 * @brief Defines the command interface for the Pressboi controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from commands.json on 2025-11-18 11:43:06
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
#define CMD_STR_RESET                               "reset" ///< Clear error states and reset system to standby.
#define CMD_STR_SET_FORCE_MODE                      "set_force_mode " ///< Sets the force sensing mode (persisted to NVM).
#define CMD_STR_SET_RETRACT                         "set_retract " ///< Sets the retract position for the press.
#define CMD_STR_RETRACT                             "retract" ///< Moves the press to the preset retract position with optional speed.
#define CMD_STR_PAUSE                               "pause" ///< Pauses any active move operation.
#define CMD_STR_RESUME                              "resume" ///< Resumes a paused move operation.
#define CMD_STR_CANCEL                              "cancel" ///< Cancels any active move operation and returns to standby.
#define CMD_STR_ENABLE                              "enable" ///< Enables power to the press motors.
#define CMD_STR_DISABLE                             "disable" ///< Disables power to the press motors.
#define CMD_STR_TEST_WATCHDOG                       "test_watchdog" ///< Test command that triggers the watchdog by blocking for 1 second.
#define CMD_STR_SET_FORCE_OFFSET                    "set_force_offset " ///< Set force calibration offset and save to NVM. Routes to load cell or motor torque calibration based on current force mode.
#define CMD_STR_SET_FORCE_ZERO                      "set_force_zero" ///< Adjusts the current force calibration offset so the present force reading becomes zero.
#define CMD_STR_SET_FORCE_SCALE                     "set_force_scale " ///< Set force sensor scale/linearity factor and save to non-volatile memory.
#define CMD_STR_SET_STRAIN_CAL                      "set_strain_cal " ///< Set machine strain energy compensation coefficients (force vs distance polynomial) and save to NVM.
#define CMD_STR_REBOOT_BOOTLOADER                   "reboot_bootloader" ///< Reboots the controller into ClearCore USB bootloader mode for firmware flashing.
#define CMD_STR_DUMP_NVM                            "dump_nvm" ///< Dump Pressboi non-volatile memory contents to the GUI.
#define CMD_STR_RESET_NVM                           "reset_nvm" ///< Restore Pressboi non-volatile memory to factory defaults.
/** @} */

/**
 * @name Motion Commands
 * @{
 */
#define CMD_STR_HOME                                "home" ///< Homes the press axis to its zero position.
#define CMD_STR_MOVE_ABS                            "move_abs " ///< Moves the press to an absolute position with speed and force limits.
#define CMD_STR_MOVE_INC                            "move_inc " ///< Moves the press by a relative distance with speed and force limits.
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
    CMD_SET_FORCE_MODE,                                    ///< @see CMD_STR_SET_FORCE_MODE
    CMD_SET_RETRACT,                                    ///< @see CMD_STR_SET_RETRACT
    CMD_RETRACT,                                    ///< @see CMD_STR_RETRACT
    CMD_PAUSE,                                    ///< @see CMD_STR_PAUSE
    CMD_RESUME,                                    ///< @see CMD_STR_RESUME
    CMD_CANCEL,                                    ///< @see CMD_STR_CANCEL
    CMD_ENABLE,                                    ///< @see CMD_STR_ENABLE
    CMD_DISABLE,                                    ///< @see CMD_STR_DISABLE
    CMD_TEST_WATCHDOG,                                    ///< @see CMD_STR_TEST_WATCHDOG
    CMD_SET_FORCE_OFFSET,                                    ///< @see CMD_STR_SET_FORCE_OFFSET
    CMD_SET_FORCE_ZERO,                                    ///< @see CMD_STR_SET_FORCE_ZERO
    CMD_SET_FORCE_SCALE,                                    ///< @see CMD_STR_SET_FORCE_SCALE
    CMD_SET_STRAIN_CAL,                                    ///< @see CMD_STR_SET_STRAIN_CAL
    CMD_REBOOT_BOOTLOADER,                                    ///< @see CMD_STR_REBOOT_BOOTLOADER
    CMD_DUMP_NVM,                                    ///< @see CMD_STR_DUMP_NVM
    CMD_RESET_NVM,                                    ///< @see CMD_STR_RESET_NVM

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