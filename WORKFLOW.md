# Project Workflows

This document explains the internal workflows and logic for the core components of the Goku IR Device.

## 1. Infrared (IR) Workflow

### Learning Mode
1.  **Trigger**: User clicks "Start Learning" on Web UI or holds the physical button (Long Press).
2.  **State Change**: LED changes to **Yellow/Blink**.
3.  **RX Task**: The RMT Receiver channel is enabled and waits for signals.
4.  **Capture**: When an IR signal is detected, the RMT interrupt captures the pulse train into a buffer.
5.  **Save**: User calls "Save" via Web UI (or automatic if implemented).
    *   The captured symbols are serialized.
    *   Data is written to **NVS** (Non-Volatile Storage) under a specific key (e.g., `ac_on`).
6.  **Idle**: System returns to Idle state (LED Static).

### Sending Mode
1.  **Trigger**: RainMaker App command (e.g., Turn ON) or Web UI click.
2.  **Lookup**: The application searches NVS for the corresponding key (e.g., `ac_on`).
3.  **Load**: If found, the pulse data (symbols) is loaded into RAM.
4.  **Transmission**:
    *   LED changes to **Red/Flash**.
    *   RMT Transmitter channel sends the carrier-modulated signal (38kHz) to the IR LED.
5.  **Completion**: LED returns to Idle state.

---

## 2. Wi-Fi Configuration Workflow

### Provisioning (First Time Setup)
1.  **Boot**: Device checks if Wi-Fi credentials exist in NVS.
2.  **Missing Credentials**:
    *   Device starts **SoftAP Mode** with SSID `PROV_GOKU_AC`.
    *   LED indicates **Provisioning Mode** (Blue Running).
    *   User connects to SoftAP or uses RainMaker App to scan QR code.
3.  **Handshake**:
    *   App sends Wi-Fi SSID/Password via Protocomm (HTTP/BLE).
    *   Device verifies Proof of Possession (PoP).
4.  **Connect**:
    *   Device saves credentials to NVS.
    *   Device reboots or switches to **Station Mode** to connect to the router.

### Connection & Fallback
1.  **Station Mode**: Device attempts to connect to configured Wi-Fi.
    *   **Success**: LED changes to **Green/Static** (or User Color). Connects to RainMaker MQTT.
    *   **Failure**: If connection fails repeatedly (timeout):
        *   Device enables **Fallback AP** (`Goku-Recovery`).
        *   LED indicates **Warning/Provisioning**.
        *   Web UI is accessible via the AP to allow re-configuration.

---

## 3. Over-The-Air (OTA) Updates

### Automatic Check (Background)
1.  **Timer**: A background task runs periodically (default: 1 hour).
2.  **Check Version**:
    *   Device fetches `version.txt` from `CONFIG_OTA_SERVER_URL` (e.g., Surge.sh).
    *   Compares remote version with `PROJECT_VERSION`.
3.  **Update**:
    *   If remote > local: Downloads `.bin` firmware.
    *   Writes to the Passive Partition.
    *   Sets Passive as Active and Reboots.
4.  **Rollback**: If new firmware crashes on boot, ESP32 automatically rolls back to the previous partition.

### Manual Trigger (Web UI)
1.  **User Action**: User clicks "Check Update" -> "Update Now" on Web UI.
2.  **API Call**: Front-end checks version API.
3.  **Execution**: Triggers the same internal download/flash logic as the automatic process.

---

## 4. ESP RainMaker Workflow

### Initialization
1.  **Node Init**: Application initializes a RainMaker Node.
2.  **Device Creation**: Creates a virtual "Air Conditioner" device.
3.  **Param Registration**: Adds parameters:
    *   `Power` (Bool)
    *   `Mode` (Int/String: Auto, Cool, Heat, etc.)
    *   `Temperature` (Float)
4.  **Callback Register**: Registers a callback function for param updates.

### Cloud to Device (Control)
1.  **User**: Toggles "Power On" in Phone App.
2.  **Cloud**: AWS IoT / RainMaker Cloud sends MQTT message to device.
3.  **Device**:
    *   Receives MQTT payload.
    *   Triggers `write_cb`.
    *   Code executes the logic (e.g., calls `app_ir_send_cmd(AC_ON)`).
    *   Updates local value and reports it back to Cloud (ACK).

### Device to Cloud (State Update)
1.  **Event**: OTA update finishes or Local Web Control changes state.
2.  **Report**: Device calls `esp_rmaker_param_update_and_report()`.
3.  **Cloud**: Updates shadow state.
4.  **App**: UI reflects the new state.

---

## 5. Web UI Workflow

### Architecture
*   **Server**: ESP32 runs a lightweight HTTP Server (`esp_http_server`).
*   **Front-end**: Single Page Application (embedded HTML/CSS/JS string in `app_web.c`).

### Interaction
1.  **Access**: User visits `http://gokuac.local`.
2.  **Load**: Browser downloads the HTML/JS.
3.  **JS Init**: JavaScript `fetch()` calls `/api/ir/list` to get saved keys.
4.  **Commands**:
    *   **Save Key**: `POST /api/save?key=my_key` -> Device enters Learn Mode -> Saves Result.
    *   **Send Key**: `POST /api/send?key=my_key` -> Device transmits IR.
    *   **Wifi Config**: `POST /api/wifi/config` -> Updates NVS -> Restart.
