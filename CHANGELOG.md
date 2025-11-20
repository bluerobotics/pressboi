# Changelog

All notable changes to the Pressboi firmware will be documented in this file.

## [1.10.1] - 2025-11-20

### Added
- **Extended watchdog breadcrumbs**: Added `NETWORK_REFRESH`, `USB_SEND`, `USB_RECONNECT`, and `USB_RECOVERY` breadcrumbs for finer-grained watchdog timeout diagnostics

### Fixed
- **CRITICAL: Unbounded network processing**: Limited `EthernetManager::Refresh()` to process maximum 10 packets per call, preventing watchdog timeouts when network accumulates many packets during USB reconnection
- **Unbounded UDP processing**: Limited `processUdp()` to process maximum 10 UDP packets per call, preventing main loop from blocking >128ms
- **USB recovery watchdog feeding removed**: Removed watchdog feeds during USB port recovery operations - if USB operations hang, watchdog should fire to recover system (feeding defeats the purpose of the watchdog)
- **Watchdog timeout on USB reconnection**: USB host reconnection and message sending now properly tracked with breadcrumbs, allowing identification of exactly where blocking occurs

## [1.10.0] - 2025-11-20

### Added
- **Error logging system**: Circular buffer with 100 entries (8.8KB) supporting DEBUG/INFO/WARN/ERROR/CRITICAL levels
- **Heartbeat log**: 24-hour USB/network status tracker (2880 × 8-byte entries, 23KB) recorded every 30 seconds
- **`dump_error_log` command**: Retrieve diagnostic logs via USB or network
- **USB auto-recovery**: Detects stuck TX buffer (2s timeout) and automatically reopens port with watchdog protection

### Changed
- **Motor fault detection**: 500ms grace period after clearing faults prevents false alarms during status register stabilization

### Fixed
- **Watchdog timeout during USB recovery**: Removed blocking delay and added watchdog feeds during port operations
- **Double-reset requirement**: Single RESET command now properly clears watchdog recovery state
- **USB connection persistence**: Auto-recovery eliminates need for power cycle after app restart

## [1.9.0] - 2025-11-19

### Changed
- **Multi-step command handling**: Commands with `force_action=retract` now send a single DONE message only when the entire sequence (move + retract) completes
  - Move completion sends INFO message: "Move complete, retracting..." or "Force limit reached, retracting..."
  - DONE message sent only when retract finishes, using original command name (e.g., `DONE: move_abs`)
  - Simplifies app-side logic - app waits for one DONE per command regardless of internal steps
  - Makes firmware responsible for its own multi-step command flow

## [1.8.5] - 2025-11-19

### Fixed
- **USB host detection**: Fixed USB connection health check only running when messages were queued, causing firmware to stay in "disconnected" state after app restart without power cycle. Health check now runs every loop iteration regardless of queue state, ensuring prompt reconnection detection.

## [1.8.4] - 2025-11-19

### Fixed
- **USB buffer management**: Modified `notifyUsbHostActive()` to clear the TX queue and flush USB input buffer when a USB command is received, preventing stale messages from accumulating

## [1.8.3] - 2025-11-19

### Fixed
- **Watchdog timeouts**: Fixed watchdog timer triggering during USB host detection and reconnection by queueing messages instead of blocking on `ConnectorUsb.Send()`
- **USB reconnection**: USB host reconnection message now only sends when sufficient buffer space is available (>40 bytes) to prevent blocking

## [1.8.2] - 2025-11-19

### Fixed
- **USB host detection**: Fixed USB host connection tracking not resetting properly when receiving commands, causing firmware to remain in "host disconnected" state after app restart without power cycle

## [1.8.1] - 2025-11-19

### Fixed
- **USB host detection**: Moved USB connection state variables from static to class members to ensure proper state management across app restarts

## [1.8.0] - 2025-11-19

### Fixed
- **USB buffer improvements**: Implemented automatic USB host connection detection to prevent buffer overflow
- **Message filtering**: Firmware now only sends USB/network messages when respective interfaces have active listeners

### Changed
- **Device naming portability**: Replaced hardcoded "PRESSBOI" strings with `DEVICE_NAME_UPPER` and `DEVICE_NAME_LOWER` defines from `config.h`

## [1.7.0] - 2025-11-18

### Changed
- **Device definition restructuring**: Moved device definition from Equipment Control App to firmware repository
  - Command definitions, telemetry schemas, and GUI layouts now live in `pressboi/definition/` folder
  - Versioning of device protocol now happens alongside firmware
  - Equipment Control App automatically discovers and loads definitions from device paths
  - Supports both standalone definition folders and definitions embedded in firmware repos

### Fixed
- **USB telemetry**: Fixed firmware not sending telemetry over USB serial when GUI not discovered via network
- **USB message buffer**: Implemented chunked message sending for telemetry packets larger than 64-byte USB CDC buffer
- **Network discovery**: Fixed firmware accepting discovery commands even when GUI already discovered, allowing reconnection
- **Retract position telemetry**: Fixed `retract_pos` reporting garbage values when motor not homed (now reports 0.0)
- **Dual connection support**: Both USB and network telemetry now stream simultaneously when both connections present

## [1.6.1] - 2025-11-17

### Changed
- **Repository structure**: Reorganized project layout - moved libraries to `lib/` folder and flattened `pressboi/` subfolder to root
- **Build system**: Updated project paths to reflect new library locations

## [1.6.0] - 2025-11-15

### Added
- **Endpoint telemetry**: New `endpoint` variable records the exact position (mm) where press moves stop

### Fixed
- **CRITICAL**: Fixed infinite retract loop that caused watchdog timeouts when using `retract` or `abort` force actions.
- Fixed `abort` force action to only trigger retract when force limit is actually hit. Previously, `abort` action would retract even when the move completed normally without hitting the force limit.
- Fixed zero-step moves causing timeout errors. Moves with 0 steps now immediately return `DONE` instead of attempting to execute and timing out.

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

