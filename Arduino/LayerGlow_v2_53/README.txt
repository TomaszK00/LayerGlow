LayerGlow v2.53 — USB recovery candidate for Arduino IDE

Open LayerGlow_v2_53/LayerGlow_v2_53.ino in Arduino IDE. Keep the bundled src folder beside the sketch. Install the esp32 board package by Espressif Systems (v3.3.12 tested), ArduinoJson 7.4.3 and Adafruit NeoPixel 1.15.5. Select the ESP32-C3 board matching the device, enable USB CDC On Boot, and use the compatible 4 MB Minimal SPIFFS (1.9 MB APP with OTA) partition scheme. Set Erase All Flash Before Sketch Upload to Disabled to preserve saved Wi-Fi, printer and LED settings.

After upload, open Serial Monitor at 115200 baud. With the firmware running, hold BOOT for at least 10 seconds and then release it. This creates a new random LayerGlow-Setup password, clears the administrator login, and restarts only after BOOT is released. Read the line "LayerGlow-Setup password:" in Serial Monitor. Reconnect to LayerGlow-Setup using that password, open http://192.168.4.1/ and pair with new admin and AP passwords. This recovery does not erase saved Wi-Fi, printer or LED settings.

The setup AP now remains available even when the home Wi-Fi is connected, and retries starting if an earlier start failed. If the AP does not appear after recovery, copy the Serial Monitor output for diagnosis.

USB installation on the actual hardware still needs validation. This candidate is not the public automatic update; no signed v2.53 OTA image is included. See docs/USB_RECOVERY_V2_53.md.
