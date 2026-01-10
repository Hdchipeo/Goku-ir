# Goku IR Device v1.3.0

**Goku IR Device** is an Advanced Agentic IoT controller powered by the ESP32-S3. It transforms standard Air Conditioners into smart devices, controllable via **ESP RainMaker**, Voice Assistants, and a local Web Interface.

## 🚀 What's New in v1.3.0
*   **Refined Provisioning**: Streamlined BLE-based provisioning via ESP RainMaker App.
*   **Web UI Toggle**: The local Web Interface (`http://gokuir.local`) is now **OFF by default** for security and performance. Enable it anytime via the "WebUI Config Mode" switch in the RainMaker App.
*   **OTA Hardening**: Enhanced Over-The-Air updates with automatic rollback on failure and update history tracking.
*   **UI/UX Improvements**: "WebUI Config Mode" is now a proper Toggle Switch, and LED effects are smoother.
*   **Universal IR Engine**: Improved compatibility for Daikin, Samsung, LG, and more.

---

## 🌟 Key Features

### 🎮 Smart Control
*   **ESP RainMaker Integration**: Full cloud control via iOS/Android App.
    *   Power, Mode, Temp, Fan Speed.
    *   **WebUI Config Mode Toggle**.
*   **Universal AC Engine**: Built-in support for major brands (Daikin, Samsung, Mitsubishi, Panasonic, LG) + Custom Learning.
*   **Local Web Interface** (when enabled):
    *   Advanced Configuration & Stats.
    *   IR Learning Dashboard.
    *   LED Effect Customization.

### 🌈 LED Control
*   **Dynamic Effects**: Rainbow, Breathing, Fire, Sparkle, and more.
*   **Status Indication**:
    *   **Cyan Breathing**: Provisioning Mode.
    *   **Blue Solid**: Wi-Fi Connected.
    *   **Green Blink**: IR Signal Sent.
    *   **Yellow/Red**: Factory Reset / Error.

### 🛡 Reliability
*   **Robust OTA**: Dual-partition update system with auto-rollback if the new firmware fails to boot.
*   **OTA History**: Tracks recent update attempts (Success/Fail) in NVS.

---

## 🛠 Hardware Configuration

| Component | Default GPIO | Notes |
| :--- | :--- | :--- |
| **IR Transmitter** | GPIO 8 | Controls AC |
| **IR Receiver** | GPIO 9 | For Learning Mode |
| **RGB LED** | GPIO 2 | WS2812B / SK6812 |
| **Button** | GPIO 3 | Factory Reset (Long Press 3s) |

---

## 📲 Provisioning Guide

**Note:** This firmware supports **ESP RainMaker Provisioning (BLE)** only. SoftAP provisioning is NOT supported in this release.

1.  **Install App**: Download **ESP RainMaker** for iOS or Android.
2.  **Power On**: The device LED should breathe **Cyan**.
3.  **Add Device**:
    *   Open App -> Add Device.
    *   Scan the QR Code (from Serial Monitor) OR select "I don't have a QR code" -> **Manual Pairing**.
    *   Select device named `PROV_XXXXXX` (via Bluetooth).
4.  **Connect**: Enter your Wi-Fi credentials in the App.
5.  **Done**: The LED turns **Solid Blue** upon connection.

---

## 🌐 Web Interface

By default, the Web UI is **DISABLED** to save resources.

**To Enable:**
1.  Open ESP RainMaker App.
2.  Toggle **"WebUI Config Mode"** to **ON**.
3.  Visit `http://gokuir.local` (or device IP) in your browser.

**To Disable:**
1.  Toggle **"WebUI Config Mode"** to **OFF**.

---

## 🔄 Firmware Updates (OTA)

The device supports Over-The-Air updates.
*   **Automatic**: Checks for updates at boot from the configured server.
*   **Manual Trigger**: Can be triggered via CLI (if enabled).
*   **Rollback**: If a bad update is flashed, the device automatically reverts to the previous working version after reboot.

---

## 📂 Project Structure & Workflows

### `/components`
*   **`goku_core`**: System utilities (Logging, Memory monitoring, NVS).
*   **`goku_wifi`**: Wi-Fi manager & Provisioning logic (`network_provisioning`).
*   **`goku_rainmaker`**: Cloud integration (Parameters, Callbacks).
*   **`goku_web`**: HTTP Server & API (only runs when toggled).
*   **`goku_ir`**: RMT Driver & Protocol Logic.
*   **`goku_ota`**: Update manager & Rollback logic.

### `/main`
*   **`main.c`**: Application Entry Point. Orchestrates initialization of all components.
*   **`Kconfig.projbuild`**: Project-level configuration options (GPIOs, URLs).

### `/firmware`
*   Contains release binaries (`.bin`) for flashing.

---

## 📦 Flashing Firmware

**Pre-built Binaries (v1.3.0)** are located in `firmware/v1.3.0/`.

**Command Line:**
```bash
# Erase Flash (Recommended for major version jumps)
idf.py -p PORT erase_flash

# Flash & Monitor
idf.py -p PORT flash monitor
```

**Build from Source:**
```bash
idf.py set-target esp32c3  # or esp32s3
idf.py build
idf.py flash monitor
```

---

## 📄 License
MIT License.
