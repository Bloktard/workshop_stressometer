from serial.tools import list_ports
import serial
import time
import threading
import pygame
import speech_recognition as sr
import sys
import math

# Shared variables and lock
stress = 0  # Current smoothed stress
target_stress = 0  # Target stress from serial data
base_stress = 0  # Permanent base level of stress
stress_lock = threading.Lock()

# Variables to track sensor history
last_z = None  # Last accelerometer z-value
last_db = None  # Last decibel value
proximity_buffer = []  # Buffer for proximity values
peak_stress = 0  # Track peak stress for base level adjustment

def get_data():
    global target_stress, base_stress, last_z, last_db, proximity_buffer, peak_stress

    # Identify the correct port
    ports = list_ports.comports()
    for port in ports:
        print(port)

    # Open the serial com (adjust the port as needed)
    try:
        serialCom = serial.Serial("/dev/ttyACM0", 9600)
    except serial.SerialException as e:
        print(f"Failed to open serial port: {e}")
        return

    # Toggle DTR to reset the Arduino
    serialCom.setDTR(False)
    time.sleep(1)
    serialCom.flushInput()
    serialCom.setDTR(True)

    # Buffer to accumulate multi-line data
    data_buffer = []

    while True:
        try:
            # Read the line from serial
            s_bytes = serialCom.readline()
            decoded_bytes = s_bytes.decode("utf-8").strip('\r\n')
            data_buffer.append(decoded_bytes)
            print(f"Received: {decoded_bytes}")  # Debug raw input

            # Check if we’ve reached the end of a prediction block
            if "|" in decoded_bytes:  # Assuming accelerometer data marks the end
                with stress_lock:
                    stress_modifier = 0
                    significant_change = False

                    # Parse the buffered prediction data
                    apaisant = None
                    stress_val = None
                    for line in data_buffer:
                        if "Apaisant:" in line:
                            apaisant = float(line.split(':')[1].strip())
                        if "Stress:" in line:
                            stress_val = float(line.split(':')[1].strip())

                    # Process Apaisant and Stress if available
                    if apaisant is not None and stress_val is not None:
                        print(f"Parsed - Apaisant: {apaisant:.5f}, Stress: {stress_val:.5f}")
                        
                        # High Apaisant reduces stress
                        if apaisant > 0.5:  # Significant calming
                            stress_modifier -= apaisant * 50
                            significant_change = True
                        elif apaisant > 0.2:  # Moderate calming
                            stress_modifier -= apaisant * 20
                            significant_change = True

                        # High Stress increases stress
                        if stress_val > 0.5:  # Significant stress
                            stress_modifier += stress_val * 50
                            significant_change = True
                        elif stress_val > 0.2:  # Moderate stress
                            stress_modifier += stress_val * 20
                            significant_change = True

                    # Parse accelerometer data (e.g., "| 1.04, 0.24, -0.06 0 0")
                    accel_data = [val.strip().replace('|', '') for val in decoded_bytes.split(",")]
                    parsed_values = []
                    for val in accel_data:
                        try:
                            cleaned_val = val.split()[0]
                            parsed_values.append(float(cleaned_val))
                        except (ValueError, IndexError):
                            continue

                    if len(parsed_values) >= 3:  # Accelerometer (x, y, z)
                        x, y, z = parsed_values[:3]
                        if last_z is not None:
                            z_change = abs(z - last_z)
                            if z_change > 0.1:  # Threshold for movement
                                stress_modifier += z_change * 1
                                significant_change = True
                        last_z = z
                        print(f"Accelerometer - X: {x:.2f}, Y: {y:.2f}, Z: {z:.2f}")

                    # Apply the modifier
                    target_stress += stress_modifier

                    # Update peak stress for base level adjustment (with a cap)
                    if target_stress > peak_stress:
                        peak_stress = target_stress
                        base_stress += peak_stress * 0.05  # Small increase
                        base_stress = min(50, base_stress)  # Cap base_stress at 50

                    # Enhanced decay logic
                    if not significant_change:
                        if target_stress > base_stress + 20:  # Stronger decay when far above base
                            target_stress *= 0.15  # Very fast decay (85% reduction)
                        else:
                            target_stress *= 0.25  # Fast decay (75% reduction)
                    else:
                        target_stress *= 0.88  # Slower decay when active (12% reduction)

                    # Ensure target_stress doesn’t drop below base_stress
                    target_stress = max(base_stress, target_stress)
                    base_stress = max(0, base_stress)  # Prevent negative base stress

                    # Clamp target_stress
                    target_stress = max(-100, min(100, target_stress))

                    # Debug print final values
                    print(f"Stress Modifier: {stress_modifier:.2f}, Target Stress: {target_stress:.2f}, Base Stress: {base_stress:.2f}")

                # Clear buffer after processing
                data_buffer = []

        except Exception as e:
            print(f"Error in get_data: {e}")
            break

    serialCom.close()

def long_dot():
    global stress, target_stress

    # Initialize Pygame
    pygame.init()

    # Define constants
    WINDOW_WIDTH = 800
    WINDOW_HEIGHT = 600
    DOT_RADIUS = 3
    BACKGROUND_COLOR = (0, 0, 0)
    DOT_COLOR = (255, 0, 0)
    TRAIL_COLOR = (255, 255, 255)
    WIND_SPEED = 6
    SPEED = 5
    DAMPING_FACTOR = 0.1
    OSCILLATION_FREQ = 1.0  # Fast oscillation

    # Create the screen
    screen = pygame.display.set_mode((WINDOW_WIDTH, WINDOW_HEIGHT))
    pygame.display.set_caption("Dot with Wind Pushed Trail (Smoothed with Fast Oscillation)")

    # Initial dot position
    dot_x = WINDOW_WIDTH // 2
    dot_y = WINDOW_HEIGHT // 2

    # Main game loop
    running = True
    trail = []
    time_elapsed = 0

    while running:
        # Handle events
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        # Smooth stress towards target_stress
        with stress_lock:
            current_target_stress = target_stress
        stress += (current_target_stress - stress) * DAMPING_FACTOR

        # Add fast oscillation
        time_elapsed += 1 / 30  # Assuming 30 FPS
        oscillation = math.sin(time_elapsed * OSCILLATION_FREQ * 2 * math.pi) * abs(stress)
        display_stress = oscillation

        # Update dot position
        dot_y = (WINDOW_HEIGHT // 2) - (display_stress * SPEED)
        dot_y = max(0, min(WINDOW_HEIGHT - DOT_RADIUS * 2, dot_y))

        # Add to trail
        trail.append((dot_x, dot_y))
        trail = [(x - WIND_SPEED, y) for (x, y) in trail]

        # Clear the screen
        screen.fill(BACKGROUND_COLOR)

        # Draw the trail
        for (trail_x, trail_y) in trail:
            pygame.draw.circle(screen, TRAIL_COLOR, (int(trail_x) + DOT_RADIUS, int(trail_y) + DOT_RADIUS), DOT_RADIUS)

        # Draw the dot
        pygame.draw.circle(screen, DOT_COLOR, (int(dot_x) + DOT_RADIUS, int(dot_y) + DOT_RADIUS), DOT_RADIUS)

        # Update the screen
        pygame.display.flip()

        # Control the frame rate
        pygame.time.Clock().tick(30)

    pygame.quit()
    sys.exit()

# Create and start the threads
thread1 = threading.Thread(target=get_data, daemon=True)
thread2 = threading.Thread(target=long_dot)

thread1.start()
thread2.start()

thread2.join()