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
import time
import platform
import subprocess


def show_onscreen_keyboard():
    """Show the on-screen keyboard (Mac only)."""
    if platform.system() == "Darwin":
        try:
            # Use AppleScript to show the keyboard viewer from the input menu
            subprocess.Popen([
                "osascript", "-e",
                'tell application "System Events" to tell process "SystemUIServer" to click (menu bar item 1 of menu bar 1 whose description contains "text input")'
            ])
        except Exception as e:
            print(f"[OPERATOR VIEW] Could not open on-screen keyboard: {e}")

# Adjust path for standalone execution
if __name__ == "__main__":
    current_dir = os.path.dirname(os.path.abspath(__file__))
    parent_dir = os.path.dirname(os.path.dirname(current_dir))
    sys.path.insert(0, parent_dir)

from src import theme
from src.stats import get_stats, format_duration, format_cycle_time, format_yield


def show_stats_window(parent):
    """Show a popup window with cycle statistics."""
    stats = get_stats()
    all_stats = stats.get_all_stats()
    
    # Create popup window
    popup = tk.Toplevel(parent)
    popup.title("Cycle Statistics")
    popup.configure(bg=theme.BG_COLOR)
    popup.geometry("500x600")
    popup.resizable(False, False)
    
    # Center on parent
    popup.transient(parent)
    popup.grab_set()
    
    # Title
    title = tk.Label(
        popup,
        text="Cycle Statistics",
        font=(theme.FONT_FAMILY, 24, 'bold'),
        foreground=theme.PRIMARY_ACCENT,
        bg=theme.BG_COLOR
    )
    title.pack(pady=(20, 20))
    
    # Stats container
    stats_frame = tk.Frame(popup, bg=theme.BG_COLOR)
    stats_frame.pack(fill=tk.BOTH, expand=True, padx=30, pady=10)
    
    def add_stat_row(frame, label, value, row):
        """Add a label-value row to the stats display."""
        lbl = tk.Label(
            frame,
            text=label,
            font=(theme.FONT_FAMILY, 14),
            foreground=theme.COMMENT_COLOR,
            bg=theme.BG_COLOR,
            anchor='w'
        )
        lbl.grid(row=row, column=0, sticky='w', pady=5)
        
        val = tk.Label(
            frame,
            text=str(value),
            font=(theme.FONT_FAMILY, 14, 'bold'),
            foreground=theme.FG_COLOR,
            bg=theme.BG_COLOR,
            anchor='e'
        )
        val.grid(row=row, column=1, sticky='e', pady=5, padx=(20, 0))
    
    # Configure grid columns
    stats_frame.columnconfigure(0, weight=1)
    stats_frame.columnconfigure(1, weight=1)
    
    row = 0
    
    # Section: Operations
    section1 = tk.Label(stats_frame, text="── Operations ──", font=(theme.FONT_FAMILY, 12, 'bold'),
                        foreground=theme.SECONDARY_ACCENT, bg=theme.BG_COLOR)
    section1.grid(row=row, column=0, columnspan=2, pady=(10, 5), sticky='w')
    row += 1
    
    add_stat_row(stats_frame, "Since Host Boot:", str(all_stats['operations_since_boot']), row)
    row += 1
    add_stat_row(stats_frame, "Total:", str(all_stats['operations_total']), row)
    row += 1
    
    # Section: Cycle Times
    section2 = tk.Label(stats_frame, text="── Cycle Times ──", font=(theme.FONT_FAMILY, 12, 'bold'),
                        foreground=theme.SECONDARY_ACCENT, bg=theme.BG_COLOR)
    section2.grid(row=row, column=0, columnspan=2, pady=(15, 5), sticky='w')
    row += 1
    
    add_stat_row(stats_frame, "Last Cycle:", format_cycle_time(all_stats['last_cycle_time']), row)
    row += 1
    # This Job Avg
    this_job_label = f"This Job Avg ({all_stats['current_job']}):" if all_stats['current_job'] else "This Job Avg:"
    add_stat_row(stats_frame, this_job_label, format_cycle_time(all_stats['job_average_cycle_time']), row)
    row += 1
    # Last Job Avg (only show if we have a last job)
    if all_stats['last_job']:
        last_job_label = f"Last Job Avg ({all_stats['last_job']}):"
        add_stat_row(stats_frame, last_job_label, format_cycle_time(all_stats['last_job_average_cycle_time']), row)
        row += 1
    # Only show "Last 100" if we have at least 1 sample
    if all_stats['sample_size_100'] > 0:
        add_stat_row(stats_frame, f"Avg (Last {all_stats['sample_size_100']}):", 
                     format_cycle_time(all_stats['average_cycle_time_100']), row)
        row += 1
    # Only show "Last 1000" if we have at least 100 samples
    if all_stats['sample_size_1000'] >= 100:
        add_stat_row(stats_frame, f"Avg (Last {all_stats['sample_size_1000']}):", 
                     format_cycle_time(all_stats['average_cycle_time_1000']), row)
        row += 1
    add_stat_row(stats_frame, "Avg (Total):", format_cycle_time(all_stats['average_cycle_time_total']), row)
    row += 1
    
    # Section: Yield
    section3 = tk.Label(stats_frame, text="── Yield ──", font=(theme.FONT_FAMILY, 12, 'bold'),
                        foreground=theme.SECONDARY_ACCENT, bg=theme.BG_COLOR)
    section3.grid(row=row, column=0, columnspan=2, pady=(15, 5), sticky='w')
    row += 1
    
    # This Job
    this_job_yield_label = f"This Job ({all_stats['current_job']}):" if all_stats['current_job'] else "This Job:"
    add_stat_row(stats_frame, this_job_yield_label, format_yield(all_stats['yield_job']), row)
    row += 1
    # Last Job (only show if we have a last job)
    if all_stats['last_job']:
        last_job_yield_label = f"Last Job ({all_stats['last_job']}):"
        add_stat_row(stats_frame, last_job_yield_label, format_yield(all_stats['yield_last_job']), row)
        row += 1
    # Only show "Last 100" if we have at least 1 sample
    if all_stats['sample_size_100'] > 0:
        add_stat_row(stats_frame, f"Last {all_stats['sample_size_100']}:", format_yield(all_stats['yield_100']), row)
        row += 1
    # Only show "Last 1000" if we have at least 100 samples
    if all_stats['sample_size_1000'] >= 100:
        add_stat_row(stats_frame, f"Last {all_stats['sample_size_1000']}:", format_yield(all_stats['yield_1000']), row)
        row += 1
    add_stat_row(stats_frame, "Total:", format_yield(all_stats['yield_total']), row)
    row += 1
    
    # Close button
    close_btn = tk.Button(
        popup,
        text="Close",
        command=popup.destroy,
        font=(theme.FONT_FAMILY, 14, 'bold'),
        bg=theme.WIDGET_BG,
        fg=theme.FG_COLOR,
        activebackground='#444444',
        activeforeground=theme.FG_COLOR,
        relief='raised',
        borderwidth=2,
        padx=30,
        pady=8,
        cursor='hand2'
    )
    close_btn.pack(pady=(10, 20))
    
    # Center window on screen
    popup.update_idletasks()
    width = popup.winfo_width()
    height = popup.winfo_height()
    x = (popup.winfo_screenwidth() // 2) - (width // 2)
    y = (popup.winfo_screenheight() // 2) - (height // 2)
    popup.geometry(f'{width}x{height}+{x}+{y}')




def create_operator_view(parent, shared_gui_refs, view_id=None):
    """
    Create pressboi operator view content directly in parent frame.
    
    Args:
        parent: Parent frame to pack content into (should be already packed)
        shared_gui_refs: Shared GUI references dictionary
        view_id: Optional view ID to determine which view to create
    """
    print(f"[PRESSBOI OPERATOR_VIEW] create_operator_view called with view_id={view_id}")
    print(f"[PRESSBOI OPERATOR_VIEW] parent={parent}")
    
    # Route to appropriate view creator based on view_id
    if view_id == 'press_operator_view':
        print(f"[PRESSBOI OPERATOR_VIEW] Creating press operator view")
        create_press_operator_view(parent, shared_gui_refs)
    elif view_id == 'injector_operator_view':
        print(f"[PRESSBOI OPERATOR_VIEW] Creating injector operator view")
        create_injector_operator_view(parent, shared_gui_refs)
    else:
        print(f"[PRESSBOI OPERATOR_VIEW] Creating generic operator view")
        # Default/generic view
        create_generic_operator_view(parent, shared_gui_refs)


def create_press_operator_view(parent, shared_gui_refs):
    """
    Create simplified press operator view with job/serial tracking and PASS/FAIL indicator.
    Creates widgets directly in the parent frame.
    
    Args:
        parent: Parent frame to pack content into (should be already packed)
        shared_gui_refs: Shared GUI references dictionary
    """
    print(f"[PRESS OPERATOR VIEW] Creating press operator view")
    print(f"[PRESS OPERATOR VIEW] parent={parent}, exists={parent.winfo_exists()}")
    
    # Get serial manager from shared refs
    serial_manager = shared_gui_refs.get('serial_manager')
    
    # PASS/FAIL Indicator Section (BIG and at the top)
    pass_fail_var = tk.StringVar(value='READY')
    pass_fail_label = tk.Label(
        parent,
        textvariable=pass_fail_var,
        font=(theme.FONT_FAMILY, 180, 'bold'),
        foreground=theme.COMMENT_COLOR,
        bg=theme.BG_COLOR
    )
    pass_fail_label.pack(pady=(40, 20))
    
    # Error/Warning display (centered, below PASS/FAIL) - only shows when there's an error/warning
    error_warning_var = tk.StringVar(value='')
    error_warning_label = tk.Label(
        parent,
        textvariable=error_warning_var,
        font=(theme.FONT_FAMILY, 24),
        foreground=theme.ERROR_RED,
        bg=theme.BG_COLOR
    )
    error_warning_label.pack(pady=(0, 40))
    
    # Separator after PASS/FAIL and error display
    separator_top = tk.Frame(parent, bg=theme.SECONDARY_ACCENT, height=3)
    separator_top.pack(fill=tk.X, pady=(0, 40), padx=100)
    
    # Input fields container - single grid for proper vertical alignment
    inputs_frame = tk.Frame(parent, bg=theme.BG_COLOR)
    inputs_frame.pack(pady=15)
    
    # Fixed label width for alignment (in characters)
    label_width = 15
    # Fixed button dimensions
    button_width = 14
    entry_width = 25
    # Common height styling for entries and buttons - make them taller
    entry_ipady = 18  # Internal padding to match button height
    button_ipady = 12  # Internal padding for buttons
    
    # Job Number Row
    job_label = tk.Label(
        inputs_frame,
        text="Job Number:",
        font=(theme.FONT_FAMILY, 24, 'bold'),
        foreground='white',
        bg=theme.BG_COLOR,
        anchor='e',
        width=label_width
    )
    job_label.grid(row=0, column=0, padx=(0, 20), pady=15, sticky='e')
    
    job_var = tk.StringVar(value=serial_manager.get_job() if serial_manager else '')
    job_entry = tk.Entry(
        inputs_frame, 
        textvariable=job_var, 
        font=(theme.FONT_FAMILY, 24),
        width=entry_width,
        bg=theme.WIDGET_BG,
        fg=theme.FG_COLOR,
        insertbackground=theme.PRIMARY_ACCENT,
        relief='solid',
        borderwidth=2,
        highlightthickness=2,
        highlightbackground='#555555',
        highlightcolor=theme.PRIMARY_ACCENT
    )
    job_entry.grid(row=0, column=1, padx=(0, 20), pady=15, ipady=entry_ipady, sticky='ew')
    
    # Show on-screen keyboard when entry is focused (Mac only)
    if platform.system() == "Darwin":
        job_entry.bind('<FocusIn>', lambda e: show_onscreen_keyboard())
    
    # Scanner target button for Job
    job_scanner_btn = tk.Button(
        inputs_frame,
        text="Scan Here",
        font=(theme.FONT_FAMILY, 20, 'bold'),
        relief='raised',
        borderwidth=2,
        padx=25,
        cursor='hand2',
        width=button_width
    )
    job_scanner_btn.grid(row=0, column=2, pady=15, ipady=button_ipady, sticky='nsew')
    
    def on_job_changed(*args):
        if serial_manager:
            serial_manager.set_job(job_var.get())
    job_var.trace_add('write', on_job_changed)
    
    # Serial Number Row
    serial_label = tk.Label(
        inputs_frame,
        text="Serial Number:",
        font=(theme.FONT_FAMILY, 24, 'bold'),
        foreground='white',
        bg=theme.BG_COLOR,
        anchor='e',
        width=label_width
    )
    serial_label.grid(row=1, column=0, padx=(0, 20), pady=15, sticky='e')
    
    serial_var = tk.StringVar(value=serial_manager.get_serial() if serial_manager else '')
    serial_entry = tk.Entry(
        inputs_frame, 
        textvariable=serial_var, 
        font=(theme.FONT_FAMILY, 24),
        width=entry_width,
        bg=theme.WIDGET_BG,
        fg=theme.FG_COLOR,
        insertbackground=theme.PRIMARY_ACCENT,
        relief='solid',
        borderwidth=2,
        highlightthickness=2,
        highlightbackground='#555555',
        highlightcolor=theme.PRIMARY_ACCENT
    )
    serial_entry.grid(row=1, column=1, padx=(0, 20), pady=15, ipady=entry_ipady, sticky='ew')
    
    # Show on-screen keyboard when entry is focused (Mac only)
    if platform.system() == "Darwin":
        serial_entry.bind('<FocusIn>', lambda e: show_onscreen_keyboard())
    
    # Scanner target button for Serial
    serial_scanner_btn = tk.Button(
        inputs_frame,
        text="Scan Here",
        font=(theme.FONT_FAMILY, 20, 'bold'),
        relief='raised',
        borderwidth=2,
        padx=25,
        cursor='hand2',
        width=button_width
    )
    serial_scanner_btn.grid(row=1, column=2, pady=15, ipady=button_ipady, sticky='nsew')
    
    # Update button appearances based on scanner target
    def update_scanner_buttons():
        current_target = serial_manager.get_scanner_target() if serial_manager else 'job'
        print(f"[OPERATOR VIEW] Updating button states, current target: {current_target}")
        if current_target == 'job':
            job_scanner_btn.config(
                bg=theme.SUCCESS_GREEN, 
                fg='white', 
                activebackground=theme.SUCCESS_GREEN,
                activeforeground='white',
                relief='sunken'
            )
            serial_scanner_btn.config(
                bg=theme.WIDGET_BG, 
                fg=theme.FG_COLOR,
                activebackground='#444444',
                activeforeground=theme.FG_COLOR,
                relief='raised'
            )
        else:  # serial
            job_scanner_btn.config(
                bg=theme.WIDGET_BG, 
                fg=theme.FG_COLOR,
                activebackground='#444444',
                activeforeground=theme.FG_COLOR,
                relief='raised'
            )
            serial_scanner_btn.config(
                bg=theme.SUCCESS_GREEN, 
                fg='white',
                activebackground=theme.SUCCESS_GREEN,
                activeforeground='white',
                relief='sunken'
            )
    
    # Scanner target button handlers
    def set_scanner_to_job():
        print(f"[OPERATOR VIEW] Job button clicked")
        if serial_manager:
            serial_manager.set_scanner_target('job')
            print(f"[OPERATOR VIEW] Scanner target set to: job")
            update_scanner_buttons()
        else:
            print(f"[OPERATOR VIEW] No serial_manager available")
    
    def set_scanner_to_serial():
        print(f"[OPERATOR VIEW] Serial button clicked")
        if serial_manager:
            serial_manager.set_scanner_target('serial')
            print(f"[OPERATOR VIEW] Scanner target set to: serial")
            update_scanner_buttons()
        else:
            print(f"[OPERATOR VIEW] No serial_manager available")
    
    job_scanner_btn.config(command=set_scanner_to_job)
    serial_scanner_btn.config(command=set_scanner_to_serial)
    
    # Set initial button states
    print(f"[OPERATOR VIEW] Setting initial button states")
    update_scanner_buttons()
    
    def on_serial_changed(*args):
        if serial_manager:
            serial_manager.set_serial(serial_var.get())
    serial_var.trace_add('write', on_serial_changed)
    
    # Cycle time display (centered, below inputs)
    cycle_time_frame = tk.Frame(parent, bg=theme.BG_COLOR)
    cycle_time_frame.pack(pady=(30, 10))
    
    cycle_time_title = tk.Label(
        cycle_time_frame,
        text="Cycle Time:",
        font=(theme.FONT_FAMILY, 20, 'bold'),
        foreground=theme.COMMENT_COLOR,
        bg=theme.BG_COLOR
    )
    cycle_time_title.pack(side=tk.LEFT, padx=(0, 10))
    
    cycle_time_var = tk.StringVar(value='--:--')
    cycle_time_label = tk.Label(
        cycle_time_frame,
        textvariable=cycle_time_var,
        font=(theme.FONT_FAMILY, 20),
        foreground=theme.PRIMARY_ACCENT,
        bg=theme.BG_COLOR
    )
    cycle_time_label.pack(side=tk.LEFT)
    
    # Track cycle start time
    cycle_start_time = [None]
    
    # Hook into script runner to track PASS/FAIL status
    # Note: script_runner is created fresh each time a script runs, so we must
    # get a fresh reference from shared_gui_refs each poll cycle
    
    # Track previous script state to detect transitions
    prev_running = [False]
    prev_held = [False]
    
    def update_pass_fail():
        """Update PASS/FAIL indicator, cycle time, and status bar based on script state."""
        try:
            # Get fresh script_runner reference each time (it changes when scripts run)
            script_runner = shared_gui_refs.get('script_runner')
            
            if script_runner is None:
                # No script running, show READY if not already showing PASS/FAIL
                is_running = False
                is_held = False
                had_errors = False
            else:
                is_running = script_runner.is_running
                is_held = script_runner.is_held
                had_errors = getattr(script_runner, 'had_errors', False)
            
            # Detect state transitions
            just_started = not prev_running[0] and is_running
            just_stopped = prev_running[0] and not is_running
            just_held = not prev_held[0] and is_held
            
            # Track cycle time
            if just_started:
                # Script just started - record start time
                cycle_start_time[0] = time.time()
                cycle_time_var.set('00:00')
                cycle_time_label.config(foreground=theme.BUSY_BLUE)
            elif is_running and not is_held and cycle_start_time[0] is not None:
                # Script is running - update elapsed time
                elapsed = time.time() - cycle_start_time[0]
                mins = int(elapsed // 60)
                secs = int(elapsed % 60)
                cycle_time_var.set(f'{mins:02d}:{secs:02d}')
            elif (just_stopped or just_held) and cycle_start_time[0] is not None:
                # Script just finished or held - show final time
                elapsed = time.time() - cycle_start_time[0]
                mins = int(elapsed // 60)
                secs = int(elapsed % 60)
                cycle_time_var.set(f'{mins:02d}:{secs:02d}')
                if had_errors or is_held:
                    cycle_time_label.config(foreground=theme.ERROR_RED)
                else:
                    cycle_time_label.config(foreground=theme.SUCCESS_GREEN)
            
            if is_running and not is_held:
                # Script is actively running
                pass_fail_var.set('RUNNING')
                pass_fail_label.config(foreground=theme.BUSY_BLUE)
            elif is_held or just_held:
                # Script hit an error or warning
                pass_fail_var.set('FAIL')
                pass_fail_label.config(foreground=theme.ERROR_RED)
            elif just_stopped:
                # Script just finished
                if had_errors:
                    pass_fail_var.set('FAIL')
                    pass_fail_label.config(foreground=theme.ERROR_RED)
                else:
                    pass_fail_var.set('PASS')
                    pass_fail_label.config(foreground=theme.SUCCESS_GREEN)
            elif not is_running and not is_held:
                # Script is idle/reset - only change to READY if not showing PASS/FAIL
                if pass_fail_var.get() not in ['PASS', 'FAIL']:
                    pass_fail_var.set('READY')
                    pass_fail_label.config(foreground=theme.COMMENT_COLOR)
            
            # Update tracking
            prev_running[0] = is_running
            prev_held[0] = is_held
            
            # Update error/warning display from main app's status_var - only show errors/warnings
            main_status_var = shared_gui_refs.get('status_var')
            if main_status_var:
                status_text = main_status_var.get()
                
                # Check if reset was performed - clear FAIL, error message, and cycle time
                if 'reset' in status_text.lower() and ('complete' in status_text.lower() or 'DONE' in status_text):
                    pass_fail_var.set('READY')
                    pass_fail_label.config(foreground=theme.COMMENT_COLOR)
                    error_warning_var.set('')
                    cycle_time_var.set('--:--')
                    cycle_time_label.config(foreground=theme.PRIMARY_ACCENT)
                    cycle_start_time[0] = None
                # Only show errors and warnings
                elif '_ERROR:' in status_text or '_WARNING:' in status_text or 'ERROR:' in status_text or 'WARNING:' in status_text:
                    # Strip device prefix and ERROR:/WARNING: prefix, just show the message
                    display_text = status_text
                    if '_ERROR:' in display_text:
                        # Find and remove everything up to and including _ERROR:
                        idx = display_text.find('_ERROR:')
                        display_text = display_text[idx + 7:].strip()
                    elif '_WARNING:' in display_text:
                        # Find and remove everything up to and including _WARNING:
                        idx = display_text.find('_WARNING:')
                        display_text = display_text[idx + 9:].strip()
                    elif 'ERROR:' in display_text:
                        idx = display_text.find('ERROR:')
                        display_text = display_text[idx + 6:].strip()
                    elif 'WARNING:' in display_text:
                        idx = display_text.find('WARNING:')
                        display_text = display_text[idx + 8:].strip()
                    error_warning_var.set(display_text)
                    error_warning_label.config(foreground=theme.ERROR_RED)
                elif pass_fail_var.get() == 'READY':
                    # Clear the error/warning when back to READY state
                    error_warning_var.set('')
                    
        except Exception as e:
            print(f"[OPERATOR VIEW] Error updating PASS/FAIL: {e}")
    
    # Poll script state periodically (every 100ms)
    def poll_state():
        try:
            update_pass_fail()
        except Exception as e:
            print(f"[OPERATOR VIEW] Polling error: {e}")
        # Schedule next poll if parent still exists
        if parent.winfo_exists():
            parent.after(100, poll_state)
    
    # Start polling
    print(f"[OPERATOR VIEW] Starting PASS/FAIL polling, initial value: {pass_fail_var.get()}")
    poll_state()
    
    print(f"[PRESS OPERATOR VIEW] Content created directly in parent frame")
    print(f"[PRESS OPERATOR VIEW] Parent has {len(parent.winfo_children())} children")


def create_injector_operator_view(parent, shared_gui_refs):
    """
    Create simplified injector operator view.
    Creates widgets directly in the parent frame.
    
    Args:
        parent: Parent frame to pack content into (should be already packed)
        shared_gui_refs: Shared GUI references dictionary
    """
    # Placeholder
    tk.Label(
        parent,
        text="Injector Operator View\n(Coming Soon)",
        font=(theme.FONT_FAMILY, 24, 'bold'),
        foreground='#B0A3D4',  # Lavender
        bg=theme.BG_COLOR
    ).pack(expand=True, pady=50)


def create_generic_operator_view(parent, shared_gui_refs):
    """
    Create generic pressboi operator view content (placeholder for future use).
    Creates widgets directly in the parent frame.
    
    Args:
        parent: Parent frame to pack content into (should be already packed)
        shared_gui_refs: Shared GUI references dictionary
    """
    # Placeholder
    tk.Label(
        parent,
        text="Generic Operator View\n(Use 'press operator view' or 'injector operator view')",
        font=(theme.FONT_FAMILY, 20, 'bold'),
        foreground='#B0A3D4',  # Lavender
        bg=theme.BG_COLOR
    ).pack(expand=True, pady=50)


if __name__ == "__main__":
    # Test the operator view
    root = tk.Tk()
    root.title("Pressboi Operator View Test")
    root.geometry("800x600")
    root.configure(bg=theme.BG_COLOR)
    
    # Create test shared GUI refs
    test_refs = {}
    
    # Create test container
    container = tk.Frame(root, bg=theme.BG_COLOR)
    container.pack(fill=tk.BOTH, expand=True)
    
    # Create press operator view
    create_press_operator_view(container, test_refs)
    
    root.mainloop()
