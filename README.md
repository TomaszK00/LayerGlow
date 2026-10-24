# LayerGlow

**ESP32-C3-powered LED status and progress indicator for 3D printers. Creality K2 is the documented hardware integration; other printer options in recent firmware are experimental.**

![Project status](https://img.shields.io/badge/status-development-orange)
![Current OTA](https://img.shields.io/badge/current%20OTA-v2.43-orange)
![Historical archive](https://img.shields.io/badge/historical%20archive-v2.16-informational)
![Platform](https://img.shields.io/badge/platform-ESP32--C3-green)

> **OTA test candidate: v2.43.** This release is compiled by GitHub Actions and has not yet been tested on a physical ESP32-C3 or printer. v2.42 remains available in Software/ for rollback; v2.16 remains archived.

## v2.43 test candidate

Selecting a state other than Printer reading immediately runs that test on the preview and connected LEDs. Printing progresses by 5% every 0.5 seconds, holds at 100% for one second and repeats. The two playback checkboxes and the manual test-progress slider have been removed; choosing Printer reading resumes live printer status and progress. The browser animates the test locally between its normal status polls.

OTA v2.43: 1,749,984 bytes; SHA-256 `7fb51423def60d1d069c6e80dd6a10b667ab010c9990e0d41d86276bc6423ec0`.

## v2.42 test candidate

The preview now puts Test state and its compact progress, lit LED count and state on one line, without a tinted status box. Printer reading uses the actual printer state and progress; when the connection is lost, it shows an unavailable reading instead of a stale percentage. The compact mobile preview keeps the line visible while scrolling.

OTA v2.42: 1,752,048 bytes; SHA-256 `a893b898d54167c55707668b8d8727edf697e21bc502ef6830c738c0c3043e5f`.

## v2.41 test candidate

Test state now includes Printer reading. Selecting it turns off simulation and displays live printer status; choosing any other state starts a test on the preview and connected strip. Play progress automatically occupies the former Test mode checkbox position, and Repeat test after 100% remains beside it. The mobile progress label says Print progress when live data is selected.

OTA v2.41: 1,751,072 bytes; SHA-256 `e459974d518ce041b7bc38f311357e84f8a146e34dc266423b43565865777c34`.

## v2.40 test candidate

The LED preview remains one full-width row. It shows up to 48 LEDs; for physical strips of 49–300 LEDs, 48 preview lights represent the complete strip and the real LED count stays visible. Colour zones, printing progress and animation positions are sampled across the physical strip. Only the browser preview is capped; the configured number of connected LEDs remains unchanged.

OTA v2.40: 1,750,624 bytes; SHA-256 `e3cf85c709f7b9839185010fcb740676adf98c8fe974667e32c83bc31af07cb1`.

## v2.39 test candidate

The footer theme selector now shows three labelled checkboxes in one row: Auto, Light and Dark. Choosing one clears the other two; one choice always stays active. Existing saved appearance preferences and the automatic system mode continue to work.

OTA v2.39: 1,749,184 bytes; SHA-256 `1f71a816dae0d73ca2d584f3a9dc4a829734db711d322d63208ac7c539340bb7`.

## v2.38 test candidate

On phones, the top menu now hides when the Colours and LED settings heading passes beneath the sticky LED preview, and returns when the heading appears again. The footer uses three coloured dots for automatic, light and dark appearance, with a label for the selected mode. The setting is remembered and the automatic choice follows the device appearance.

OTA v2.38: 1,749,696 bytes; SHA-256 `0ba86c089c91e4cbc0363dcb94cdf0db559b4cedc8c90c344695a57d4a4c3632`.

## v2.37 test candidate

On phones, the +/− colour controls share the first row for 1–4 colours, the second row for 5–10, and the third row for 11–12. The selected effect name appears alongside its label. The previous FAQ and appearance controls remain available.

The FAQ now presents the blue status LED signals as three expandable answers alongside the other searchable help, for 31 questions in English, Polish, Spanish and German. A footer control switches the entire panel between automatic system appearance, light mode and dark mode; the browser remembers the selection. The DHCP hostname remains `LayerGlow`, and the default `layerglow.local` address and `LayerGlow-Setup` AP are unchanged.

OTA v2.37: 1,747,920 bytes; SHA-256 `5ffe13de2f9e028b41119f8e6df2e1e24eb712e809b60beba2c15f898ddccfcd`.

The current OTA manifest names a single LayerGlow image; older versions and recovery artifacts remain available. Historical K2 binaries remain available in the archive.

The setup AP is `LayerGlow-Setup` with password `layerglowsetup`; the default local address is `http://layerglow.local/`. The update keeps saved settings in the existing device storage.

## About

LayerGlow (formerly Creality K2 Status Light) uses an ESP32-C3 board, addressable LEDs, and a local browser interface to visualise Creality K2 printing progress. Its firmware communicates with the printer through the local network, offers status lighting and effects, and provides Wi-Fi and device configuration.

Features documented in the historical firmware include:
- Progress display on 12 or 24 addressable LEDs (in pairs).
- Configurable colours, brightness, flashing patterns, and effects.
- Browser-based configuration, preview, and LED test tools.
- Wi-Fi onboarding via `LayerGlow-Setup` in v2.37 (`K2-LED-Setup` on older firmware).
- English, Polish, Spanish, and German UI in developed versions.
- Manual OTA updates and a GitHub version-check workflow.

Specific functions depend on the installed firmware version. See the [change history](CHANGELOG.md).

## Project name and compatibility

**LayerGlow** is the project identity. The repository URL is `TomaszK00/LayerGlow`. v2.37 publishes only a LayerGlow-named OTA image. Firmware older than the LayerGlow updater may need one manual OTA upload before GitHub update detection works. **New printer integrations require testing on each printer model.** See [rename and OTA migration plan](docs/RENAMING_TO_LAYERGLOW.md).

## Firmware and source

| Item | Download |
| --- | --- |
| **OTA test candidate v2.41** | [LayerGlow .bin](Software/LayerGlow_v2_41_OTA.bin) |
| **Previous OTA v2.40** | [LayerGlow .bin](Software/LayerGlow_v2_40_OTA.bin) |
| **Previous OTA v2.39** | [LayerGlow .bin](Software/LayerGlow_v2_39_OTA.bin) |
| **Previous OTA v2.38** | [LayerGlow .bin](Software/LayerGlow_v2_38_OTA.bin) |
| **Previous OTA v2.37** | [LayerGlow .bin](Software/LayerGlow_v2_37_OTA.bin) |
| **Previous OTA v2.36** | [LayerGlow .bin](Software/LayerGlow_v2_36_OTA.bin) |
| **Previous OTA v2.35** | [LayerGlow .bin](Software/LayerGlow_v2_35_OTA.bin) |
| **Previous OTA v2.34** | [LayerGlow .bin](Software/LayerGlow_v2_34_OTA.bin) |
| **Previous OTA v2.33** | [LayerGlow .bin](Software/LayerGlow_v2_33_OTA.bin) |
| **Previous OTA v2.32** | [LayerGlow .bin](Software/LayerGlow_v2_32_OTA.bin) |
| **Previous OTA v2.31** | [LayerGlow .bin](Software/LayerGlow_v2_31_OTA.bin) |
| **Previous OTA v2.30** | [LayerGlow .bin](Software/LayerGlow_v2_30_OTA.bin) |
| **Previous OTA v2.29** | [LayerGlow .bin](Software/LayerGlow_v2_29_OTA.bin) |
| **Earlier OTA v2.28** | [LayerGlow .bin](Software/LayerGlow_v2_28_OTA.bin) |
| **Earlier OTA v2.27** | [LayerGlow .bin](Software/LayerGlow_v2_27_OTA.bin) |
| **Older OTA v2.15** | [Download .bin](Software/K2_Status_Light_v2_15_OTA.bin) |
| **Source v2.41** | [Arduino sketch](Arduino/LayerGlow_v2_41/LayerGlow_v2_41.ino) · [PlatformIO project](PlatformIO/) |
| **Source v2.40** | [Arduino sketch](Arduino/LayerGlow_v2_40/LayerGlow_v2_40.ino) · [PlatformIO project](PlatformIO/) |
| **Source v2.39** | [Arduino sketch](Arduino/LayerGlow_v2_39/LayerGlow_v2_39.ino) · [PlatformIO project](PlatformIO/) |
| **Source v2.38** | [Arduino sketch](Arduino/LayerGlow_v2_38/LayerGlow_v2_38.ino) · [PlatformIO project](PlatformIO/) |
| **Source v2.37** | [Arduino sketch](Arduino/LayerGlow_v2_37/LayerGlow_v2_37.ino) · [PlatformIO project](PlatformIO/) |
| **Source code v2.15** | [View .ino](Arduino/archive/K2_Status_Light_v2_15.ino) |
| **Archived build v2.16** | [OTA .bin](Software/archive/K2_Status_Light_v2_16_OTA.bin) · [source .ino](Arduino/archive/K2_Status_Light_v2_16.ino) |
| **All historical versions** | [Firmware archive](Software/archive/README.md) · [Source archive](Arduino/README.md) |
| **Why each version was created** | [Detailed changelog](CHANGELOG.md) |
| **OTA update manifest** | [Software/update.json](Software/update.json) |

The archive contains available original Arduino sources for v2.0, v2.1 and v2.3–v2.16, and OTA binaries from v2.3 through v2.16. **v2.2 remains unavailable** as its earlier ZIP could not be retrieved intact.

## Hardware used by the archived firmware

The v2.15 source and the packaged installation notes target:
- **ESP32-C3 SuperMini / ESP32C3 Dev Module**, 4 MB flash.
- **SK6812 RGBW** addressable LEDs, with data on **GPIO4** in the reference configuration.
- **24 LEDs maximum** in the original code, with 12/24 display layouts.
- Onboard blue status LED on **GPIO8** (active LOW on the referenced board).
- Printer network interface through **Moonraker** (default port 7125 in earlier builds).

**These are firmware settings, not a verified physical wiring diagram for every board.** Verify voltage, current, level shifting, grounds, and the GPIOs on your specific ESP32 and LED strip. Never connect printer 24 V directly to the ESP32 or 5 V LEDs. See [hardware notes](docs/HARDWARE.md).

## Installation

**Existing installation:** use the device's built-in update page with the correct compatible `_OTA.bin` file. Do not disconnect power during the update. The GitHub manifest advertises **2.37** after the new OTA binary has been compiled, checked for size and committed. This is a test candidate. Devices running older firmware may still use the former repository URL; if GitHub update detection fails there, upload the v2.37 LayerGlow OTA BIN manually once, then use the new URL for later versions.

**New ESP32 device:** see the [first-time installation guide](docs/INSTALLATION.md) for the historically packaged v2.3 USB image and the Arduino sketch option. USB flashing can erase saved settings and requires the correct chip/flash/partition settings. Firmware has not been retested on every possible board.

**Security:** several archived versions have no web-panel authentication, and the setup AP has a published default password. Use the device only on a trusted LAN; never expose its admin/OTA interface directly to the internet.

## Repository folders

- [`Arduino/archive/`](Arduino/archive/) — original source sketches by version.
- [`Arduino/legacy/`](Arduino/legacy/) — recovered v2.1 sketch from Git history.
- [`Software/`](Software/) — current update manifest and selected original OTA files.
- [`Software/archive/`](Software/archive/) — additional earlier OTA and USB images.
- [`K2/`](K2/) — planned 3D printable cases, LED bar mounts, cable guides, and diffusers.
- [`docs/`](docs/) — installation, hardware notes, release checklist and licensing.

## Documentation

- [Version-by-version changelog](CHANGELOG.md)
- [Planned improvements and feature requests](docs/ROADMAP.md)
- [LayerGlow rename and OTA compatibility plan](docs/RENAMING_TO_LAYERGLOW.md)
- [Polish version history](docs/CHANGELOG_PL.md)
- [Installation and first run](docs/INSTALLATION.md)
- [Hardware and wiring](docs/HARDWARE.md)
- [OTA publishing checklist](docs/RELEASING.md)
- [3D design roadmap](K2/README.md)
- [License and prior MIT versions](docs/LICENSING.md)
- [Contributions](CONTRIBUTING.md) · [Security](SECURITY.md)

## License

**Private, non-commercial home use only** under the current [Personal Home Use License](LICENSE); other uses require prior written permission. This is **source-available**, not OSI open source. Material previously distributed under MIT retains existing MIT permissions. See [licensing details](docs/LICENSING.md).

## Disclaimer

Independent community project; not affiliated with or endorsed by Creality. Firmware archives and their notes are preserved for reference; archived binaries and first-flash images should only be used on verified compatible hardware and are provided without warranty.
