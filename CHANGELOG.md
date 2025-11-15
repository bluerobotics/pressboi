# Changelog

All notable changes to the Pressboi firmware will be documented in this file.

## [1.6.0] - 2025-11-15

### Added
- **Endpoint telemetry**: New `endpoint` variable records the exact position (mm) where press moves stop, either when force limit is hit or target position is reached. Preserved through subsequent retract operations for post-move analysis.

### Fixed
- **CRITICAL**: Fixed infinite retract loop that caused watchdog timeouts when using `retract` or `abort` force actions. The `force_action` parameter is now properly cleared before starting a retract move to prevent re-triggering.
- Fixed `abort` force action to only trigger retract when force limit is actually hit. Previously, `abort` action would retract even when the move completed normally without hitting the force limit.

### Changed
- Updated `force_action` parameter documentation to clarify behavior: `retract` always retracts on any completion, `abort` only retracts when force limit is hit.

## [1.5.0] - 2025-11-14

### Added
- **USB serial communication**: Commands and telemetry now mirrored to USB serial (ConnectorUsb) alongside network communication
- **Dual-channel command processing**: USB commands processed identically to network commands through unified dispatcher

### Changed
- **Lowercase status text**: Main state and motor status use lowercase for consistency (`standby`, `busy`, `error`, `enabled`, `disabled`, `homed`, `not homed`)
- **USB processing limit**: Characters per call capped at 32 to prevent watchdog timeout (128ms threshold)

## [1.4.2] - 2025-11-13

### Added
- **Force zeroing**: new `set_force_zero` command lets the controller automatically tare either the load cell or the motor-torque force model

## [1.4.1] - 2025-11-07

### Added
- **Bootloader control**: `reboot_bootloader` command drops the controller into ClearCore UF2 mode, enabling remote flashes from the Equipment Control app.
- **Discovery metadata**: discovery responses now include the running firmware version so tooling can detect when an update is available.

## [1.4.0] - 2025-11-07

### Added
- **Force mode commands**: new `set_force_mode`, `set_force_offset`, `set_force_scale`, and `set_strain_cal` commands
- **Press energy integration**: incremental net force × travel accumulation for Joule reporting
- **Machine compliance model**: 4th-order strain calibration accounts for machine deflection in energy calculation
- **Contact thresholding**: 3 kg engagement gate aligns compensation with real workpiece contact
- **Persistent calibration**: non-volatile memory stores calibration constants and force-mode selection (load-cell vs motor torque)
- **Bootloader control**: `reboot_bootloader` command drops directly into ClearCore UF2 mode for automated flashing
- **Discovery metadata**: discovery responses now advertise firmware version for app-side upgrade prompts

### Changed
- **Force mode**: move commands drop the `force_mode` argument; new `set_force_mode` selects sensor source globally
- **Better precision**: doubled math for position/energy and telemetry now reports millimeters with three decimals
- **Retract control**: `set_retract` accepts optional speed (default 25 mm/s) and force-action retracts honour that rate
- **Force mode behaviour**: load-cell moves keep default torque limit while motor-torque mode still uses calibrated curve

### Fixed
- **Retract speed on force action**: force-action retracts now run at the configured retract speed instead of the previous slow fallback rate

## [1.3.0] - 2025-11-06

### Added
- **Hardware watchdog timer** (SAMD51 WDT): 128ms timeout, eses RSTC->RCAUSE register to detect watchdog resets
- **STATE_RECOVERED**: Motors disabled after watchdog timeout until explicit reset
- **test_watchdog command**: Intentionally blocks to trigger watchdog for testing
- **Non-blocking reset**: STATE_RESETTING now uses non-blocking delay to avoid WDT trip
- **Motor torque force limiting**: `motor_torque` mode uses motor HLFB as force limit (no load cell required)
- **Torque-to-force conversion**: `Torque% = 0.0335 × kg + 1.04` (50-1000 kg range)
- **Force mode parameter**: Move commands accept `force_mode` (`motor_torque` or `load_cell`)
- **Split force telemetry**: `force_load_cell` and `force_motor_torque` fields
- **Force source field**: Telemetry indicates active mode (`load_cell` or `motor_torque`)

### Changed
- **Watchdog functions**: Modularized into dedicated recovery, init, feed, and clear functions
- **Limit handling**: `handleLimitReached()` centralizes retract/skip/hold logic for both force modes
- **Load cell checks**: Only runs in `load_cell` mode; `motor_torque` mode operates independently
- **Force limits**: Execute `force_action` (retract/skip/hold) on limit instead of throwing errors
- **Motor enable reporting**: Uses internal `m_isEnabled` flag instead of hardware register
- **Torque telemetry**: Single `torque_avg` field (average of both motors)

### Fixed
- **Torque flickering**: Holds last non-zero value during active moves

## [1.2.1] - 2025-11-04

### Added
- `FORCE_SENSOR_ENABLED` configuration flag in config.h to support machines without force transducers
- Documentation in README for force sensor configuration

### Changed
- Force sensor checks now conditional based on `FORCE_SENSOR_ENABLED` flag
- Machines without force transducers can now operate normally by setting flag to `false`

## [1.2.0] - 2025-11-04

### Added
- Pause and resume functionality for move operations (move_abs, move_inc, retract)
- Move parameter storage for proper pause/resume state tracking
- Move start time tracking for timeout detection
- Force transducer integration with HX711 load cell
- Force-based move control with configurable actions (hold, skip, retract)
- Real-time force monitoring during moves with configurable force limits

### Changed
- State transition logic reordered to check motor status before timeout evaluation
- Resume operation now recalculates remaining steps at resume time (not pause time)
- Force parameter now fully functional (previously temporarily ignored)

### Fixed
- **Critical:** Fixed premature DONE messages - moves no longer send DONE immediately after START
- **Critical:** Fixed move timeout errors - `m_moveStartTime` now properly initialized for all moves
- Fixed resume calculation to use current position instead of stale pause-time position
- Fixed state machine to properly track STARTING → ACTIVE → completion transitions
- Move parameters (initial position, target steps, velocity, acceleration, torque) now properly stored for all move types

## [1.1.0] - 2025-11-03

### Added
- Reset command (replaces clear_errors) for system recovery
- Target position telemetry tracking for all move commands
- Speed limiting (100 mm/s maximum)
- Standardized DONE messages for all commands (enable, disable, home, moves, pause, resume, cancel, reset)

### Changed
- Cancel command now handles emergency stops (abort command removed)
- Force parameter temporarily ignored until force transducer implementation (uses default torque limit)
- Improved command parsing reliability

### Fixed
- Target position now updates correctly during moves (was always 0)
- Code generator permanently fixed for enum comma placement
- All autogenerated files compile without warnings

## [1.0.2] - 2025-11-03

### Added
- MIT License file
- License section in README

### Fixed
- License badge in README now displays correctly
- Release badge styling improved

## [1.0.1] - 2025-11-03

### Changed
- Enhanced README with comprehensive class and method documentation
- Added Blue Robotics and Pressboi logos
- Added detailed flashing instructions for Microchip Studio with Atmel ICE
- Added debugging workflow documentation
- Reorganized flashing methods by use case (development vs field updates)
- Added link to BR Equipment Control App
- Improved visual layout and community engagement footer

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

