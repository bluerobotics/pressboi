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


def create_operator_view(parent, shared_gui_refs, view_id=None):
    """
    Create pressboi operator view content.
    
    Args:
        parent: Parent frame to pack content into
        shared_gui_refs: Shared GUI references dictionary
        view_id: Optional view ID to determine which view to create
        
    Returns:
        Widget containing operator view content
    """
    # Route to appropriate view creator based on view_id
    if view_id == 'press_operator_view':
        return create_press_operator_view(parent, shared_gui_refs)
    elif view_id == 'injector_operator_view':
        return create_injector_operator_view(parent, shared_gui_refs)
    else:
        # Default/generic view
        return create_generic_operator_view(parent, shared_gui_refs)


def create_press_operator_view(parent, shared_gui_refs):
    """
    Create simplified press operator view with job/serial tracking and PASS/FAIL indicator.
    
    Args:
        parent: Parent frame to pack content into
        shared_gui_refs: Shared GUI references dictionary
        
    Returns:
        Widget containing operator view content
    """
    # Main container - fill parent
    container = tk.Frame(parent, bg=theme.BG_COLOR)
    container.pack(fill=tk.BOTH, expand=True)
    
    # Get serial manager from shared refs
    serial_manager = shared_gui_refs.get('serial_manager')
    
    # Job Number
    job_outer = tk.Frame(container, bg=theme.BG_COLOR)
    job_outer.pack(fill=tk.X, pady=15)
    
    job_frame = tk.Frame(job_outer, bg=theme.BG_COLOR)
    job_frame.pack()
    
    tk.Label(
        job_frame,
        text="Job Number:",
        font=(theme.FONT_FAMILY, 24, 'bold'),
        foreground='#B0A3D4',  # Lavender
        bg=theme.BG_COLOR
    ).pack(side=tk.LEFT, padx=(0, 20))
    
    job_var = tk.StringVar(value=serial_manager.get_job() if serial_manager else '')
    job_entry = tk.Entry(
        job_frame, 
        textvariable=job_var, 
        font=(theme.FONT_FAMILY, 24),
        width=25,
        bg=theme.WIDGET_BG,
        fg=theme.FG_COLOR,
        insertbackground=theme.PRIMARY_ACCENT,
        relief='solid',
        borderwidth=3
    )
    job_entry.pack(side=tk.LEFT)
    
    def on_job_changed(*args):
        if serial_manager:
            serial_manager.set_job(job_var.get())
    job_var.trace_add('write', on_job_changed)
    
    # Serial Number
    serial_outer = tk.Frame(container, bg=theme.BG_COLOR)
    serial_outer.pack(fill=tk.X, pady=15)
    
    serial_frame = tk.Frame(serial_outer, bg=theme.BG_COLOR)
    serial_frame.pack()
    
    tk.Label(
        serial_frame,
        text="Serial Number:",
        font=(theme.FONT_FAMILY, 24, 'bold'),
        foreground='#B0A3D4',  # Lavender
        bg=theme.BG_COLOR
    ).pack(side=tk.LEFT, padx=(0, 20))
    
    serial_var = tk.StringVar(value=serial_manager.get_serial() if serial_manager else '')
    serial_entry = tk.Entry(
        serial_frame, 
        textvariable=serial_var, 
        font=(theme.FONT_FAMILY, 24),
        width=25,
        bg=theme.WIDGET_BG,
        fg=theme.FG_COLOR,
        insertbackground=theme.PRIMARY_ACCENT,
        relief='solid',
        borderwidth=3
    )
    serial_entry.pack(side=tk.LEFT)
    
    def on_serial_changed(*args):
        if serial_manager:
            serial_manager.set_serial(serial_var.get())
    serial_var.trace_add('write', on_serial_changed)
    
    # Separator
    separator = tk.Frame(container, bg=theme.SECONDARY_ACCENT, height=3)
    separator.pack(fill=tk.X, pady=40, padx=100)
    
    # PASS/FAIL Indicator Section (BIG and centered)
    pass_fail_var = tk.StringVar(value='READY')
    pass_fail_label = tk.Label(
        container,
        textvariable=pass_fail_var,
        font=(theme.FONT_FAMILY, 180, 'bold'),
        foreground=theme.COMMENT_COLOR,
        bg=theme.BG_COLOR
    )
    pass_fail_label.pack(pady=(20, 40), expand=True)
    
    # Hook into script runner to track PASS/FAIL status
    script_runner = shared_gui_refs.get('script_runner')
    if script_runner:
        # Track previous script state to detect transitions
        prev_running = [False]
        prev_held = [False]
        
        def update_pass_fail():
            """Update PASS/FAIL indicator based on script state."""
            try:
                is_running = script_runner.is_running
                is_held = script_runner.is_held
                had_errors = getattr(script_runner, 'had_errors', False)
                
                # Detect state transitions
                just_stopped = prev_running[0] and not is_running
                just_held = not prev_held[0] and is_held
                
                if is_running and not is_held:
                    # Script is actively running
                    pass_fail_var.set('RUNNING')
                    pass_fail_label.config(foreground=theme.BUSY_BLUE)
                    print(f"[OPERATOR VIEW] PASS/FAIL: RUNNING")
                elif is_held or just_held:
                    # Script hit an error or warning
                    pass_fail_var.set('FAIL')
                    pass_fail_label.config(foreground=theme.ERROR_RED)
                    print(f"[OPERATOR VIEW] PASS/FAIL: FAIL")
                elif just_stopped:
                    # Script just finished
                    if had_errors:
                        pass_fail_var.set('FAIL')
                        pass_fail_label.config(foreground=theme.ERROR_RED)
                        print(f"[OPERATOR VIEW] PASS/FAIL: FAIL (had errors)")
                    else:
                        pass_fail_var.set('PASS')
                        pass_fail_label.config(foreground=theme.SUCCESS_GREEN)
                        print(f"[OPERATOR VIEW] PASS/FAIL: PASS")
                elif not is_running and not is_held and not had_errors:
                    # Script is idle/reset
                    if pass_fail_var.get() not in ['PASS', 'FAIL']:
                        pass_fail_var.set('READY')
                        pass_fail_label.config(foreground=theme.COMMENT_COLOR)
                
                # Update tracking
                prev_running[0] = is_running
                prev_held[0] = is_held
            except Exception as e:
                print(f"[OPERATOR VIEW] Error updating PASS/FAIL: {e}")
        
        # Poll script state periodically (every 100ms)
        def poll_state():
            try:
                update_pass_fail()
            except Exception as e:
                print(f"[OPERATOR VIEW] Polling error: {e}")
            # Schedule next poll if container still exists
            if container.winfo_exists():
                container.after(100, poll_state)
        
        # Start polling
        print(f"[OPERATOR VIEW] Starting PASS/FAIL polling, initial value: {pass_fail_var.get()}")
        poll_state()
    else:
        print(f"[OPERATOR VIEW] No script_runner found, PASS/FAIL will not update")
    
    return container


def create_injector_operator_view(parent, shared_gui_refs):
    """
    Create simplified injector operator view.
    
    Args:
        parent: Parent frame to pack content into
        shared_gui_refs: Shared GUI references dictionary
        
    Returns:
        Widget containing operator view content
    """
    # Main container
    container = ttk.Frame(parent, style='Card.TFrame')
    
    # Placeholder
    ttk.Label(
        container,
        text="Injector Operator View\n(Coming Soon)",
        font=(theme.FONT_FAMILY, 24, 'bold'),
        foreground='#B0A3D4',  # Lavender
        style='Subtle.TLabel'
    ).pack(expand=True, pady=50)
    
    return container


def create_generic_operator_view(parent, shared_gui_refs):
    """
    Create generic pressboi operator view content (original implementation).
    
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

