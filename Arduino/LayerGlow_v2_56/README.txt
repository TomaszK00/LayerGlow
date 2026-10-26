LayerGlow v2.56 — open setup for testing

Open LayerGlow_v2_56/LayerGlow_v2_56.ino in Arduino IDE. Keep the bundled src folder beside the sketch. Install esp32 by Espressif Systems 3.3.12, ArduinoJson 7.4.3 and Adafruit NeoPixel 1.15.5. On Tenstar Robot ESP32-C3 SuperMini select ESP32C3 Dev Module, USB CDC On Boot: Enabled, 4 MB Minimal SPIFFS (1.9 MB APP with OTA), and Erase All Flash Before Sketch Upload: Disabled.

After upload, join the open LayerGlow-Setup network without a password. If your phone or computer remembers an older password for that network, forget it and reconnect. Open http://192.168.4.1/. The main panel opens directly with no administrator login or first-run password change.

Saved home Wi-Fi, printer and LED settings remain. Home Wi-Fi still requires the router's password. The setup AP closes after home Wi-Fi connects and no setup client remains, then reopens if home Wi-Fi is lost.

This is a temporary test build: anyone able to reach the open setup network or the device on home Wi-Fi can use the panel. See docs/OPEN_SETUP_TEST_V2_56.md for the Polish guide. Physical AP behaviour must be checked on the owner's board. This source ZIP is for USB installation, not the public OTA release.
