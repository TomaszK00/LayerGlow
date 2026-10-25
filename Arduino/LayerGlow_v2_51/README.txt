LayerGlow v2.51 — test candidate

Use LayerGlow_v2_51_OTA.bin for a manual web update. It is supplied separately on candidate/v2.51. This ZIP contains the Arduino source and bundled PubSubClient, not an OTA image.

Arduino IDE: open LayerGlow_v2_51/LayerGlow_v2_51.ino. Install ArduinoJson 7.4.3 and Adafruit NeoPixel 1.15.5 using Library Manager. Select the appropriate ESP32-C3 board and a compatible 4 MB Minimal SPIFFS (1.9MB APP with OTA) partition scheme. Do not change partition layout via a normal OTA upload.

PlatformIO compilation and automated regression checks are available in GitHub Actions. Arduino IDE preprocessing/compilation and actual device installation still need validation.

This is a candidate for hardware testing, not a hardware-validated stable release. Review docs/AUDIT_2026-10-25.md on candidate/v2.51 for limitations and acceptance steps. The public updater on main continues advertising v2.45.
