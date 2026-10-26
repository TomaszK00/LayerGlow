LayerGlow v2.54 — Arduino IDE setup AP recovery

Open LayerGlow_v2_54/LayerGlow_v2_54.ino in Arduino IDE. Keep the bundled src folder beside the sketch. Install esp32 by Espressif Systems (3.3.12 tested), ArduinoJson 7.4.3 and Adafruit NeoPixel 1.15.5. Select the matching ESP32-C3 board, USB CDC On Boot: Enabled, and the 4 MB Minimal SPIFFS (1.9 MB APP with OTA) partition scheme. Keep Erase All Flash Before Sketch Upload: Disabled to preserve saved Wi-Fi, printer and LED settings.

On the first v2.54 boot, the old administrator and setup AP access credentials are reset once. LayerGlow-Setup then uses the temporary password layerglowsetup. Connect to that network and open http://192.168.4.1/. The panel requires you to choose new administrator and setup AP passwords, each at least 12 characters. To authorize saving, hold a working BOOT button while submitting, or send PAIR in the Arduino IDE Serial Monitor at 115200 baud and submit within 2 minutes. Release BOOT if held; the device restarts. Reconnect with the password you chose during pairing.

Later recovery: hold a working BOOT button for 10 seconds while firmware runs, or send RESET_AP via the USB Serial Monitor. Both reset the administrator login and setup AP password to the temporary default without erasing saved Wi-Fi, printer or LED settings. If the button marked RST/EN only restarts the board, use the USB command. Do not hold BOOT while powering on or resetting the board.

While pairing is incomplete, the device runs only LayerGlow-Setup and pauses home Wi-Fi attempts. After pairing, it uses DHCP by default; static IP is optional per saved Wi-Fi network. The setup AP closes after home Wi-Fi connects and no setup client is using it, then reopens if home Wi-Fi is lost.

The actual board and button wiring still need physical validation. This source ZIP is for USB installation; it is not the public OTA release. See docs/USB_RECOVERY_V2_54.md.
