/**
 * @file commands.h
 * @author Eldin Miller-Stead
 * @date September 10, 2025
 * @brief Defines the command interface for the Fillhead controller.
 *
 * @details This header file centralizes the entire command vocabulary for the Fillhead system.
 * It defines all raw command strings sent from the GUI, maps them to a comprehensive
 * `Command` enum for structured handling, and specifies the prefixes used for telemetry
 * and status messages. This approach decouples the command parsing logic from the main
 * application and ensures a single source of truth for the communication protocol.
 */
#pragma once

//==================================================================================================
// Command Strings & Prefixes
//==================================================================================================

/**
 * @name General System Commands
 * @{
 */
#define CMD_STR_DISCOVER_DEVICE             "DISCOVER_DEVICE"          ///< Generic command for any device to respond to.
#define CMD_STR_ENABLE                      "ENABLE"                   ///< Command to enable all motors.
#define CMD_STR_DISABLE                     "DISABLE"                  ///< Command to disable all motors.
#define CMD_STR_ABORT                       "ABORT"                    ///< Command to halt all ongoing operations.
#define CMD_STR_CLEAR_ERRORS                "CLEAR_ERRORS"             ///< Command to clear any existing fault states.
/** @} */

/**
 * @name Injector Motion Commands
 * @{
 */
#define CMD_STR_INJECT_STATOR           "INJECT_STATOR "            ///< Command to dispense a specific volume using the stator (5:1) configuration.
#define CMD_STR_INJECT_ROTOR            "INJECT_ROTOR "             ///< Command to dispense a specific volume using the rotor (1:1) configuration.
#define CMD_STR_JOG_MOVE                "JOG_MOVE "                 ///< Command to jog the injector motors by a relative distance.
#define CMD_STR_MACHINE_HOME_MOVE       "MACHINE_HOME_MOVE"         ///< Command to home the main machine axis.
#define CMD_STR_CARTRIDGE_HOME_MOVE     "CARTRIDGE_HOME_MOVE"       ///< Command to home the injector against the cartridge.
#define CMD_STR_MOVE_TO_CARTRIDGE_HOME      "MOVE_TO_CARTRIDGE_HOME"   ///< Command to move the injector to the cartridge home position.
#define CMD_STR_MOVE_TO_CARTRIDGE_RETRACT   "MOVE_TO_CARTRIDGE_RETRACT "///< Command to retract the injector a specified distance from cartridge home.
#define CMD_STR_PAUSE_INJECTION             "PAUSE_INJECTION"          ///< Command to pause an ongoing injection.
#define CMD_STR_RESUME_INJECTION            "RESUME_INJECTION"         ///< Command to resume a paused injection.
#define CMD_STR_CANCEL_INJECTION            "CANCEL_INJECTION"         ///< Command to cancel an ongoing injection.
/** @} */

/**
 * @name Pinch Valve Commands
 * @{
 */
#define CMD_STR_INJECTION_VALVE_HOME_UNTUBED "INJECTION_VALVE_HOME_UNTUBED" ///< Command to home the injection valve without tubing.
#define CMD_STR_INJECTION_VALVE_HOME_TUBED "INJECTION_VALVE_HOME_TUBED"   ///< Command to home the injection valve with tubing installed.
#define CMD_STR_INJECTION_VALVE_OPEN        "INJECTION_VALVE_OPEN"         ///< Command to open the injection valve.
#define CMD_STR_INJECTION_VALVE_CLOSE       "INJECTION_VALVE_CLOSE"        ///< Command to close the injection valve.
#define CMD_STR_INJECTION_VALVE_JOG         "INJECTION_VALVE_JOG "         ///< Command to jog the injection valve motor.
#define CMD_STR_VACUUM_VALVE_HOME_UNTUBED   "VACUUM_VALVE_HOME_UNTUBED"    ///< Command to home the vacuum valve without tubing.
#define CMD_STR_VACUUM_VALVE_HOME_TUBED     "VACUUM_VALVE_HOME_TUBED"      ///< Command to home the vacuum valve with tubing installed.
#define CMD_STR_VACUUM_VALVE_OPEN           "VACUUM_VALVE_OPEN"            ///< Command to open the vacuum valve.
#define CMD_STR_VACUUM_VALVE_CLOSE          "VACUUM_VALVE_CLOSE"           ///< Command to close the vacuum valve.
#define CMD_STR_VACUUM_VALVE_JOG            "VACUUM_VALVE_JOG "            ///< Command to jog the vacuum valve motor.
/** @} */

/**
 * @name Heater Commands
 * @{
 */
#define CMD_STR_HEATER_ON                   "HEATER_ON"                ///< Command to turn the heater on.
#define CMD_STR_HEATER_OFF                  "HEATER_OFF"               ///< Command to turn the heater off.
#define CMD_STR_SET_HEATER_GAINS            "SET_HEATER_GAINS "        ///< Command to set the heater's PID gains.
#define CMD_STR_SET_HEATER_SETPOINT         "SET_HEATER_SETPOINT "     ///< Command to set the heater's target temperature.
/** @} */

/**
 * @name Vacuum Commands
 * @{
 */
#define CMD_STR_VACUUM_ON                   "VACUUM_ON"                ///< Command to turn the vacuum pump on.
#define CMD_STR_VACUUM_OFF                  "VACUUM_OFF"               ///< Command to turn the vacuum pump off.
#define CMD_STR_VACUUM_LEAK_TEST            "VACUUM_LEAK_TEST"         ///< Command to initiate a vacuum leak test.
#define CMD_STR_SET_VACUUM_TARGET           "SET_VACUUM_TARGET "       ///< Command to set the target vacuum level.
#define CMD_STR_SET_VACUUM_TIMEOUT_S        "SET_VACUUM_TIMEOUT_S "    ///< Command to set the timeout for reaching the vacuum target.
#define CMD_STR_SET_LEAK_TEST_DELTA         "SET_LEAK_TEST_DELTA "     ///< Command to set the pressure delta for the leak test.
#define CMD_STR_SET_LEAK_TEST_DURATION_S    "SET_LEAK_TEST_DURATION_S "///< Command to set the duration for the leak test.
/** @} */

/**
 * @name Telemetry and Status Prefixes
 * @{
 */
#define TELEM_PREFIX						"FILLHEAD_TELEM: "         ///< Prefix for all telemetry messages.
#define STATUS_PREFIX_INFO                  "FILLHEAD_INFO: "          ///< Prefix for informational status messages.
#define STATUS_PREFIX_START                 "FILLHEAD_START: "         ///< Prefix for messages indicating the start of an operation.
#define STATUS_PREFIX_DONE                  "FILLHEAD_DONE: "          ///< Prefix for messages indicating the successful completion of an operation.
#define STATUS_PREFIX_ERROR                 "FILLHEAD_ERROR: "         ///< Prefix for messages indicating an error or fault.
#define STATUS_PREFIX_DISCOVERY             "DISCOVERY_RESPONSE: "     ///< Prefix for the device discovery response.
/** @} */

/**
 * @enum Command
 * @brief Enumerates all possible commands that can be processed by the Fillhead.
 * @details This enum provides a type-safe way to handle incoming commands. The
 * `CommsController` parses raw command strings into these enum values, which
 * are then used in switch statements for command dispatch.
 */
typedef enum {
	CMD_UNKNOWN,                        ///< Represents an unrecognized or invalid command.

	// General Commands
	CMD_ENABLE,                         ///< @see CMD_STR_ENABLE
	CMD_DISABLE,                        ///< @see CMD_STR_DISABLE
	CMD_DISCOVER_DEVICE,                ///< @see CMD_STR_DISCOVER_DEVICE
	CMD_ABORT,                          ///< @see CMD_STR_ABORT
	CMD_CLEAR_ERRORS,                   ///< @see CMD_STR_CLEAR_ERRORS

	// Injector Commands
	CMD_INJECT_STATOR,                  ///< @see CMD_STR_INJECT_STATOR
	CMD_INJECT_ROTOR,                   ///< @see CMD_STR_INJECT_ROTOR
	CMD_JOG_MOVE,                       ///< @see CMD_STR_JOG_MOVE
	CMD_MACHINE_HOME_MOVE,              ///< @see CMD_STR_MACHINE_HOME_MOVE
	CMD_CARTRIDGE_HOME_MOVE,            ///< @see CMD_STR_CARTRIDGE_HOME_MOVE
	CMD_MOVE_TO_CARTRIDGE_HOME,         ///< @see CMD_STR_MOVE_TO_CARTRIDGE_HOME
	CMD_MOVE_TO_CARTRIDGE_RETRACT,      ///< @see CMD_STR_MOVE_TO_CARTRIDGE_RETRACT
	CMD_PAUSE_INJECTION,                ///< @see CMD_STR_PAUSE_INJECTION
	CMD_RESUME_INJECTION,               ///< @see CMD_STR_RESUME_INJECTION
	CMD_CANCEL_INJECTION,               ///< @see CMD_STR_CANCEL_INJECTION

	// Vacuum System Commands
	CMD_VACUUM_ON,                      ///< @see CMD_STR_VACUUM_ON
	CMD_VACUUM_OFF,                     ///< @see CMD_STR_VACUUM_OFF
	CMD_VACUUM_LEAK_TEST,               ///< @see CMD_STR_VACUUM_LEAK_TEST
	CMD_SET_VACUUM_TARGET,              ///< @see CMD_STR_SET_VACUUM_TARGET
	CMD_SET_VACUUM_TIMEOUT_S,           ///< @see CMD_STR_SET_VACUUM_TIMEOUT_S
	CMD_SET_LEAK_TEST_DELTA,            ///< @see CMD_STR_SET_LEAK_TEST_DELTA
	CMD_SET_LEAK_TEST_DURATION_S,       ///< @see CMD_STR_SET_LEAK_TEST_DURATION_S

	// Heater Commands
	CMD_HEATER_ON,                      ///< @see CMD_STR_HEATER_ON
	CMD_HEATER_OFF,                     ///< @see CMD_STR_HEATER_OFF
	CMD_SET_HEATER_GAINS,               ///< @see CMD_STR_SET_HEATER_GAINS
	CMD_SET_HEATER_SETPOINT,            ///< @see CMD_STR_SET_HEATER_SETPOINT

	// Pinch Valve Commands
	CMD_INJECTION_VALVE_HOME_UNTUBED,   ///< @see CMD_STR_INJECTION_VALVE_HOME_UNTUBED
	CMD_INJECTION_VALVE_HOME_TUBED,     ///< @see CMD_STR_INJECTION_VALVE_HOME_TUBED
	CMD_INJECTION_VALVE_OPEN,           ///< @see CMD_STR_INJECTION_VALVE_OPEN
	CMD_INJECTION_VALVE_CLOSE,          ///< @see CMD_STR_INJECTION_VALVE_CLOSE
	CMD_INJECTION_VALVE_JOG,            ///< @see CMD_STR_INJECTION_VALVE_JOG
	CMD_VACUUM_VALVE_HOME_UNTUBED,      ///< @see CMD_STR_VACUUM_VALVE_HOME_UNTUBED
	CMD_VACUUM_VALVE_HOME_TUBED,        ///< @see CMD_STR_VACUUM_VALVE_HOME_TUBED
	CMD_VACUUM_VALVE_OPEN,              ///< @see CMD_STR_VACUUM_VALVE_OPEN
	CMD_VACUUM_VALVE_CLOSE,             ///< @see CMD_STR_VACUUM_VALVE_CLOSE
	CMD_VACUUM_VALVE_JOG                ///< @see CMD_STR_VACUUM_VALVE_JOG
} Command;
