# Goku IR Device v1.4.1

**Goku IR Device** is an Advanced Agentic IoT controller powered by ESP32-C3/S3. It transforms standard Air Conditioners into smart devices, controllable via **ESP RainMaker** (Android), **Apple HomeKit** (iOS), Voice Assistants, and a local Web Interface.

## 🚀 What's New in v1.4.x

*   **Dual Platform Support**: Android (RainMaker) and iOS (HomeKit) with selectable configuration.
*   **HomeKit Integration**: Native Apple Home app control with Thermostat, Temperature/Humidity sensors.
*   **AHT20 Sensor Support**: Optional temperature and humidity monitoring (configurable via menuconfig).
*   **Captive Portal**: iOS SoftAP provisioning with auto-popup trigger (DHCP Option 114).
*   **Sensor Display Fix**: Temperature/humidity rounded to 1 decimal place for cleaner display.
*   **State Restore**: AC state automatically restored in RainMaker/HomeKit after reboot.
*   **Log Optimization**: Reduced console spam from RainMaker parameter updates.
*   **ESP32-S3 PSRAM Fix**: Proper memory monitoring for PSRAM-enabled boards.

---

## 🌟 Key Features

### 🎮 Smart Control
*   **Dual Platform**:
    *   **Android**: ESP RainMaker cloud control via iOS/Android App.
    *   **iOS**: Native HomeKit integration via Apple Home app.
*   **Universal AC Engine**: Built-in support for major brands (Daikin, Samsung, Mitsubishi, Panasonic, LG) + Custom Learning.
*   **Local Web Interface** (when enabled):
    *   Advanced Configuration & Stats.
    *   IR Learning Dashboard.
    *   LED Effect Customization.

### 🌡 Sensors
*   **AHT20 Support**: Optional I2C temperature/humidity sensor.
*   **RainMaker/HomeKit Integration**: Sensor data displayed in cloud dashboard.

### 🌈 LED Control
*   **Dynamic Effects**: Rainbow, Breathing, Fire, Sparkle, and more.
*   **Status Indication**:
    *   **Cyan Breathing**: Provisioning Mode.
    *   **Blue Solid**: Wi-Fi Connected.
    *   **Green Blink**: IR Signal Sent.
    *   **Yellow/Red**: Factory Reset / Error.

### 🛡 Reliability
*   **Robust OTA**: Dual-partition update system with auto-rollback.
*   **State Persistence**: AC settings saved to NVS, restored on reboot.

---

## 🛠 Hardware Configuration

| Component | Default GPIO | Notes |
| :--- | :--- | :--- |
| **IR Transmitter** | GPIO 4 | Controls AC |
| **IR Receiver** | GPIO 7 | For Learning Mode |
| **RGB LED** | GPIO 8 | WS2812B / SK6812 |
| **Button** | GPIO 3 | Factory Reset (Long Press 3s) |
| **AHT20 SDA** | GPIO 5 | I2C (Optional Sensor) |
| **AHT20 SCL** | GPIO 6 | I2C (Optional Sensor) |

*GPIOs are configurable via `idf.py menuconfig` -> Hardware Configuration.*

---

## 📲 Provisioning Guide

### Android (ESP RainMaker)
1.  Download **ESP RainMaker** app.
2.  Power on device (LED breathes **Cyan**).
3.  Add Device -> Scan QR Code or Manual Pairing.
4.  Select `PROV_XXXXXX` via Bluetooth, enter Wi-Fi credentials.
5.  LED turns **Solid Blue** when connected.

### iOS (HomeKit + Captive Portal)
1.  Power on device (LED breathes **Cyan**).
2.  Connect to `GokuAC_XXXX` WiFi network.
3.  Captive portal auto-opens for WiFi configuration.
4.  After WiFi connection, add device in Apple Home app using the pairing code displayed in Serial Monitor.

---

## 🌐 Web Interface

The Web UI is **DISABLED by default** to save resources.

**To Enable (Android):**
1.  Open ESP RainMaker App.
2.  Toggle **"WebUI Config Mode"** to ON.
3.  Visit `http://gokuac.local:8080` in browser.

**To Enable (iOS):**
*   Web UI is available at `http://gokuac.local:8080` when connected to the same network.

---

## 📂 Project Components

| Component | Description |
| :--- | :--- |
| **`goku_core`** | System utilities, memory monitoring, NVS |
| **`goku_wifi`** | Wi-Fi manager, mDNS, provisioning |
| **`goku_rainmaker`** | Cloud integration (Android) |
| **`goku_homekit`** | Apple HomeKit integration (iOS) |
| **`goku_sensor`** | AHT20 temperature/humidity driver |
| **`goku_wifi_portal`** | Captive portal for iOS provisioning |
| **`goku_web`** | HTTP Server & API |
| **`goku_ir`** | RMT Driver & IR Protocol Logic |
| **`goku_ota`** | Update manager & Rollback |

---

## 📦 Build & Flash

```bash
# Set target
idf.py set-target esp32c3  # or esp32s3

# Configure (optional - select platform, sensors, GPIOs)
idf.py menuconfig

# Build
idf.py build

# Flash & Monitor
idf.py -p PORT flash monitor
```

**Platform Selection** (in menuconfig):
- `Mobile Platform` -> Android (RainMaker) or iOS (HomeKit)
- `Sensors` -> Enable AHT20 if connected

---

## 📄 License
MIT License.
