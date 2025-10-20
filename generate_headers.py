#!/usr/bin/env python3
"""
GUI Code Generator for Fillhead Communication Protocol
Generates commands.h and telemetry.h from JSON schema files.

This GUI version provides an easy-to-use interface for selecting files
and generating C headers without using the command line.
"""

import json
import os
import sys
from pathlib import Path
from datetime import datetime
from typing import Dict, Any
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext

# Import the generation functions from the main script
def load_json(filepath: str) -> Dict:
    """Load and parse a JSON file."""
    with open(filepath, 'r') as f:
        return json.load(f)

def generate_command_header(commands: Dict[str, Any], output_path: str) -> None:
    """Generate commands.h from commands.json."""
    
    # Organize commands by category
    categories = {
        'General System Commands': [],
        'Injector Motion Commands': [],
        'Pinch Valve Commands': [],
        'Heater Commands': [],
        'Vacuum Commands': [],
        'Script Commands': []
    }
    
    # Categorize commands based on keywords
    for cmd_name, cmd_data in commands.items():
        if 'HEATER' in cmd_name:
            categories['Heater Commands'].append((cmd_name, cmd_data))
        elif 'VACUUM' in cmd_name and 'VALVE' not in cmd_name:
            categories['Vacuum Commands'].append((cmd_name, cmd_data))
        elif 'VALVE' in cmd_name:
            categories['Pinch Valve Commands'].append((cmd_name, cmd_data))
        elif 'INJECT' in cmd_name or 'HOME' in cmd_name or 'JOG' in cmd_name:
            categories['Injector Motion Commands'].append((cmd_name, cmd_data))
        elif cmd_data.get('handler') == 'script':
            categories['Script Commands'].append((cmd_name, cmd_data))
        else:
            categories['General System Commands'].append((cmd_name, cmd_data))
    
    # Generate header content
    lines = []
    lines.append("/**")
    lines.append(" * @file commands.h")
    lines.append(" * @brief Defines the command interface for the Fillhead controller.")
    lines.append(" * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY")
    lines.append(f" * Generated from commands.json on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    lines.append(" * ")
    lines.append(" * This header file centralizes the entire command vocabulary for the Fillhead system.")
    lines.append(" * To modify commands, edit commands.json and regenerate this file using generate_headers.py")
    lines.append(" */")
    lines.append("#pragma once")
    lines.append("")
    lines.append("//==================================================================================================")
    lines.append("// Command Strings & Prefixes")
    lines.append("//==================================================================================================")
    lines.append("")
    
    # Generate command string defines for each category
    for category, cmds in categories.items():
        if not cmds:
            continue
            
        lines.append("/**")
        lines.append(f" * @name {category}")
        lines.append(" * @{")
        lines.append(" */")
        
        for cmd_name, cmd_data in cmds:
            # Determine if command has parameters (add trailing space)
            has_params = len(cmd_data.get('params', [])) > 0
            cmd_str = cmd_name + (" " if has_params else "")
            
            help_text = cmd_data.get('help', 'No description available.')
            lines.append(f'#define CMD_STR_{cmd_name:<35} "{cmd_str:<30}" ///< {help_text}')
        
        lines.append("/** @} */")
        lines.append("")
    
    # Add telemetry and status prefixes
    lines.append("/**")
    lines.append(" * @name Telemetry and Status Prefixes")
    lines.append(" * @{")
    lines.append(" */")
    lines.append('#define TELEM_PREFIX                        "FILLHEAD_TELEM: "         ///< Prefix for all telemetry messages.')
    lines.append('#define STATUS_PREFIX_INFO                  "FILLHEAD_INFO: "          ///< Prefix for informational status messages.')
    lines.append('#define STATUS_PREFIX_START                 "FILLHEAD_START: "         ///< Prefix for messages indicating the start of an operation.')
    lines.append('#define STATUS_PREFIX_DONE                  "FILLHEAD_DONE: "          ///< Prefix for messages indicating the successful completion of an operation.')
    lines.append('#define STATUS_PREFIX_ERROR                 "FILLHEAD_ERROR: "         ///< Prefix for messages indicating an error or fault.')
    lines.append('#define STATUS_PREFIX_DISCOVERY             "DISCOVERY_RESPONSE: "     ///< Prefix for the device discovery response.')
    lines.append("/** @} */")
    lines.append("")
    
    # Generate enum
    lines.append("/**")
    lines.append(" * @enum Command")
    lines.append(" * @brief Enumerates all possible commands that can be processed by the Fillhead.")
    lines.append(" * @details This enum provides a type-safe way to handle incoming commands.")
    lines.append(" */")
    lines.append("typedef enum {")
    lines.append("    CMD_UNKNOWN,                        ///< Represents an unrecognized or invalid command.")
    lines.append("")
    
    # Add enum entries by category
    for category, cmds in categories.items():
        if not cmds:
            continue
            
        lines.append(f"    // {category}")
        for i, (cmd_name, cmd_data) in enumerate(cmds):
            comma = "," if i < len(cmds) - 1 or category != list(categories.keys())[-1] else ""
            lines.append(f"    CMD_{cmd_name:<35} ///< @see CMD_STR_{cmd_name}" + comma)
        lines.append("")
    
    # Remove trailing empty line and add closing brace
    if lines[-1] == "":
        lines.pop()
    lines.append("} Command;")
    
    # Write to file
    with open(output_path, 'w') as f:
        f.write('\n'.join(lines))

def generate_telemetry_header(telemetry: Dict[str, Any], output_path: str) -> None:
    """Generate telemetry.h from telemetry.json."""
    
    lines = []
    lines.append("/**")
    lines.append(" * @file telemetry.h")
    lines.append(" * @brief Defines the telemetry field identifiers for the Fillhead controller.")
    lines.append(" * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY")
    lines.append(f" * Generated from telemetry.json on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    lines.append(" * ")
    lines.append(" * This header file defines all telemetry field keys used in the system.")
    lines.append(" * To modify telemetry fields, edit telemetry.json and regenerate this file using generate_headers.py")
    lines.append(" */")
    lines.append("#pragma once")
    lines.append("")
    lines.append("//==================================================================================================")
    lines.append("// Telemetry Field Identifiers")
    lines.append("//==================================================================================================")
    lines.append("")
    lines.append("/**")
    lines.append(" * @name Telemetry Field Keys")
    lines.append(" * @brief String identifiers for telemetry data fields in the CSV-like format.")
    lines.append(" * @details These defines specify the exact field names used in the telemetry string.")
    lines.append(" * Format: \"FILLHEAD_TELEM: field1:value1,field2:value2,...\"")
    lines.append(" * @{")
    lines.append(" */")
    lines.append("")
    
    # Group telemetry by subsystem
    groups = {
        'Main System': ['MAIN_STATE'],
        'Injector': ['inj_t0', 'inj_t1', 'inj_h_mach', 'inj_h_cart', 'inj_mach_mm', 
                     'inj_cart_mm', 'inj_cumulative_ml', 'inj_active_ml', 'inj_tgt_ml', 
                     'enabled0', 'enabled1', 'inj_st'],
        'Injection Valve': ['inj_valve_pos', 'inj_valve_torque', 'inj_valve_homed', 'inj_v_st'],
        'Vacuum Valve': ['vac_valve_pos', 'vac_valve_torque', 'vac_valve_homed', 'vac_v_st'],
        'Heater': ['h_pv', 'h_sp', 'h_st_str'],
        'Vacuum': ['vac_pv', 'vac_st_str']
    }
    
    for group_name, fields in groups.items():
        lines.append(f"// --- {group_name} Telemetry ---")
        for field in fields:
            if field in telemetry:
                field_data = telemetry[field]
                gui_var = field_data.get('gui_var', 'N/A')
                default = field_data.get('default', 'N/A')
                lines.append(f'#define TELEM_KEY_{field.upper():<25} "{field:<20}"  ///< GUI: {gui_var}, Default: {default}')
        lines.append("")
    
    lines.append("/** @} */")
    lines.append("")
    
    # Add a comment about usage
    lines.append("/**")
    lines.append(" * @section Usage Example")
    lines.append(" * @code")
    lines.append(" * char buffer[256];")
    lines.append(' * snprintf(buffer, sizeof(buffer), "%s%s:%d,%s:%.2f",')
    lines.append(" *          TELEM_PREFIX,")
    lines.append(" *          TELEM_KEY_MAIN_STATE, stateValue,")
    lines.append(" *          TELEM_KEY_H_PV, temperatureValue);")
    lines.append(" * @endcode")
    lines.append(" */")
    
    # Write to file
    with open(output_path, 'w') as f:
        f.write('\n'.join(lines))


class CodeGeneratorGUI:
    """GUI application for generating C headers from JSON schemas."""
    
    def __init__(self, root):
        self.root = root
        self.root.title("Fillhead Code Generator")
        self.root.geometry("900x700")
        
        # Set default paths
        script_dir = Path(__file__).parent
        self.default_commands_json = str(script_dir / 'st8erboi-fillhead' / 'commands.json')
        self.default_telemetry_json = str(script_dir / 'st8erboi-fillhead' / 'telemetry.json')
        self.default_commands_h = str(script_dir / 'st8erboi-fillhead' / 'inc' / 'commands.h')
        self.default_telemetry_h = str(script_dir / 'st8erboi-fillhead' / 'inc' / 'telemetry.h')
        
        # Apply dark theme
        self.setup_dark_theme()
        
        # Create UI elements
        self.create_widgets()
        
        # Load default paths
        self.load_default_paths()
    
    def setup_dark_theme(self):
        """Configure dark theme colors."""
        # Define color scheme
        self.bg_color = "#2b2b2b"
        self.fg_color = "#e0e0e0"
        self.entry_bg = "#3c3f41"
        self.button_bg = "#4a4a4a"
        self.button_active = "#5a5a5a"
        self.accent_color = "#5294e2"
        self.success_color = "#4caf50"
        self.error_color = "#f44336"
        
        # Configure root window
        self.root.configure(bg=self.bg_color)
        
        # Configure ttk style
        style = ttk.Style()
        style.theme_use('clam')
        
        # Configure colors for ttk widgets
        style.configure('TFrame', background=self.bg_color)
        style.configure('TLabel', background=self.bg_color, foreground=self.fg_color, font=('Arial', 10))
        style.configure('Header.TLabel', font=('Arial', 12, 'bold'))
        style.configure('TButton', background=self.button_bg, foreground=self.fg_color, 
                       borderwidth=1, relief='flat', font=('Arial', 10))
        style.map('TButton',
                 background=[('active', self.button_active), ('pressed', self.accent_color)])
        
        style.configure('Accent.TButton', background=self.accent_color, foreground='white',
                       font=('Arial', 11, 'bold'))
        style.map('Accent.TButton',
                 background=[('active', '#6aa8f0'), ('pressed', '#4080d0')])
    
    def create_widgets(self):
        """Create and layout all GUI widgets."""
        # Main container with padding
        main_frame = ttk.Frame(self.root, padding="20")
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # Title
        title_label = ttk.Label(main_frame, text="Fillhead Code Generator", 
                               style='Header.TLabel')
        title_label.pack(pady=(0, 20))
        
        # File selection frame
        files_frame = ttk.Frame(main_frame)
        files_frame.pack(fill=tk.X, pady=(0, 20))
        
        # Input files section
        input_label = ttk.Label(files_frame, text="Input Files (JSON):", style='Header.TLabel')
        input_label.grid(row=0, column=0, columnspan=3, sticky=tk.W, pady=(0, 10))
        
        # Commands JSON
        ttk.Label(files_frame, text="commands.json:").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.commands_json_var = tk.StringVar()
        self.commands_json_entry = tk.Entry(files_frame, textvariable=self.commands_json_var,
                                            bg=self.entry_bg, fg=self.fg_color, 
                                            insertbackground=self.fg_color, width=60)
        self.commands_json_entry.grid(row=1, column=1, padx=10, pady=5)
        ttk.Button(files_frame, text="Browse...", 
                  command=lambda: self.browse_file(self.commands_json_var, "json")).grid(row=1, column=2, pady=5)
        
        # Telemetry JSON
        ttk.Label(files_frame, text="telemetry.json:").grid(row=2, column=0, sticky=tk.W, pady=5)
        self.telemetry_json_var = tk.StringVar()
        self.telemetry_json_entry = tk.Entry(files_frame, textvariable=self.telemetry_json_var,
                                             bg=self.entry_bg, fg=self.fg_color,
                                             insertbackground=self.fg_color, width=60)
        self.telemetry_json_entry.grid(row=2, column=1, padx=10, pady=5)
        ttk.Button(files_frame, text="Browse...",
                  command=lambda: self.browse_file(self.telemetry_json_var, "json")).grid(row=2, column=2, pady=5)
        
        # Separator
        ttk.Separator(files_frame, orient='horizontal').grid(row=3, column=0, columnspan=3, 
                                                             sticky=tk.EW, pady=15)
        
        # Output files section
        output_label = ttk.Label(files_frame, text="Output Files (Headers):", style='Header.TLabel')
        output_label.grid(row=4, column=0, columnspan=3, sticky=tk.W, pady=(0, 10))
        
        # Commands header
        ttk.Label(files_frame, text="commands.h:").grid(row=5, column=0, sticky=tk.W, pady=5)
        self.commands_h_var = tk.StringVar()
        self.commands_h_entry = tk.Entry(files_frame, textvariable=self.commands_h_var,
                                        bg=self.entry_bg, fg=self.fg_color,
                                        insertbackground=self.fg_color, width=60)
        self.commands_h_entry.grid(row=5, column=1, padx=10, pady=5)
        ttk.Button(files_frame, text="Browse...",
                  command=lambda: self.browse_file(self.commands_h_var, "h")).grid(row=5, column=2, pady=5)
        
        # Telemetry header
        ttk.Label(files_frame, text="telemetry.h:").grid(row=6, column=0, sticky=tk.W, pady=5)
        self.telemetry_h_var = tk.StringVar()
        self.telemetry_h_entry = tk.Entry(files_frame, textvariable=self.telemetry_h_var,
                                          bg=self.entry_bg, fg=self.fg_color,
                                          insertbackground=self.fg_color, width=60)
        self.telemetry_h_entry.grid(row=6, column=1, padx=10, pady=5)
        ttk.Button(files_frame, text="Browse...",
                  command=lambda: self.browse_file(self.telemetry_h_var, "h")).grid(row=6, column=2, pady=5)
        
        # Buttons frame
        buttons_frame = ttk.Frame(main_frame)
        buttons_frame.pack(pady=20)
        
        # Generate buttons
        ttk.Button(buttons_frame, text="Generate Commands", 
                  command=self.generate_commands,
                  width=20).grid(row=0, column=0, padx=10)
        
        ttk.Button(buttons_frame, text="Generate Telemetry",
                  command=self.generate_telemetry,
                  width=20).grid(row=0, column=1, padx=10)
        
        ttk.Button(buttons_frame, text="Generate Both",
                  style='Accent.TButton',
                  command=self.generate_both,
                  width=20).grid(row=0, column=2, padx=10)
        
        # Output console frame
        console_frame = ttk.Frame(main_frame)
        console_frame.pack(fill=tk.BOTH, expand=True, pady=(20, 0))
        
        ttk.Label(console_frame, text="Output Log:", style='Header.TLabel').pack(anchor=tk.W)
        
        # Scrolled text widget for output
        self.output_text = scrolledtext.ScrolledText(console_frame, 
                                                     bg=self.entry_bg,
                                                     fg=self.fg_color,
                                                     insertbackground=self.fg_color,
                                                     height=15,
                                                     font=('Courier', 9))
        self.output_text.pack(fill=tk.BOTH, expand=True, pady=(5, 0))
        
        # Configure text tags for colored output
        self.output_text.tag_config("success", foreground=self.success_color)
        self.output_text.tag_config("error", foreground=self.error_color)
        self.output_text.tag_config("info", foreground=self.accent_color)
        
        self.log("Ready to generate headers...\n", "info")
    
    def load_default_paths(self):
        """Load default file paths if they exist."""
        if os.path.exists(self.default_commands_json):
            self.commands_json_var.set(self.default_commands_json)
        if os.path.exists(self.default_telemetry_json):
            self.telemetry_json_var.set(self.default_telemetry_json)
        if os.path.exists(self.default_commands_h):
            self.commands_h_var.set(self.default_commands_h)
        if os.path.exists(self.default_telemetry_h):
            self.telemetry_h_var.set(self.default_telemetry_h)
    
    def browse_file(self, var, file_type):
        """Open file browser dialog."""
        if file_type == "json":
            filetypes = [("JSON files", "*.json"), ("All files", "*.*")]
        else:
            filetypes = [("Header files", "*.h"), ("All files", "*.*")]
        
        filename = filedialog.askopenfilename(
            title=f"Select {file_type.upper()} file",
            filetypes=filetypes
        )
        
        if filename:
            var.set(filename)
    
    def log(self, message, tag="normal"):
        """Add message to output console."""
        self.output_text.insert(tk.END, message, tag)
        self.output_text.see(tk.END)
        self.output_text.update()
    
    def generate_commands(self):
        """Generate commands.h from commands.json."""
        try:
            self.log("\n" + "="*80 + "\n", "info")
            self.log("Generating commands.h...\n", "info")
            
            # Validate input files
            commands_json_path = self.commands_json_var.get()
            if not commands_json_path or not os.path.exists(commands_json_path):
                raise FileNotFoundError(f"Commands JSON file not found: {commands_json_path}")
            
            commands_h_path = self.commands_h_var.get()
            if not commands_h_path:
                raise ValueError("Please specify output path for commands.h")
            
            # Load JSON
            self.log(f"Loading {commands_json_path}...\n")
            commands = load_json(commands_json_path)
            self.log(f"Found {len(commands)} commands\n")
            
            # Generate header
            self.log(f"Generating {commands_h_path}...\n")
            generate_command_header(commands, commands_h_path)
            
            self.log("✓ Successfully generated commands.h!\n", "success")
            messagebox.showinfo("Success", "commands.h generated successfully!")
            
        except Exception as e:
            error_msg = f"✗ Error: {str(e)}\n"
            self.log(error_msg, "error")
            messagebox.showerror("Error", str(e))
    
    def generate_telemetry(self):
        """Generate telemetry.h from telemetry.json."""
        try:
            self.log("\n" + "="*80 + "\n", "info")
            self.log("Generating telemetry.h...\n", "info")
            
            # Validate input files
            telemetry_json_path = self.telemetry_json_var.get()
            if not telemetry_json_path or not os.path.exists(telemetry_json_path):
                raise FileNotFoundError(f"Telemetry JSON file not found: {telemetry_json_path}")
            
            telemetry_h_path = self.telemetry_h_var.get()
            if not telemetry_h_path:
                raise ValueError("Please specify output path for telemetry.h")
            
            # Load JSON
            self.log(f"Loading {telemetry_json_path}...\n")
            telemetry = load_json(telemetry_json_path)
            self.log(f"Found {len(telemetry)} telemetry fields\n")
            
            # Generate header
            self.log(f"Generating {telemetry_h_path}...\n")
            generate_telemetry_header(telemetry, telemetry_h_path)
            
            self.log("✓ Successfully generated telemetry.h!\n", "success")
            messagebox.showinfo("Success", "telemetry.h generated successfully!")
            
        except Exception as e:
            error_msg = f"✗ Error: {str(e)}\n"
            self.log(error_msg, "error")
            messagebox.showerror("Error", str(e))
    
    def generate_both(self):
        """Generate both commands.h and telemetry.h."""
        try:
            self.log("\n" + "="*80 + "\n", "info")
            self.log("Generating both headers...\n", "info")
            
            # Generate commands
            commands_json_path = self.commands_json_var.get()
            if not commands_json_path or not os.path.exists(commands_json_path):
                raise FileNotFoundError(f"Commands JSON file not found: {commands_json_path}")
            
            commands_h_path = self.commands_h_var.get()
            if not commands_h_path:
                raise ValueError("Please specify output path for commands.h")
            
            self.log(f"Loading {commands_json_path}...\n")
            commands = load_json(commands_json_path)
            self.log(f"Found {len(commands)} commands\n")
            
            self.log(f"Generating {commands_h_path}...\n")
            generate_command_header(commands, commands_h_path)
            self.log("✓ commands.h generated\n", "success")
            
            # Generate telemetry
            telemetry_json_path = self.telemetry_json_var.get()
            if not telemetry_json_path or not os.path.exists(telemetry_json_path):
                raise FileNotFoundError(f"Telemetry JSON file not found: {telemetry_json_path}")
            
            telemetry_h_path = self.telemetry_h_var.get()
            if not telemetry_h_path:
                raise ValueError("Please specify output path for telemetry.h")
            
            self.log(f"Loading {telemetry_json_path}...\n")
            telemetry = load_json(telemetry_json_path)
            self.log(f"Found {len(telemetry)} telemetry fields\n")
            
            self.log(f"Generating {telemetry_h_path}...\n")
            generate_telemetry_header(telemetry, telemetry_h_path)
            self.log("✓ telemetry.h generated\n", "success")
            
            self.log("\n✓ All headers generated successfully!\n", "success")
            messagebox.showinfo("Success", "Both headers generated successfully!")
            
        except Exception as e:
            error_msg = f"✗ Error: {str(e)}\n"
            self.log(error_msg, "error")
            messagebox.showerror("Error", str(e))


def main():
    """Main entry point for the GUI application."""
    root = tk.Tk()
    app = CodeGeneratorGUI(root)
    root.mainloop()


if __name__ == '__main__':
    main()

