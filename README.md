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

1. Open `pressboi.atsln` in Atmel Studio
2. Ensure libraries are properly referenced:
   - `lib/libClearCore/ClearCore.cppproj`
   - `lib/LwIP/LwIP.cppproj`
3. Select build configuration (Debug or Release)
4. Build the solution (F7)

### Output Files

- `Debug/pressboi.bin` - Binary firmware image
- `Debug/pressboi.uf2` - UF2 format for bootloader flashing

### Flashing

### Via BR Equipment Control App (Recommended for Updates)

The easiest way to update firmware on an already-running device is through the [BR Equipment Control App](https://github.com/bluerobotics/br-equipment-control-app):

1. Open the Firmware Manager in the app
2. Select your device
3. Choose the firmware file or download the latest release
4. Click "Update Firmware" - the app handles the entire flashing process automatically

**Note:** Firmware flashing is only supported over USB connections. For initial flashing of a new device, use the bootloader method below.

### Via Bootloader (For Initial Flashing)

For initial flashing of a new device or when the app is not available:

1. Put the ClearCore into bootloader mode (hold button during power-on)
2. Copy `pressboi.uf2` to the mounted bootloader drive
3. The device will automatically reboot with new firmware

### Via Atmel Studio

1. Connect ClearCore via USB
2. Select "Custom Programming Tool" in project settings
3. Build and program (F5)

---

## Communication Protocol

The firmware uses a simple text-based command protocol over UDP or USB serial:

- Commands: `move_abs <position> mm`, `move_inc <distance> mm`, `home`, `enable`, `disable`
- Status: `DONE: <command>`, `ERROR: <message>`, `INFO: <message>`
- Telemetry: Periodic status updates with position, force, torque, and motor status

See the [BR Equipment Control App](https://github.com/bluerobotics/br-equipment-control-app) for full protocol documentation and command reference.

---

## Hardware Configuration

### Motors
- **Motor A**: Primary press motor (M0)
- **Motor B**: Secondary press motor (M1) - synchronized with Motor A

### Sensors
- **Force sensor**: Load cell or motor torque-based force monitoring
- **Position feedback**: Encoder feedback from ClearPath motors

## Configuration

Key parameters can be configured in `inc/config.h`:

- **Motor parameters**: Steps per mm, velocities, accelerations, torque limits
- **Homing parameters**: Velocities, accelerations, torque limits, backoff distances
- **Force monitoring**: Force limits, sensor calibration, enable/disable options
- **Network settings**: UDP port, packet sizes, telemetry interval

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
