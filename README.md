# Grind Tracker 2.0

A coffee grind size tracking application for the M5Paper e-ink display. Track multiple coffee beans and their preferred grind settings with an intuitive interface and web-based management.

## Features

- **Multiple Coffee Tracking**: Store and manage up to 10 different coffee entries
- **Persistent Storage**: All settings saved to SD card
- **Joystick Navigation**: Navigate and adjust grind sizes using the built-in joystick
- **Web-Based Management**: WiFi access point with a modern web UI for adding, editing, and deleting entries from your smartphone
- **Power Efficient**: Automatic deep sleep after inactivity to extend battery lifetime

## Hardware Requirements

- **M5Paper** (M5Stack Paper e-ink display)
- **MicroSD Card** (for persistent storage)

## Software Requirements

- **Arduino IDE** (or Arduino CLI)
- **ESP32 Board Support** (via Board Manager)
- **M5Stack Board Package** (for M5Paper support)
- **M5Unified Library** (for M5Stack device support)

## Installation

### 1. Install Arduino IDE and Board Support

1. Download and install [Arduino IDE](https://www.arduino.cc/en/software)
2. Open Arduino IDE and go to **File > Preferences**
3. Add this URL to **Additional Board Manager URLs**:
   ```
   https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json
   ```
4. Go to **Tools > Board > Boards Manager**
5. Search for "M5Stack" and install **M5Stack Boards**

### 2. Install Required Libraries

1. Go to **Tools > Manage Libraries**
2. Search for and install:
   - **M5Unified** (by M5Stack)

### 3. Configure Board Settings

1. Select **Tools > Board > M5Stack Arduino > M5Paper**
2. Select the appropriate **Port** (Tools > Port)
3. Ensure **Partition Scheme** is set to **Default** or **No OTA**

### 4. Upload the Sketch

1. Open `grind_tracker_2_0.ino` in Arduino IDE
2. Click **Upload** (or use Arduino CLI: `arduino-cli upload -p <PORT> --fqbn m5stack:esp32:m5stack_paper grind_tracker_2_0.ino`)

## Usage

### On-Device Controls

The M5Paper has three buttons (A, B, C) that function as a joystick:

- **Button A (Up)**: 
  - In navigate mode: Move selection up
  - In edit mode: Increase grind size value
- **Button B (Select)**:
  - In navigate mode: Enter edit mode for selected entry
  - In edit mode: Exit edit mode
- **Button C (Down)**:
  - In navigate mode: Move selection down
  - In edit mode: Decrease grind size value
- **Button A (Long Press)**: Delete the selected entry (in navigate mode)

### Navigation Modes

- **Navigate Mode**: Browse through coffee entries. Selected entry is highlighted with a black background and white text.
- **Edit Mode**: Adjust the grind size value of the selected entry. The value is shown in brackets `[25]` to indicate editing mode.

### Web Interface

1. **Connect to WiFi**: When the device is awake, it creates a WiFi access point named **"GrindTracker"** (no password required)
2. **Open Web Interface**: Connect to the network and navigate to `http://192.168.4.1` in your browser
3. **Manage Entries**:
   - **Add Coffee**: Enter coffee name and grind size, click the `+` button
   - **Edit Grind Size**: Change the number in the grind size input field
   - **Delete Coffee**: Click the `X` button next to an entry
   - **Sync to Device**: Click "Sync to Device" at the bottom to push all changes to the M5Paper and update the display

### Power Management

- The device automatically enters deep sleep after **5 minutes** of inactivity
- Wake the device by pressing any button
