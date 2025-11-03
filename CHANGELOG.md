# Changelog

All notable changes to the Pressboi firmware will be documented in this file.

## [1.0.0] - 2025-11-03

### Added
- Initial release of Pressboi press control firmware
- Dual-motor synchronized control for press operations
- UDP/Ethernet communication with discovery protocol
- USB Serial communication support
- Command system: home, move_abs, move_inc, set_start_pos, move_to_start, enable, disable
- Pause/resume/cancel operations for any active move
- Real-time telemetry reporting (position, force, torque, motor status)
- Homing routines with configurable parameters
- Force limiting and monitoring
- State machine-based operation for reliable control

### Technical Details
- Based on ClearCore platform (SAME53 microcontroller)
- Communication: UDP port 8888, USB Serial
- Command parser with parameter extraction
- Circular message queues for Rx/Tx
- Auto-generated protocol files from JSON schemas

