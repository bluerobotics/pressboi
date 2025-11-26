"""
Pressboi Operator View - Simplified operator interface for pressboi.

This provides a streamlined view focused on essential press operation:
- Large force display
- Position indicators
- State status
- Essential controls
"""

import tkinter as tk
from tkinter import ttk
import sys
import os

# Adjust path for standalone execution
if __name__ == "__main__":
    current_dir = os.path.dirname(os.path.abspath(__file__))
    parent_dir = os.path.dirname(os.path.dirname(current_dir))
    sys.path.insert(0, parent_dir)

from src import theme


def create_operator_view(parent, shared_gui_refs):
    """
    Create pressboi operator view content.
    
    Args:
        parent: Parent frame to pack content into
        shared_gui_refs: Shared GUI references dictionary
        
    Returns:
        Widget containing operator view content
    """
    # Main container
    container = ttk.Frame(parent, style='Card.TFrame')
    
    # Initialize variables if not present
    var_names = [
        'pressboi_force_var', 'pressboi_force_limit_var',
        'pressboi_current_pos_var', 'pressboi_target_pos_var',
        'pressboi_main_state_var', 'pressboi_homed_var',
        'pressboi_force_source_var'
    ]
    
    for var_name in var_names:
        if var_name not in shared_gui_refs:
            shared_gui_refs[var_name] = tk.StringVar(value='---')
            
    # Top section: Large force display
    force_section = ttk.Frame(container, style='Card.TFrame', padding=20)
    force_section.pack(fill=tk.X, pady=(0, 10))
    
    # Force label
    force_label = ttk.Label(
        force_section,
        text="FORCE",
        font=(theme.FONT_FAMILY, 14, 'bold'),
        foreground='#B0A3D4',  # Lavender
        style='Subtle.TLabel'
    )
    force_label.pack()
    
    # Large force value
    force_value = ttk.Label(
        force_section,
        textvariable=shared_gui_refs['pressboi_force_var'],
        font=(theme.FONT_FAMILY, 48, 'bold'),
        foreground=theme.PRIMARY_ACCENT,
        style='Subtle.TLabel'
    )
    force_value.pack(pady=(10, 0))
    
    # Force limit
    force_limit_frame = ttk.Frame(force_section, style='Card.TFrame')
    force_limit_frame.pack(pady=(5, 0))
    
    ttk.Label(
        force_limit_frame,
        text="Limit: ",
        font=theme.FONT_NORMAL,
        foreground=theme.COMMENT_COLOR,
        style='Subtle.TLabel'
    ).pack(side=tk.LEFT)
    
    ttk.Label(
        force_limit_frame,
        textvariable=shared_gui_refs['pressboi_force_limit_var'],
        font=theme.FONT_BOLD,
        foreground=theme.COMMENT_COLOR,
        style='Subtle.TLabel'
    ).pack(side=tk.LEFT)
    
    # Force source indicator
    force_source_label = ttk.Label(
        force_section,
        text="",
        font=theme.FONT_SMALL,
        foreground=theme.COMMENT_COLOR,
        style='Subtle.TLabel'
    )
    force_source_label.pack(pady=(5, 0))
    
    def update_force_source(*args):
        try:
            source = shared_gui_refs['pressboi_force_source_var'].get()
            if source == "load_cell":
                force_source_label.config(text="[Load Cell]", foreground=theme.SUCCESS_GREEN)
            elif source == "motor_torque":
                force_source_label.config(text="[Motor Torque]", foreground=theme.WARNING_YELLOW)
            else:
                force_source_label.config(text="[---]", foreground=theme.COMMENT_COLOR)
        except tk.TclError:
            pass
            
    shared_gui_refs['pressboi_force_source_var'].trace_add('write', update_force_source)
    update_force_source()
    
    # Add force color tracer
    def force_color_tracer(*args):
        try:
            value_str = shared_gui_refs['pressboi_force_var'].get().split()[0]
            force = float(value_str)
            if force < 100:
                color = theme.FG_COLOR
            elif force < 500:
                color = theme.SUCCESS_GREEN
            elif force < 800:
                color = theme.WARNING_YELLOW
            else:
                color = theme.ERROR_RED
            force_value.config(foreground=color)
        except (ValueError, IndexError, tk.TclError):
            force_value.config(foreground=theme.PRIMARY_ACCENT)
            
    shared_gui_refs['pressboi_force_var'].trace_add('write', force_color_tracer)
    force_color_tracer()
    
    # Middle section: Position display
    position_section = ttk.Frame(container, style='Card.TFrame', padding=20)
    position_section.pack(fill=tk.X, pady=(0, 10))
    
    # Position grid
    pos_grid = ttk.Frame(position_section, style='Card.TFrame')
    pos_grid.pack()
    
    # Current position
    current_col = ttk.Frame(pos_grid, style='Card.TFrame', padding=10)
    current_col.grid(row=0, column=0, padx=10)
    
    ttk.Label(
        current_col,
        text="CURRENT",
        font=theme.FONT_BOLD,
        foreground='#B0A3D4',  # Lavender
        style='Subtle.TLabel'
    ).pack()
    
    # Strip units for cleaner display
    current_display = tk.StringVar(value='0.00')
    
    def update_current(*args):
        try:
            val = shared_gui_refs['pressboi_current_pos_var'].get()
            if ' mm' in val:
                val = val.replace(' mm', '')
            current_display.set(val)
        except:
            current_display.set('---')
            
    shared_gui_refs['pressboi_current_pos_var'].trace_add('write', update_current)
    update_current()
    
    current_value = ttk.Label(
        current_col,
        textvariable=current_display,
        font=(theme.FONT_FAMILY, 32, 'bold'),
        foreground=theme.SUCCESS_GREEN,
        style='Subtle.TLabel'
    )
    current_value.pack(pady=(5, 0))
    
    ttk.Label(
        current_col,
        text="mm",
        font=theme.FONT_NORMAL,
        foreground=theme.COMMENT_COLOR,
        style='Subtle.TLabel'
    ).pack()
    
    # Arrow
    ttk.Label(
        pos_grid,
        text="→",
        font=(theme.FONT_FAMILY, 32),
        foreground=theme.COMMENT_COLOR,
        style='Subtle.TLabel'
    ).grid(row=0, column=1, padx=10)
    
    # Target position
    target_col = ttk.Frame(pos_grid, style='Card.TFrame', padding=10)
    target_col.grid(row=0, column=2, padx=10)
    
    ttk.Label(
        target_col,
        text="TARGET",
        font=theme.FONT_BOLD,
        foreground='#B0A3D4',  # Lavender
        style='Subtle.TLabel'
    ).pack()
    
    # Strip units for cleaner display
    target_display = tk.StringVar(value='0.00')
    
    def update_target(*args):
        try:
            val = shared_gui_refs['pressboi_target_pos_var'].get()
            if ' mm' in val:
                val = val.replace(' mm', '')
            target_display.set(val)
        except:
            target_display.set('---')
            
    shared_gui_refs['pressboi_target_pos_var'].trace_add('write', update_target)
    update_target()
    
    target_value = ttk.Label(
        target_col,
        textvariable=target_display,
        font=(theme.FONT_FAMILY, 32, 'bold'),
        foreground=theme.WARNING_YELLOW,
        style='Subtle.TLabel'
    )
    target_value.pack(pady=(5, 0))
    
    ttk.Label(
        target_col,
        text="mm",
        font=theme.FONT_NORMAL,
        foreground=theme.COMMENT_COLOR,
        style='Subtle.TLabel'
    ).pack()
    
    # Add position color tracer
    def position_color_tracer(*args):
        try:
            homed_state = shared_gui_refs['pressboi_homed_var'].get()
            main_state = shared_gui_refs['pressboi_main_state_var'].get().upper()
            
            # If not homed, everything is red
            if homed_state != 'homed':
                current_value.config(foreground=theme.ERROR_RED)
                target_value.config(foreground=theme.ERROR_RED)
                return
                
            # Parse positions
            current_str = current_display.get()
            target_str = target_display.get()
            current_pos = float(current_str) if current_str not in ['---', ''] else 0.0
            target_pos = float(target_str) if target_str not in ['---', ''] else 0.0
            
            # Check if at target
            at_target = abs(current_pos - target_pos) < 0.5
            
            # Check if moving
            is_moving = 'MOVING' in main_state or 'HOMING' in main_state
            
            if is_moving:
                current_value.config(foreground=theme.BUSY_BLUE)
                target_value.config(foreground=theme.WARNING_YELLOW)
            elif at_target:
                current_value.config(foreground=theme.SUCCESS_GREEN)
                target_value.config(foreground=theme.SUCCESS_GREEN)
            else:
                current_value.config(foreground=theme.SUCCESS_GREEN)
                target_value.config(foreground=theme.WARNING_YELLOW)
        except:
            pass
            
    shared_gui_refs['pressboi_homed_var'].trace_add('write', position_color_tracer)
    shared_gui_refs['pressboi_main_state_var'].trace_add('write', position_color_tracer)
    shared_gui_refs['pressboi_current_pos_var'].trace_add('write', position_color_tracer)
    shared_gui_refs['pressboi_target_pos_var'].trace_add('write', position_color_tracer)
    position_color_tracer()
    
    # Bottom section: State status
    status_section = ttk.Frame(container, style='Card.TFrame', padding=20)
    status_section.pack(fill=tk.X)
    
    # State display
    state_frame = ttk.Frame(status_section, style='Card.TFrame')
    state_frame.pack()
    
    ttk.Label(
        state_frame,
        text="STATE:",
        font=theme.FONT_BOLD,
        foreground='#B0A3D4',  # Lavender
        style='Subtle.TLabel'
    ).pack(side=tk.LEFT, padx=(0, 10))
    
    state_value = ttk.Label(
        state_frame,
        textvariable=shared_gui_refs['pressboi_main_state_var'],
        font=(theme.FONT_FAMILY, 18, 'bold'),
        foreground=theme.FG_COLOR,
        style='Subtle.TLabel'
    )
    state_value.pack(side=tk.LEFT)
    
    # Add state color tracer
    def state_color_tracer(*args):
        try:
            state = shared_gui_refs['pressboi_main_state_var'].get().upper()
            color = theme.FG_COLOR
            if "STANDBY" in state or "IDLE" in state:
                color = theme.SUCCESS_GREEN
            elif "BUSY" in state or "ACTIVE" in state or "MOVING" in state or "PRESSING" in state:
                color = theme.BUSY_BLUE
            elif "ERROR" in state:
                color = theme.ERROR_RED
            elif "DISABLED" in state:
                color = theme.ERROR_RED
            state_value.config(foreground=color)
        except tk.TclError:
            pass
            
    shared_gui_refs['pressboi_main_state_var'].trace_add('write', state_color_tracer)
    state_color_tracer()
    
    # Homed status
    homed_frame = ttk.Frame(status_section, style='Card.TFrame')
    homed_frame.pack(pady=(10, 0))
    
    homed_value = ttk.Label(
        homed_frame,
        textvariable=shared_gui_refs['pressboi_homed_var'],
        font=theme.FONT_BOLD,
        foreground=theme.FG_COLOR,
        style='Subtle.TLabel'
    )
    homed_value.pack()
    
    def homed_color_tracer(*args):
        try:
            state = shared_gui_refs['pressboi_homed_var'].get()
            if state == 'homed':
                homed_value.config(foreground=theme.SUCCESS_GREEN)
            else:
                homed_value.config(foreground=theme.ERROR_RED)
        except tk.TclError:
            pass
            
    shared_gui_refs['pressboi_homed_var'].trace_add('write', homed_color_tracer)
    homed_color_tracer()
    
    return container


if __name__ == "__main__":
    # Test the operator view
    root = tk.Tk()
    root.title("Pressboi Operator View Test")
    root.geometry("800x600")
    root.configure(bg=theme.BG_COLOR)
    
    # Create test variables
    test_refs = {
        'pressboi_force_var': tk.StringVar(value='450.2 kg'),
        'pressboi_force_limit_var': tk.StringVar(value='1200 kg'),
        'pressboi_current_pos_var': tk.StringVar(value='25.5 mm'),
        'pressboi_target_pos_var': tk.StringVar(value='50.0 mm'),
        'pressboi_main_state_var': tk.StringVar(value='STANDBY'),
        'pressboi_homed_var': tk.StringVar(value='homed'),
        'pressboi_force_source_var': tk.StringVar(value='load_cell')
    }
    
    content = create_operator_view(root, test_refs)
    content.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
    
    root.mainloop()

