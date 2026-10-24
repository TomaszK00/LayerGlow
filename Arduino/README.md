# Arduino source archive

The original Arduino sketches have been recovered from versioned ZIP packages and Git history. These are preserved source releases, **not** newly rewritten firmware. The corresponding binary OTA images are documented in the [firmware archive](../Software/archive/README.md).

## Sketches

| Version | Original sketch | Status |
| --- | --- | --- |
| 2.0 | [K2_Status_Light_v2_0.ino](archive/K2_Status_Light_v2_0.ino) | Initial no-physical-LED web/Moonraker prototype |
| 2.1 | [K2_Status_Light_v2_1.ino](legacy/K2_Status_Light_v2_1.ino) | Restored directly from original Git commit `d6295dec` |
| 2.2 | — | Original ZIP in older file collection could not be retrieved |
| 2.3 | [K2_Status_Light_v2_3.ino](archive/K2_Status_Light_v2_3.ino) | Original source from versioned firmware ZIP |
| 2.4 | [K2_Status_Light_v2_4.ino](archive/K2_Status_Light_v2_4.ino) | Original source from versioned firmware ZIP |
| 2.5 | [K2_Status_Light_v2_5.ino](archive/K2_Status_Light_v2_5.ino) | Original source from versioned firmware ZIP |
| 2.6 | [K2_Status_Light_v2_6.ino](archive/K2_Status_Light_v2_6.ino) | Original source from versioned firmware ZIP |
| 2.7 | [K2_Status_Light_v2_7.ino](archive/K2_Status_Light_v2_7.ino) | Original source from versioned firmware ZIP |
| 2.8 | [K2_Status_Light_v2_8.ino](archive/K2_Status_Light_v2_8.ino) | Original source from versioned firmware ZIP |
| 2.9 | [K2_Status_Light_v2_9.ino](archive/K2_Status_Light_v2_9.ino) | Original source from versioned firmware ZIP |
| 2.10 | [K2_Status_Light_v2_10.ino](archive/K2_Status_Light_v2_10.ino) | Original source from versioned firmware ZIP |
| 2.11 | [K2_Status_Light_v2_11.ino](archive/K2_Status_Light_v2_11.ino) | Original source from versioned firmware ZIP |
| 2.12 | [K2_Status_Light_v2_12.ino](archive/K2_Status_Light_v2_12.ino) | Original source from versioned firmware ZIP |
| 2.13 | [K2_Status_Light_v2_13.ino](archive/K2_Status_Light_v2_13.ino) | Original source from versioned firmware ZIP |
| 2.14 | [K2_Status_Light_v2_14.ino](archive/K2_Status_Light_v2_14.ino) | Original source from versioned firmware ZIP |
| 2.15 | [K2_Status_Light_v2_15.ino](archive/K2_Status_Light_v2_15.ino) | Original source from versioned firmware ZIP |
| 2.16 | [K2_Status_Light_v2_16.ino](archive/K2_Status_Light_v2_16.ino) | Original source from versioned firmware ZIP (newer archived build) |

**Current OTA advertised to devices: v2.44 test candidate.** The current [Arduino sketch](LayerGlow_v2_44/LayerGlow_v2_44.ino) includes its bundled PubSubClient source; v2.16 remains in the archive. [Change history](../CHANGELOG.md) documents what changed.

## Original ESP32 settings (v2.3 onward)

Original build notes target ESP32-C3 SuperMini (ESP32C3 Dev Module in Arduino IDE), 4 MB flash, USB CDC on Boot enabled and a compatible OTA partition layout such as **Minimal SPIFFS (1.9MB APP with OTA)**. Installed libraries include ArduinoJson 7 and Adafruit NeoPixel.

Typical code definitions in these builds use:
- GPIO4 for SK6812 RGBW LED data;
- GPIO8 for the onboard blue LED (active LOW on the referenced board);
- up to 24 RGBW LEDs, with a 12/24 selectable layout.

These are not guaranteed pinouts for every hardware revision. See [hardware notes](../docs/HARDWARE.md) and [installation guidance](../docs/INSTALLATION.md).

## Security and usage

Older versions have fixed setup access-point credentials and may not authenticate the web and OTA interface. Restrict access to a trusted local network; change defaults wherever the sketch allows it. Never publish personal Wi-Fi passwords or private credentials in GitHub commits.

## Rebuilding

Open the chosen sketch in Arduino IDE with compatible ESP32 board definitions and required libraries. Build requirements and flash layout may differ between versions. Always match the firmware to the intended board and the actual strip wiring. Archived binaries have not been physically retested during restoration.

See [license notes](../docs/LICENSING.md) for the current personal-use terms and historical MIT permissions.
