"""
PressBoi Device Simulator
Handles pressboi-specific command simulation and state updates.
"""
import time
import random


def handle_command(device_sim, command, args, gui_address):
    """
    Handle pressboi-specific commands.
    
    Args:
        device_sim: Reference to the DeviceSimulator instance
        command: Command string (e.g., "pressboi.home")
        args: List of command arguments
        gui_address: Tuple of (ip, port) for GUI
    
    Returns:
        True if command was handled, False to use default handler
    """
    # Normalize to lowercase for case-insensitive matching
    cmd_lower = command.lower()
    
    if cmd_lower == "pressboi.home":
        device_sim.set_state('MAIN_STATE', 'HOMING')
        device_sim.command_queue.append((simulate_homing, (device_sim, 2.0, gui_address, command)))
        return True
    
    elif cmd_lower == "pressboi.move":
        target = float(args[0]) if args else 0
        device_sim.set_state('MAIN_STATE', 'MOVING')
        device_sim.command_queue.append((simulate_move, (device_sim, target, 2.0, gui_address, command)))
        return True
    
    elif cmd_lower == "pressboi.set_retract":
        pos = float(args[0]) if args else 0
        device_sim.state['retract_pos'] = pos
        return False  # Send generic DONE response
    
    elif cmd_lower == "pressboi.retract":
        target = device_sim.state.get('retract_pos', 0.0)
        speed = float(args[0]) if args else 6.25  # Optional speed parameter
        duration = abs(target - device_sim.state.get('current_pos', 0.0)) / speed if speed > 0 else 2.0
        device_sim.set_state('MAIN_STATE', 'MOVING')
        device_sim.command_queue.append((simulate_move, (device_sim, target, duration, gui_address, command)))
        return True
    
    return False  # Command not handled, use default


def simulate_homing(device_sim, duration, gui_address, command):
    """Simulates pressboi homing process."""
    print(f"[pressboi] simulate_homing started, will sleep for {duration}s")
    time.sleep(duration)
    if device_sim._stop_event.is_set():
        print(f"[pressboi] simulate_homing aborted (stop event)")
        return
    
    device_sim.state['current_pos'] = 0.0
    device_sim.state['homed'] = 1
    device_sim.state['force_load_cell'] = 0.0
    device_sim.state['force_motor_torque'] = 0.0
    device_sim.state['force_source'] = "load_cell"
    device_sim.state['joules'] = 0.0  # Reset joules on home
    device_sim.set_state('MAIN_STATE', 'STANDBY')
    # Send generic DONE message that includes the original command for the script runner
    print(f"[pressboi] Sending DONE: {command} to {gui_address}")
    device_sim.sock.sendto(f"DONE: {command}".encode(), gui_address)
    print(f"[pressboi] Homing complete")


def simulate_move(device_sim, target, duration, gui_address, command):
    """Simulates pressboi move process with force simulation."""
    start_time = time.time()
    start_pos = device_sim.state.get('current_pos', 0.0)
    
    # Reset joules at start of move
    device_sim.state['joules'] = 0.0
    prev_pos = start_pos
    
    while time.time() - start_time < duration:
        elapsed = time.time() - start_time
        progress = elapsed / duration
        device_sim.state['current_pos'] = start_pos + (target - start_pos) * progress
        current_pos = device_sim.state['current_pos']
        
        # Simulate both force values during move (in kg)
        force_kg = random.uniform(50, 300)  # Simulate realistic pressing force
        device_sim.state['force_load_cell'] = force_kg
        device_sim.state['force_motor_torque'] = force_kg
        device_sim.state['force_source'] = "load_cell"  # Default to load cell mode
        # Simulate average torque
        device_sim.state['torque_avg'] = random.uniform(20, 60)
        
        # Calculate joules: Energy = Force (N) × Distance (m)
        # Force in kg → Newtons: kg × 9.81
        # Distance in mm → meters: mm × 0.001
        distance_mm = abs(current_pos - prev_pos)
        device_sim.state['joules'] += force_kg * distance_mm * 0.00981
        prev_pos = current_pos
        
        time.sleep(0.1)
        if device_sim._stop_event.is_set():
            return
    
    device_sim.state['current_pos'] = target
    device_sim.state['force_load_cell'] = 0
    device_sim.state['force_motor_torque'] = 0
    device_sim.state['force_source'] = "load_cell"
    device_sim.state['target_pos'] = target
    device_sim.state['torque_avg'] = 0
    # Joules persists after move completes, showing total energy expended
    device_sim.set_state('MAIN_STATE', 'STANDBY')
    # Send generic DONE message that includes the original command for the script runner
    device_sim.sock.sendto(f"DONE: {command}".encode(), gui_address)
    print(f"[pressboi] Move complete to {target}, Energy expended: {device_sim.state['joules']:.1f} J")


def update_state(device_sim):
    """
    Update pressboi dynamic state (called periodically).
    
    Args:
        device_sim: Reference to the DeviceSimulator instance
    """
    # Add any continuous state updates here
    # e.g., temperature drift, sensor noise, etc.
    pass


