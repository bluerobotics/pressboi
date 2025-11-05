<div align="center">

<img src="assets/icon.png" alt="Pressboi" width="150">

# Pressboi

**Dual-Motor Press Control System**

[![release](https://img.shields.io/github/v/release/bluerobotics/pressboi?style=flat-square)](https://github.com/bluerobotics/pressboi/releases/latest)
[![build](https://img.shields.io/github/actions/workflow/status/bluerobotics/pressboi/release.yml?style=flat-square)](https://github.com/bluerobotics/pressboi/actions)
[![downloads](https://img.shields.io/github/downloads/bluerobotics/pressboi/total?style=flat-square)](https://github.com/bluerobotics/pressboi/releases)
[![license: MIT](https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square)](LICENSE)

[View Changelog](CHANGELOG.md) • [Download Latest Release](https://github.com/bluerobotics/pressboi/releases/latest)

</div>

---

## Overview

Firmware for the Pressboi dual-motor press control system, built on the Teknic ClearCore platform. This firmware provides precision control for press operations with real-time force monitoring, synchronized dual-motor movement, and network/USB communication.

**Designed to work with:** [BR Equipment Control App](https://github.com/bluerobotics/br-equipment-control-app) - A Python/Tkinter GUI for controlling and monitoring the Pressboi and other Blue Robotics manufacturing equipment.

## Features

- **Dual-motor synchronized control** - Two motors on a single axis for press operations
- **UDP/Ethernet communication** - Network-based control with device discovery
- **USB Serial fallback** - Direct USB communication support
- **Command-based control** - Simple text-based command protocol
- **Real-time telemetry** - Position, force, torque, and status reporting
- **Pause/resume/cancel** - Interrupt and resume operations safely
- **Force monitoring** - Real-time force feedback with configurable limits
- **Auto-generated protocol** - Commands and telemetry from JSON schemas

---

## Building

### Requirements

- **Atmel Studio 7** (Windows) or compatible ARM GCC toolchain
- **Teknic ClearCore libraries** (included as submodules/dependencies)
  - `libClearCore`
  - `LwIP` (Lightweight IP stack)

### Build Instructions

1. Open `pressboi/pressboi.atsln` in Atmel Studio
2. Select **Debug** configuration
3. Build the project (F7)
4. Output files will be in `pressboi/Debug/`:
   - `pressboi.elf` - Executable with debug symbols
   - `pressboi.bin` - Raw binary
   - `pressboi.uf2` - UF2 bootloader format (recommended)

### Flashing

#### **Method 1: Microchip Studio with Atmel ICE (Recommended for Development)**

This is the recommended method for development and debugging.

**Requirements:**
- [Microchip Studio](https://www.microchip.com/en-us/tools-resources/develop/microchip-studio) (formerly Atmel Studio 7)
- Atmel ICE debugger/programmer
- 10-pin SWD cable

**Steps:**
1. Connect Atmel ICE to your computer via USB
2. Connect Atmel ICE to ClearCore SWD header (10-pin connector)
3. Power on the ClearCore
4. In Microchip Studio, open `pressboi/pressboi.atsln`
5. Select **Debug** configuration
6. Go to **Tools → Device Programming** (Ctrl+Shift+P)
7. Configure:
   - **Tool:** Atmel-ICE
   - **Device:** ATSAME53N19A
   - **Interface:** SWD
   - Click **Apply**
8. Click **Read** under Device Signature to verify connection
9. Go to **Memories** tab
10. Under **Flash**, browse to `pressboi/Debug/pressboi.elf`
11. Click **Program** to flash the firmware
12. Optionally check **Verify** to confirm successful programming

**For Development with Debugging:**
- Press **F5** or click **Start Debugging and Break** to build, flash, and start a debug session
- Use breakpoints, watch variables, and step through code
- Serial Wire Viewer (SWV) available for printf debugging

---

#### **Method 2: Microchip Studio with USB (BOSSA Bootloader)**

If the ClearCore has the BOSSA bootloader installed:

1. Double-tap reset button on ClearCore (LED will pulse)
2. ClearCore appears as COM port in Device Manager
3. In Microchip Studio:
   - Go to **Tools → External Tools**
   - Add new tool:
     - **Title:** Flash via BOSSA
     - **Command:** `C:\Program Files (x86)\Teknic\ClearCore-library\Teknic\tools\bossac.exe`
     - **Arguments:** `--port=COM# -e -w -v -R "$(ProjectDir)Debug\$(TargetName).bin"`
     - Replace `COM#` with your ClearCore's COM port
4. Build project (F7)
5. Run **Tools → Flash via BOSSA**

---

#### **Method 3: UF2 Bootloader (Quick Field Updates)**

For quick firmware updates without development tools:

1. Double-press the reset button on the ClearCore
2. ClearCore appears as USB mass storage device
3. Copy `pressboi.uf2` to the drive
4. ClearCore automatically reboots with new firmware

---

#### **Method 4: BOSSA Command Line**

For automated or scripted flashing:

```bash
cd Tools
flash_clearcore.cmd
```

Or manually:
```bash
bossac.exe --port=COM# -e -w -v -R pressboi.bin
```

---

## Communication Protocol

### Discovery
- **Broadcast:** `DISCOVER_DEVICE PORT=<gui_port>` on UDP port 8888
- **Response:** `DISCOVERY_RESPONSE: DEVICE_ID=pressboi PORT=8888`

### Commands
All commands are plain text over UDP or USB Serial:

| Command | Parameters | Description |
|---------|-----------|-------------|
| `home` | - | Home both motors to zero position |
| `move_abs` | `pos=<mm>` | Move to absolute position |
| `move_inc` | `dist=<mm>` | Move incremental distance |
| `set_start_pos` | - | Set current position as start |
| `move_to_start` | - | Return to start position |
| `enable` | - | Enable motor drivers |
| `disable` | - | Disable motor drivers |
| `pause` | - | Pause current operation |
| `resume` | - | Resume paused operation |
| `cancel` | - | Cancel current operation |

### Telemetry
Sent periodically (100ms intervals) with prefix `TELEM_DATA:`:
- `MAIN_STATE` - Current state (IDLE, HOMING, MOVING, ERROR, etc.)
- `force` - Current force reading
- `force_limit` - Configured force limit
- `current_pos` - Current position (mm)
- `target_pos` - Target position (mm)
- `start_pos` - Stored start position
- `torque_m1`, `torque_m2` - Motor torques
- `enabled0`, `enabled1` - Motor enable states
- `homed` - Homing status

---

## Architecture

### Class Structure

The firmware follows an object-oriented design with clear separation of concerns:

```
Pressboi (Main Controller)
├── CommsController (Network/Serial Communication)
└── MotorController (Dual-Motor Press Control)
```

---

## Class Reference

### `Pressboi` - Main Controller

**File:** `pressboi.h` / `pressboi.cpp`

The master orchestrator for the entire press system. Manages system state, command dispatch, and coordinates all sub-controllers.

#### States

**MainState** - Top-level system state:
- `STATE_STANDBY` - Idle and ready
- `STATE_BUSY` - Operation in progress
- `STATE_ERROR` - Fault occurred
- `STATE_DISABLED` - Motors disabled
- `STATE_CLEARING_ERRORS` - Recovering from error

**ErrorState** - Specific error conditions:
- `ERROR_NONE` - No error
- `ERROR_MANUAL_ABORT` - User aborted
- `ERROR_TORQUE_ABORT` - Torque limit exceeded
- `ERROR_MOTION_EXCEEDED_ABORT` - Motion limit exceeded
- `ERROR_NO_HOME` - Home position not set
- `ERROR_HOMING_TIMEOUT` - Homing took too long
- `ERROR_HOMING_NO_TORQUE_RAPID` - Homing failed (rapid phase)
- `ERROR_HOMING_NO_TORQUE_TOUCH` - Homing failed (touch phase)
- `ERROR_NOT_HOMED` - Operation requires homing first
- `ERROR_INVALID_PARAMETERS` - Invalid command parameters
- `ERROR_MOTORS_DISABLED` - Motors must be enabled first

#### Public Methods

| Method | Description |
|--------|-------------|
| `Pressboi()` | Constructor - initializes all member variables |
| `void setup()` | Initialize hardware and controllers (call once at startup) |
| `void loop()` | Main execution loop (call continuously from main()) |
| `void reportEvent(const char* statusType, const char* message)` | Send status message to GUI |

#### Private Methods

| Method | Description |
|--------|-------------|
| `void updateState()` | Update system state and sub-controller state machines |
| `void dispatchCommand(const Message& msg)` | Route incoming commands to appropriate handlers |
| `void publishTelemetry()` | Aggregate and send telemetry from all sub-controllers |
| `void enable()` | Enable all motors and ready the system |
| `void disable()` | Disable all motors and stop operations |
| `void abort()` | Emergency stop - halt all motion immediately |
| `void clearErrors()` | Reset error states and clear motor faults |
| `void standby()` | Reset all controllers to idle state |

---

### `MotorController` - Dual-Motor Press Control

**File:** `motor_controller.h` / `motor_controller.cpp`

Manages the two ganged motors (M0 and M1) that drive the press mechanism. Implements complex state machines for homing and precision moves with torque-based sensing.

#### States

**HomingState** - Active homing operation:
- `HOMING_NONE` - No homing active
- `HOMING_MACHINE` - Homing to physical zero (fully retracted)
- `HOMING_CARTRIDGE` - Homing to material cartridge front

**HomingPhase** - Detailed homing sub-states:
- `HOMING_PHASE_IDLE_GLOBAL` - Not homing
- `HOMING_PHASE_STARTING_MOVE` - Confirming motion start
- `HOMING_PHASE_RAPID_MOVE` - High-speed approach to hard stop
- `HOMING_PHASE_BACK_OFF` - Backing away from hard stop
- `HOMING_PHASE_TOUCH_OFF` - Slow precision contact
- `HOMING_PHASE_RETRACT` - Moving to safe position
- `HOMING_PHASE_COMPLETE` - Homing successful
- `HOMING_PHASE_ERROR_GLOBAL` - Homing failed

**FeedState** - Material feed/injection state:
- `FEED_NONE` - No feed operation
- `FEED_STANDBY` - Ready to start
- `FEED_INJECT_STARTING` - Starting injection
- `FEED_INJECT_ACTIVE` - Injection in progress
- `FEED_INJECT_PAUSED` - User paused
- `FEED_INJECT_RESUMING` - Resuming from pause
- `FEED_MOVING_TO_HOME` - Moving to home position
- `FEED_MOVING_TO_RETRACT` - Moving to retract position
- `FEED_INJECTION_CANCELLED` - User cancelled
- `FEED_INJECTION_COMPLETED` - Injection finished

#### Public Methods

| Method | Description |
|--------|-------------|
| `MotorController(MotorDriver* motorA, MotorDriver* motorB, Pressboi* controller)` | Constructor - takes pointers to motor drivers and main controller |
| `void setup()` | Initialize motors and configure parameters |
| `void updateState()` | Update state machines (call repeatedly in main loop) |
| `void handleCommand(Command cmd, const char* args)` | Execute motor-related commands |
| `void updateTelemetry(TelemetryData* data)` | Populate telemetry structure with current state |
| `void enable()` | Enable motor drivers |
| `void disable()` | Disable motor drivers |
| `void abort()` | Emergency stop - halt motion immediately |
| `void clearErrors()` | Clear motor faults and error states |
| `void standby()` | Return to idle state |
| `bool isBusy()` | Check if any operation is in progress |
| `bool hasFault()` | Check if any motor has a fault |
| `void home()` | Execute homing routine |
| `void setStartPosition()` | Store current position as start |
| `void moveAbsolute(float position_mm)` | Move to absolute position |
| `void moveIncremental(float distance_mm)` | Move relative distance |
| `void moveToStart()` | Return to stored start position |
| `void pauseOperation()` | Pause current move (homing or feeding) |
| `void resumeOperation()` | Resume paused operation |
| `void cancelOperation()` | Cancel current operation |

#### Private Methods

| Method | Description |
|--------|-------------|
| `void updateHomingStateMachine()` | Manage homing sequence state transitions |
| `void updateFeedStateMachine()` | Manage feeding/injection state transitions |
| `void reportEvent(const char* prefix, const char* message)` | Send motor event to GUI |
| `float getAverageTorque()` | Calculate average torque across both motors |
| `bool motorsMoving()` | Check if motors are currently in motion |
| `void setTorqueLimit(float percent)` | Set motor torque limits |
| `void syncMotorPositions()` | Zero position counters on both motors |

---

### `CommsController` - Network/Serial Communication

**File:** `comms_controller.h` / `comms_controller.cpp`

Handles all communication tasks including UDP/Ethernet and USB Serial. Uses circular message queues for non-blocking operation.

#### Data Structures

**Message** - Communication packet:
```cpp
struct Message {
    char buffer[MAX_MESSAGE_LENGTH];  // Raw message payload
    IpAddress remoteIp;               // Sender/recipient IP
    uint16_t remotePort;              // Sender/recipient port
};
```

#### Public Methods

| Method | Description |
|--------|-------------|
| `CommsController()` | Constructor - initialize queues and state |
| `void setup()` | Initialize Ethernet and USB Serial hardware |
| `void update()` | Process incoming packets and outgoing queue (call in main loop) |
| `bool enqueueRx(const char* msg, const IpAddress& ip, uint16_t port)` | Add received message to RX queue |
| `bool dequeueRx(Message& msg)` | Retrieve oldest message from RX queue |
| `bool enqueueTx(const char* msg, const IpAddress& ip, uint16_t port)` | Add message to TX queue for sending |
| `void reportEvent(const char* statusType, const char* message)` | Send formatted status/event message to GUI |
| `bool isGuiDiscovered() const` | Check if GUI has been discovered |
| `IpAddress getGuiIp() const` | Get discovered GUI IP address |
| `uint16_t getGuiPort() const` | Get discovered GUI port |
| `void setGuiDiscovered(bool discovered)` | Set GUI discovery state |
| `void setGuiIp(const IpAddress& ip)` | Set GUI IP address |
| `void setGuiPort(uint16_t port)` | Set GUI port |

#### Private Methods

| Method | Description |
|--------|-------------|
| `void processUdp()` | Read incoming UDP packets and enqueue |
| `void processTxQueue()` | Dequeue and send outgoing messages |
| `void setupEthernet()` | Initialize Ethernet with DHCP and start UDP listener |
| `void setupUsbSerial()` | Configure USB port as CDC serial device |

---

## Auto-Generated Protocol Files

The following files are auto-generated from JSON schemas in the [BR Equipment Control App](https://github.com/bluerobotics/br-equipment-control-app):

### `commands.h` / `commands.cpp`

**Auto-generated from:** `br-equipment-control-app/devices/pressboi/commands.json`

Contains:
- Command string definitions (`CMD_STR_*`)
- Command enum (`Command`)
- `parseCommand(const char* cmdStr)` - Parse command string to enum
- `getCommandParams(const char* cmdStr, Command cmd)` - Extract parameters

### `variables.h` / `variables.cpp`

**Auto-generated from:** `br-equipment-control-app/devices/pressboi/telemetry.json`

Contains:
- `TelemetryData` struct with all telemetry fields
- `telemetry_init(TelemetryData* data)` - Initialize telemetry structure
- `telemetry_build_message(TelemetryData* data, char* buffer, size_t size)` - Build telemetry message string
- `telemetry_send(TelemetryData* data)` - Send telemetry to GUI

### `events.h` / `events.cpp`

**Auto-generated from:** `br-equipment-control-app/devices/pressboi/events.json`

Contains:
- Event string definitions (`EVENT_STR_*`)
- Event enum (`Event`)
- `sendEvent(Event event)` - Send simple event
- `sendEventInt(Event event, int32_t param)` - Send event with integer parameter
- `sendEventString(Event event, const char* param)` - Send event with string parameter
- `sendEventMulti(Event event, int32_t param1, int32_t param2)` - Send event with multiple parameters

### Regenerating Protocol Files

1. Edit JSON files in `br-equipment-control-app/devices/pressboi/`:
   - `commands.json` - Command definitions
   - `telemetry.json` - Variable definitions
   - `events.json` - Event definitions

2. Run code generator:
   ```bash
   cd br-equipment-control-app
   python generate_all_headers.py
   ```

3. Copy generated files from `devices/pressboi/generated/` to firmware folders:
   ```bash
   # Headers
   cp devices/pressboi/generated/*.h ../pressboi/pressboi/inc/
   # Implementation
   cp devices/pressboi/generated/*.cpp ../pressboi/pressboi/src/
   ```

---

## Configuration

Edit `inc/config.h` to customize:

### Motor Parameters
- `INJECTOR_PITCH_MM_PER_REV` - Lead screw pitch
- `STEPS_PER_MM_INJECTOR` - Steps per millimeter
- `INJECTOR_DEFAULT_VEL_MAX_SPS` - Maximum velocity
- `INJECTOR_DEFAULT_ACCEL_MAX_SPS2` - Maximum acceleration
- `INJECTOR_DEFAULT_VELOCITY_MMS` - Default move velocity
- `INJECTOR_MAX_TORQUE_PERCENT` - Maximum torque limit

### Homing Parameters
- `INJECTOR_HOMING_MACHINE_STROKE_MM` - Homing travel distance
- `INJECTOR_HOMING_MACHINE_VEL_MMS` - Homing velocity
- `INJECTOR_HOMING_MACHINE_ACCEL_MMSS` - Homing acceleration
- `INJECTOR_HOMING_MACHINE_SEARCH_TORQUE_PERCENT` - Torque threshold for detection
- `INJECTOR_HOMING_MACHINE_BACKOFF_MM` - Backoff distance after contact

### Communication Settings
- `LOCAL_PORT` - UDP port for incoming commands (default: 8888)
- `MAX_PACKET_LENGTH` - Maximum packet size
- `RX_QUEUE_SIZE` - Receive queue depth
- `TX_QUEUE_SIZE` - Transmit queue depth
- `TELEMETRY_INTERVAL_MS` - Telemetry update rate (default: 100ms)

### Force Sensor
- `FORCE_SENSOR_ENABLED` - Enable/disable force transducer functionality (default: `true`)
  - Set to `false` if no force transducer is connected to your machine
  - When disabled, all force-related checks are skipped and moves execute without force monitoring
  - **Note:** Machines without force transducers should set this to `false` to prevent sensor errors

---

## Project Structure

```
pressboi/
├── inc/                       # Header files
│   ├── pressboi.h            # Main application class
│   ├── motor_controller.h    # Dual-motor control
│   ├── comms_controller.h    # UDP/Serial communication
│   ├── commands.h            # Command definitions (auto-generated)
│   ├── variables.h           # Telemetry structure (auto-generated)
│   ├── events.h              # Event definitions (auto-generated)
│   └── config.h              # Configuration constants
├── src/                       # Implementation files
│   ├── pressboi.cpp
│   ├── motor_controller.cpp
│   ├── comms_controller.cpp
│   ├── commands.cpp          # Command parser (auto-generated)
│   ├── variables.cpp         # Telemetry builder (auto-generated)
│   └── events.cpp            # Event sender (auto-generated)
├── Device_Startup/            # MCU startup code
│   ├── startup_same53.c
│   └── flash_*.ld            # Linker scripts
├── Debug/                     # Build output (gitignored except .bin/.uf2)
│   ├── pressboi.bin          # Raw binary
│   ├── pressboi.uf2          # UF2 bootloader format
│   └── pressboi.elf          # ELF with debug symbols
├── assets/                    # Images and resources
│   ├── icon.png              # Pressboi icon
│   └── logo.png              # Blue Robotics logo
├── Tools/                     # Flashing utilities
│   ├── bossac/
│   └── flash_clearcore.cmd
├── pressboi.atsln             # Atmel Studio solution
├── pressboi.cppproj           # Project file
├── CHANGELOG.md               # Version history
└── README.md                  # This file
```

---

## Related Projects

- **[BR Equipment Control App](https://github.com/bluerobotics/br-equipment-control-app)** - Python/Tkinter GUI for controlling the Pressboi and other Blue Robotics manufacturing equipment
- **[ClearCore Documentation](https://teknic-inc.github.io/ClearCore-library/)** - Teknic ClearCore library reference

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

Copyright (c) 2025 Blue Robotics

## Contributing

For issues, feature requests, or contributions, please open an issue or pull request on GitHub.

---

<div align="center">

⭐ **Star us on GitHub if you found this useful!**

Made with 💙 by the Blue Robotics team and contributors worldwide

---

<img src="assets/logo.png" alt="Blue Robotics" width="300">

**[bluerobotics.com](https://bluerobotics.com)** | Manufacturing Equipment Control

</div>
