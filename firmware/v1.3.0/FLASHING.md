# Flashing Instructions (v1.3.0)

## Using `esptool` (Python)

If you have Python installed, you can flash these binaries directly:

```bash
python -m esptool --chip esp32c3 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 4MB --flash_freq 80m \
0x0 bootloader.bin \
0x8000 partition-table.bin \
0x11000 OTA_data_initial.bin \
0x20000 firmware.bin
```

## Using ESP Flash Download Tool (Windows)

1.  Open the Tool.
2.  Select **Developer Mode** -> **ESP32-C3**.
3.  Load the files:
    *   `bootloader.bin` @ `0x0`
    *   `partition-table.bin` @ `0x8000`
    *   `ota_data_initial.bin` @ `0x11000`
    *   `firmware.bin` @ `0x20000`
4.  Click **START**.
