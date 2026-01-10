# Goku IR Device (ESP32-S3)

**Goku IR Device** is an advanced smart home controller powered by the ESP32-S3. It transforms standard Air Conditioners into smart devices, controllable via a local web interface, the ESP RainMaker mobile app, or voice assistants. It features a new **Universal IR Engine** for broad AC compatibility and a dynamic 8-LED ring for visual feedback.

## 🌟 Key Features

### 🎮 Smart Control
*   **Universal AC Engine**: Supports Daikin, Samsung, Mitsubishi, and generic NEC protocols via a registry-based system.
*   **Local WebUI**: Full control via `http://gokuir.local` (or IP address).
    *   Control Power, Mode (Auto, Cool, Heat, Fan), and Temperature.
    *   Real-time Dashboard with device stats (Uptime, RSSI, Memory).
    *   **New**: Optimized loading speed and stable connection.
*   **Remote Access**: Integrated **ESP RainMaker** for control from anywhere.
*   **mDNS**: Automatically discoverable as `gokuir.local`.

### 🌈 Advanced LED Control
*   **Hardware**: Supports WS2812B/SK6812 8-LED Ring.
*   **Effects**:
    *   **Auto Cycle**: Automatically rotates through effects every 20s.
    *   **Random**: Chaotic colored noise.
    *   Standard: Rainbow, Running (Custom Colors), Breathing, Blink, Knight Rider, Loading, Color Wipe, Theater Chase, Fire, Sparkle, Static.
*   **Customization**: Set Speed, Brightness, and individual LED colors.
*   **Persistence**: Saves your last used Effect and Settings to flash memory (survives restarts).

### 📡 Connectivity & System
*   **IR Learning (Rx)**: Capture and analyze IR signals from existing remotes.
*   **OTA Updates**: Automatic firmware updates from a configured server (e.g., Surge.sh).
*   **Factory Reset**: Hard reset via onboard button.

---

## 🛠 Hardware Configuration

The project is optimized for **ESP32-S3 SuperMini** boards but works with generic ESP32-S3 modules.

| Component | Default GPIO (Kconfig) | Description |
| :--- | :--- | :--- |
| **IR Transmitter** | **GPIO 8** | IR LED (controls AC) |
| **IR Receiver** | **GPIO 9** | IR Receiver Module (38kHz) |
| **RGB LED** | **GPIO 2** | WS2812B/SK6812 (Data Pin) |
| **Button** | **GPIO 3** | Boot/User Button |

> **Note**: These pins can be changed via `idf.py menuconfig` under **Hardware Configuration**.

---

## 🚀 Getting Started

### Prerequisites
*   ESP-IDF v5.x (v5.4.2 recommended)
*   Git

### Installation
1.  **Clone the Repository**:
    ```bash
    git clone https://github.com/Hdchipeo/Goku-ir.git
    cd Goku-ir
    ```

2.  **Set Target**:
    ```bash
    idf.py set-target esp32s3
    ```

3.  **Configure**:
    ```bash
    idf.py menuconfig
    ```
    *   **Hardware Configuration**: Set GPIOs for IR and LEDs.
    *   **WiFi Configuration**: Set Provisioning Method (BLE/SoftAP) and PoP.
    *   **OTA Configuration**: Set your firmware update URL.

4.  **Build & Flash**:
    ```bash
    idf.py build flash monitor
    ```

---

## 📖 Usage Guide

### 1. Provisioning (First Setup)
Upon first boot, the Main LED will breathe **Cyan** (Provisioning Mode).
*   **Method A: ESP RainMaker App (BLE)** - *Recommended*
    1.  Install **ESP RainMaker** (iOS/Android).
    2.  Scan the QR code printed in the Serial Monitor.
    3.  Follow instructions to connect the device to your Wi-Fi.
*   **Method B: SoftAP**
    1.  Connect to Wi-Fi `PROV_GOKU_IR`.
    2.  Password (PoP): `12345678` (default).
    3.  Use a provisioning tool to send Wi-Fi credentials.

### 2. Manual Controls
*   **Button Usage**:
    *   **Single Click**: Restart Device.
    *   **Long Press (3s)**: Factory Reset (Base LED turns Red/Yellow, then restarts).

### 3. LED Control
Navigate to `http://gokuir.local/` -> **LED Control**.
*   Select effects like **Rainbow**, **Fire**, or **Auto Cycle**.
*   Adjust **Speed** (1-100) and **Brightness** (0-100).
*   Click **Save Preset** to keep these settings after reboot.

### 4. Over-The-Air (OTA) Updates
The device automatically checks for updates at boot and periodically from the URL configured in `menuconfig`.
*   Current OTA Server: `https://salty-mouth.surge.sh`

---

## 📂 Project Structure

This project follows a component-based architecture:

*   **`components/goku_core`**: Core utilities (Logging `goku_log`, Memory `goku_mem`, Data/NVS `goku_data`).
*   **`components/goku_peripherals`**: Hardware drivers (LED `goku_led`, Button `goku_button`).
*   **`components/goku_wifi`**: Wi-Fi connection and mDNS (`goku_wifi`, `goku_mdns`).
*   **`components/goku_ir`**: **Universal IR Engine**, Protocols, RMT Driver, and IR App logic.
*   **`components/goku_web`**: Embedded Web Server and API handlers.
*   **`components/goku_rainmaker`**: ESP RainMaker Cloud integration.
*   **`components/goku_ac`**: High-level AC control state machine.
*   **`components/goku_ota`**: OTA Update manager.
*   **`main/`**: Application Entry point (`main.c`) and configuration.

## 📄 License
This project is open-source. Feel free to modify and distribute.
