# Code Generation System for Fillhead Controller

This directory contains an automated code generation system that generates C header files from JSON schema definitions. This allows you to define your communication protocol (commands and telemetry) in easily-editable JSON files, and automatically generate the C code needed to use them.

## Overview

The code generation system consists of:

1. **JSON Schema Files** (in `st8erboi-fillhead/`)
   - `commands.json` - Defines all commands the controller can receive
   - `telemetry.json` - Defines all telemetry fields the controller sends

2. **Python Generator Script**
   - `generate_headers.py` - GUI application with dark theme

3. **Generated C Headers** (in `st8erboi-fillhead/inc/`)
   - `commands.h` - Command strings, prefixes, and enum definitions
   - `telemetry.h` - Telemetry field key definitions

## Quick Start

### Adding a New Command

1. Open `st8erboi-fillhead/commands.json`
2. Add your new command following this format:

```json
{
  "YOUR_NEW_COMMAND": {
    "device": "fillhead",
    "params": [
      {"name": "param_name", "type": "float", "optional": false, "keywords": ["p", "param"]}
    ],
    "help": "Description of what this command does."
  }
}
```

3. Run the generator script (see "Running the Generator" below)
4. The new command will automatically be added to `commands.h` with:
   - A `#define CMD_STR_YOUR_NEW_COMMAND "YOUR_NEW_COMMAND "`
   - An enum entry `CMD_YOUR_NEW_COMMAND`
5. Add command handling in your C++ code using the new enum value

### Adding a New Telemetry Field

1. Open `st8erboi-fillhead/telemetry.json`
2. Add your new field:

```json
{
  "your_field_name": {
    "gui_var": "gui_variable_name",
    "default": 0.0,
    "format": {
      "suffix": " units",
      "precision": 2
    }
  }
}
```

3. Run the generator script
4. A new `#define TELEM_KEY_YOUR_FIELD_NAME "your_field_name"` will be added to `telemetry.h`
5. Use the define in your telemetry generation code:

```cpp
snprintf(buffer, sizeof(buffer), "%s:%.2f", TELEM_KEY_YOUR_FIELD_NAME, value);
```

## Running the Generator

### Prerequisites

- Python 3.6 or higher
- tkinter (usually included with Python)

### How to Run

The easiest way to use the generator:

**Option 1: Double-click launcher (easiest)**
- **macOS/Linux:** Double-click `generate_headers.command`
- **Windows:** Double-click `generate_headers.bat`

**Option 2: Run from terminal**
```bash
python3 generate_headers.py
```

This opens a dark-themed GUI where you can:
- Browse and select your JSON input files
- Browse and select your output header files
- Generate commands.h, telemetry.h, or both with a button click
- See real-time output and error messages in the console

**Features:**
- 🎨 Dark theme for comfortable viewing
- 📁 File browser dialogs for easy file selection
- 🔘 Separate buttons for generating commands, telemetry, or both
- 📊 Live output console showing progress and errors
- ✅ Success/error message boxes
- 💾 Remembers default paths automatically

**GUI Layout:**
```
╔════════════════════════════════════════════════╗
║        Fillhead Code Generator                 ║
║                                                ║
║  Input Files (JSON):                           ║
║    commands.json:   [path___________] [Browse] ║
║    telemetry.json:  [path___________] [Browse] ║
║                                                ║
║  Output Files (Headers):                       ║
║    commands.h:      [path___________] [Browse] ║
║    telemetry.h:     [path___________] [Browse] ║
║                                                ║
║  [Generate Commands] [Generate Telemetry]      ║
║           [Generate Both]                      ║
║                                                ║
║  Output Log:                                   ║
║  ┌──────────────────────────────────────────┐ ║
║  │ Ready to generate headers...             │ ║
║  │                                          │ ║
║  │ ════════════════════════════════════     │ ║
║  │ Generating both headers...               │ ║
║  │ Loading commands.json...                 │ ║
║  │ Found 17 commands                        │ ║
║  │ ✓ commands.h generated                   │ ║
║  └──────────────────────────────────────────┘ ║
╚════════════════════════════════════════════════╝
```

### What the Script Does

1. Reads `commands.json` and `telemetry.json`
2. Validates the schema
3. Generates `st8erboi-fillhead/inc/commands.h` with:
   - Command string defines (`CMD_STR_*`)
   - Status/telemetry prefix defines
   - Command enum type
4. Generates `st8erboi-fillhead/inc/telemetry.h` with:
   - Telemetry field key defines (`TELEM_KEY_*`)
   - Documentation with GUI variable mappings

## JSON Schema Reference

### commands.json Format

```json
{
  "COMMAND_NAME": {
    "device": "fillhead",                    // Target device (currently always "fillhead")
    "handler": "script",                     // Optional: "script" for script-handled commands
    "params": [                              // Array of parameters (can be empty)
      {
        "name": "parameter_name",            // Parameter name
        "type": "float",                     // Type: "float", "int", "string", etc.
        "optional": false,                   // Whether parameter is optional
        "default": 100,                      // Default value (for optional params)
        "keywords": ["key", "k"]             // Alternative names for parameter
      }
    ],
    "help": "Description of the command."   // Help text
  }
}
```

**Command Naming Conventions:**
- Use `UPPER_SNAKE_CASE`
- Be descriptive but concise
- Group related commands with common prefixes (e.g., `HEATER_ON`, `HEATER_OFF`)

**Parameter Types:**
- `float` - Floating point numbers
- `int` - Integers
- `string` - Text strings
- `bool` - Boolean (true/false)

### telemetry.json Format

```json
{
  "field_key": {
    "gui_var": "name_of_gui_variable",      // GUI variable name for display
    "default": 0.0,                          // Default value
    "format": {                              // Optional formatting
      "suffix": " units",                    // Unit suffix for display
      "prefix": "$",                         // Unit prefix for display
      "precision": 2,                        // Decimal places
      "map": {                               // Value mapping (for enums/states)
        "0": "Off",
        "1": "On"
      }
    }
  }
}
```

**Field Naming Conventions:**
- Use `lower_snake_case`
- Use common prefixes to group related fields:
  - `inj_*` - Injector fields
  - `h_*` - Heater fields
  - `vac_*` - Vacuum fields
  - `*_st` or `*_st_str` - State fields

**Common Field Suffixes:**
- `_pv` - Process value (current/measured value)
- `_sp` - Setpoint (target value)
- `_st` - State (numeric)
- `_st_str` - State string (text)

## Integration with C++ Code

### Using Commands

The generated `commands.h` provides:

```cpp
#include "commands.h"

// String defines for command matching
#define CMD_STR_HEATER_ON "HEATER_ON"

// Enum for type-safe command handling
typedef enum {
    CMD_UNKNOWN,
    CMD_HEATER_ON,
    CMD_HEATER_OFF,
    // ...
} Command;
```

In your parser (e.g., `comms_controller.cpp`):

```cpp
Command CommsController::parseCommand(const char* msg) {
    if (strcmp(msg, CMD_STR_HEATER_ON) == 0) return CMD_HEATER_ON;
    if (strcmp(msg, CMD_STR_HEATER_OFF) == 0) return CMD_HEATER_OFF;
    // ...
    return CMD_UNKNOWN;
}
```

In your command dispatcher:

```cpp
switch (command_enum) {
    case CMD_HEATER_ON:
        m_heater.turnOn();
        break;
    case CMD_HEATER_OFF:
        m_heater.turnOff();
        break;
    // ...
}
```

### Using Telemetry

The generated `telemetry.h` provides:

```cpp
#include "telemetry.h"

// Field key defines
#define TELEM_KEY_H_PV "h_pv"
#define TELEM_KEY_H_SP "h_sp"
```

In your telemetry generation code:

```cpp
char buffer[256];
snprintf(buffer, sizeof(buffer),
    "%s%s:%.1f,%s:%.1f",
    TELEM_PREFIX,
    TELEM_KEY_H_PV, currentTemp,
    TELEM_KEY_H_SP, setpoint);
```

**Benefits:**
- Compiler catches typos (undefined symbol errors)
- Autocomplete works in your IDE
- Easy to refactor (change JSON, regenerate, compiler shows what needs updating)
- Single source of truth for the protocol

## Workflow

### Typical Development Cycle

1. **Define Protocol in JSON**
   ```bash
   # Edit commands.json to add new commands
   # Edit telemetry.json to add new telemetry fields
   ```

2. **Generate Headers**
   ```bash
   python generate_headers.py
   ```

3. **Implement in C++**
   - Add command parsing logic in `comms_controller.cpp`
   - Add command handling in `fillhead.cpp` dispatch function
   - Add telemetry field generation in appropriate controller

4. **Test**
   - Build and flash firmware
   - Send commands via GUI or UDP
   - Verify telemetry output

### Version Control Tips

- **Commit JSON files:** Always commit `commands.json` and `telemetry.json`
- **Regenerate headers:** After pulling changes, regenerate headers
- **Review generated code:** Use `git diff` to review changes to generated headers
- **Keep in sync:** JSON files are the source of truth; manually editing `.h` files will be overwritten

## File Structure

```
pressboi/
├── generate_headers.py            # GUI code generator
├── generate_headers.command       # macOS/Linux launcher (double-click)
├── generate_headers.bat           # Windows launcher (double-click)
├── CODEGEN_README.md              # This file
│
└── st8erboi-fillhead/
    ├── commands.json              # Command schema (source of truth)
    ├── telemetry.json             # Telemetry schema (source of truth)
    │
    ├── inc/
    │   ├── commands.h             # Generated command definitions
    │   ├── telemetry.h            # Generated telemetry definitions
    │   └── ... (other headers)
    │
    └── src/
        ├── comms_controller.cpp   # Command parsing
        ├── fillhead.cpp           # Command dispatch & telemetry
        └── ... (other source files)
```

## Advanced Topics

### Categorizing Commands

Commands are automatically categorized based on naming patterns:
- `HEATER_*` → Heater Commands
- `VACUUM_*` (no VALVE) → Vacuum Commands
- `*_VALVE_*` → Pinch Valve Commands
- `INJECT_*`, `*_HOME_*`, `JOG_*` → Injector Motion Commands
- Commands with `"handler": "script"` → Script Commands
- Everything else → General System Commands

Categories control the grouping in the generated header file comments.

### Custom Generator Modifications

To customize the generator behavior:

1. Open `generate_headers.py`
2. Modify the `generate_command_header()` or `generate_telemetry_header()` functions
3. Change formatting, add new features, or adjust categorization logic
4. Regenerate headers

### Integrating with Build System

You can add the generator to your build process:

**Option 1: Pre-build Script**
Add to your build tool to run before compilation

**Option 2: Git Hook**
Create `.git/hooks/pre-commit` to regenerate on commit

**Option 3: Manual**
Run manually when JSON files change (current approach)

## Troubleshooting

### Generator Script Errors

**Problem:** `FileNotFoundError: commands.json not found`
- **Solution:** Run script from project root, or use `--commands` flag with full path

**Problem:** `json.decoder.JSONDecodeError`
- **Solution:** Check JSON syntax (missing commas, brackets, quotes)
- Use a JSON validator: https://jsonlint.com/

### C++ Compilation Errors

**Problem:** `undefined reference to TELEM_KEY_*`
- **Solution:** Make sure `#include "telemetry.h"` is in your source file

**Problem:** Headers not updating
- **Solution:** Regenerate headers after changing JSON files
- Check file timestamps to ensure new version was generated

### Field Name Mismatches

**Problem:** Telemetry fields don't match between firmware and GUI
- **Solution:** Ensure telemetry.json is synchronized between firmware and GUI repositories
- Use same JSON file for both systems if possible

## Future Enhancements

Potential improvements to the system:

- [ ] Add JSON schema validation
- [ ] Generate parser code automatically
- [ ] Generate command dispatch boilerplate
- [ ] Support for multiple devices
- [ ] Generate documentation (Markdown or HTML)
- [ ] Add unit tests for generator
- [ ] Create GUI to edit JSON files
- [ ] Add TypeScript definitions for GUI side

## Questions or Issues?

If you encounter problems or have suggestions for improving this system, please contact the firmware team or create an issue in the project repository.

---

**Last Updated:** October 14, 2025  
**Version:** 1.0

