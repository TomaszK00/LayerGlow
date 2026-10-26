LayerGlow v2.55 — Arduino IDE simple password setup

Open LayerGlow_v2_55/LayerGlow_v2_55.ino in Arduino IDE. Keep the bundled src folder beside the sketch. Install esp32 by Espressif Systems 3.3.12, ArduinoJson 7.4.3 and Adafruit NeoPixel 1.15.5. On Tenstar Robot ESP32-C3 SuperMini select ESP32C3 Dev Module, USB CDC On Boot: Enabled, 4 MB Minimal SPIFFS (1.9 MB APP with OTA), and Erase All Flash Before Sketch Upload: Disabled.

After upload, join LayerGlow-Setup with its current password. If v2.54 pairing was never completed, it is layerglowsetup. Open http://192.168.4.1/. Enter the current Wi-Fi password, choose one new password of 12–63 characters, and press Save. No button or Serial Monitor command is needed. After restart, reconnect with the new Wi-Fi password and log in to the panel as admin with that same password.

If you later forget the password, hold BOOT for 10 seconds while firmware is running, or send RESET_AP via the USB Serial Monitor at 115200 baud. That restores temporary setup access without deleting saved home Wi-Fi, printer or LED settings. RST/EN only restarts the board.

See docs/SIMPLE_SETUP_V2_55.md for the Polish guide. Physical AP behaviour must be checked on the owner's board. This source ZIP is for USB installation, not the public OTA release.
