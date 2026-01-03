# Goku IR Device (ESP32)

## Overview

**Goku IR Device** is an ESP32-based smart home controller designed to manage Air Conditioners via Infrared (IR). It integrates seamlessly with **ESP RainMaker** for remote control via a mobile app and supports local control through a web interface.

### Key Features
- **Smart AC Control**: Turn AC On/Off, change modes (Auto, Cool, Heat, Fan), and adjust temperature (16-30°C).
- **ESP RainMaker Integration**: Remote control, provisioning, and param updates via the RainMaker App.
- **Local Web Interface**: Control the device locally via a browser at `http://gokuac.local`.
- **Automatic OTA Updates**: Periodically checks for firmware updates from a configured server.
- **mDNS Support**: Easily discoverable on the network as `gokuac.local`.

## Hardware Requirements

- **ESP32 Development Board** (e.g., ESP32-DevKitC)
- **IR Transmitter LED**: Connected to a PWM-capable GPIO (Default: configured in `app_ir.c`).
- **IR Receiver (Optional)**: For learning IR codes.
- **Status LED**: Indicates Wi-Fi and power status.
- **Boot/User Button**: Used for resetting or provisioning triggering.

## Getting Started

### Prerequisites
- **ESP-IDF** (v4.4 or later recommended)
- **Git**

### Installation

1.  **Clone the Repository**:
    ```bash
    git clone https://github.com/your-repo/goku-ir-device.git
    cd goku-ir-device
    ```

2.  **Configure the Project**:
    Set your target (e.g., esp32, esp32c3) and menuconfig if needed.
    ```bash
    idf.py set-target esp32
    idf.py menuconfig
    ```

3.  **Build the Project**:
    ```bash
    idf.py build
    ```

4.  **Flash and Monitor**:
    ```bash
    idf.py flash monitor
    ```

## Usage

### 1. Wi-Fi Provisioning
See [WORKFLOW.md](WORKFLOW.md) for detailed process.
Upon first boot, the device will be in **Provisioning Mode**.
- **Mobile App**: Use the **ESP RainMaker** app to scan the QR code (printed in the serial monitor).
- **Manual**: Connect to the SoftAP `PROV_GOKU_AC` (Password: `12345678`) and use a provisioning tool.

### 2. RainMaker App Control
Once provisioned and connected to Wi-Fi:
- Open the RainMaker App.
- You will see the "Air Conditioner" device.
- Use the UI to toggle power, set temperature, and change modes.

### 3. Local Web Control
- Ensure your computer/phone is on the same Wi-Fi network.
- Navigate to `http://gokuac.local`.
- Use the web interface to send commands directly.

### 4. Over-The-Air (OTA) Updates
- The device checks for updates at `CONFIG_OTA_SERVER_URL` (configured in menuconfig).
- Ensure your server hosts `version.txt` and `goku-ir-device.bin`.

## Project Structure

- `main/`
    - `main.c`: Application entry point and initialization.
    - `app_wifi.c`: Wi-Fi connection and provisioning logic.
    - `app_rainmaker.c`: ESP RainMaker node configuration and callbacks.
    - `app_ir.c`: IR transmission logic (RMT).
    - `app_web.c`: Local HTTP server.
    - `app_ota.c`: Automatic firmware update task.
    - `app_mdns.c`: mDNS service registration.

## License

This project is licensed under the MIT License.
