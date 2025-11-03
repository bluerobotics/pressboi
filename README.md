# Pressboi Firmware

Firmware for the Pressboi dual-motor press control system, built on the Teknic ClearCore platform.

## Features

- **Dual-motor synchronized control** - Two motors on a single axis for press operations
- **UDP/Ethernet communication** - Network-based control with device discovery
- **USB Serial fallback** - Direct USB communication support
- **Command-based control** - Simple text-based command protocol
- **Real-time telemetry** - Position, force, torque, and status reporting
- **Pause/resume/cancel** - Interrupt and resume operations safely
- **Force monitoring** - Real-time force feedback with configurable limits
- **Auto-generated protocol** - Commands and telemetry from JSON schemas

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

**Method 1: UF2 Bootloader (Recommended)**
1. Double-press the reset button on the ClearCore
2. ClearCore appears as USB mass storage device
3. Copy `pressboi.uf2` to the drive
4. ClearCore automatically reboots with new firmware

**Method 2: BOSSA (Command Line)**
```bash
cd Tools
flash_clearcore.cmd
```

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

## Configuration

Edit `inc/config.h` to customize:
- Motor parameters (pitch, steps per mm, acceleration, velocity)
- Network settings (UDP port)
- Telemetry intervals
- Force limits

## Project Structure

```
pressboi/
├── inc/                      # Header files
│   ├── pressboi.h           # Main application class
│   ├── motor_controller.h   # Dual-motor control
│   ├── comms_controller.h   # UDP/Serial communication
│   ├── commands.h           # Command definitions (auto-generated)
│   ├── variables.h          # Telemetry structure (auto-generated)
│   ├── events.h             # Event definitions (auto-generated)
│   └── config.h             # Configuration constants
├── src/                      # Implementation files
│   ├── pressboi.cpp
│   ├── motor_controller.cpp
│   ├── comms_controller.cpp
│   ├── commands.cpp         # Command parser (auto-generated)
│   ├── variables.cpp        # Telemetry builder (auto-generated)
│   └── events.cpp           # Event sender (auto-generated)
└── Device_Startup/           # MCU startup code
    ├── startup_same53.c
    └── flash_*.ld           # Linker scripts
```

## Code Generation

Protocol files (`commands.h/cpp`, `variables.h/cpp`, `events.h/cpp`) are auto-generated from JSON schemas using the `br-equipment-control-app` code generator:

1. Edit JSON files in `br-equipment-control-app/devices/pressboi/`:
   - `commands.json` - Command definitions
   - `telemetry.json` - Variable definitions
   - `events.json` - Event definitions

2. Run code generator:
   ```bash
   cd br-equipment-control-app
   python generate_all_headers.py
   ```

3. Copy generated files from `devices/pressboi/generated/` to firmware `inc/` and `src/` folders

## License

[Your License Here]

## Contributing

See the main `br-equipment-control-app` repository for the control GUI and protocol definitions.

