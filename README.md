# LayerGlow

**ESP32-C3-powered LED status and progress indicator for 3D printers. Creality K2 is the documented hardware integration; other printer options in recent firmware are experimental.**

![Project status](https://img.shields.io/badge/status-development-orange)
![Current OTA](https://img.shields.io/badge/current%20OTA-v2.48-orange)
![Historical archive](https://img.shields.io/badge/historical%20archive-v2.16-informational)
![Platform](https://img.shields.io/badge/platform-ESP32--C3-green)
[![Repository views](https://hits.dwyl.com/TomaszK00/LayerGlow.svg?style=flat-square)](https://hits.dwyl.com/TomaszK00/LayerGlow)

> **OTA test candidate: v2.48.** This release is compiled by GitHub Actions and has not yet been tested on a physical ESP32-C3 or printer. v2.47 remains available in Software/ for manual recovery; v2.16 remains archived.

## v2.48 — saved printers

Save up to 8 printers, including multiple printers of the same type. Each entry shows its name, adapter, IP/port, optional manually entered MAC, Bambu serial number and Wi-Fi at save time. Connect selects one active printer; Edit changes a saved profile; Delete removes it. Disconnect retains the profile. The selected legacy printer migrates automatically. Credentials remain private to each profile and are never returned in the list API.

The profile set and selected printer are persisted together as one NVS blob. Wi-Fi and LED settings remain independent. MAC is optional and is not read automatically.

Downloads: [OTA BIN](Software/LayerGlow_v2_48_OTA.bin) · [Arduino source ZIP](Software/LayerGlow_v2_48.zip).

## v2.47 — LED controls and live printer data

Opening brightness or pulse closes the previous LED adjustment. Live printer data is labelled and highlighted separately from test states in all four languages. Save printer already keeps settings across restarts; the panel now explains this next to the button. It stores one configuration for each adapter type, without a multiple-printer profile list.

Downloads: [OTA BIN](Software/LayerGlow_v2_47_OTA.bin) · [Arduino source ZIP](Software/LayerGlow_v2_47.zip).

## v2.46 audit fixes

The current v2.46 firmware contains connection, persistence and OTA fixes following the [25 October audit](docs/AUDIT_2026-10-25.md) / [raport po polsku](docs/AUDIT_2026-10-25_PL.md). HTTP printer reads run in a separate task; Bambu no longer leaves stale live readings after a disconnect, and recovery AP clients no longer stop saved-network retries indefinitely. Saves report storage failures and OTA confirms a new boot rather than assuming a restart after six seconds.

Downloads: [v2.46 OTA binary](Software/LayerGlow_v2_46_OTA.bin) · [v2.46 Arduino source ZIP](Software/LayerGlow_v2_46.zip).

The public updater on `main` advertises v2.48 for device testing. The v2.46 audit-fix image below remains available. The [build and regression tests passed](https://github.com/TomaszK00/LayerGlow/actions/runs/40193438093). OTA: 1766592 bytes; SHA-256 `06801244c5bf4f185d869b40513a4cea19f0e3627ed5a90d49559ed44ee6e448`. This does not establish hardware stability or resolve the authentication/TLS limitations described in the audit.

## v2.45 test candidate

Restore the browser's original colour picker when a LED swatch is tapped. Its eight suggested colours include the seven standard shades and the most recently chosen custom colour in place of the white suggestion. Custom colour choices are stored on the device and automatically offered for the next swatch and after a restart. The appearance and placement of suggestions depend on the browser.

OTA v2.45: 1,759,248 bytes; SHA-256 `339f5aa0292aecc9b5936d94f521854023671e75e236be338220609f486a10e6`.

## v2.44 test candidate

Tap any colour swatch to open a shared colour chooser. The last eight chosen colours are saved on the LayerGlow device and can be reused for another swatch, stage, browser or after a restart. Pick New colour to open the system colour picker. Existing custom LED colours seed the remembered list on the first upgrade, and LED settings continue saving automatically.

OTA v2.44: 1,762,992 bytes; SHA-256 `2267906a7aee4c13a86976f23c94169478dc9b1e69c08a7a18759de5d02230c8`.

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
- Wi-Fi onboarding via `LayerGlow-Setup` in current LayerGlow firmware (`K2-LED-Setup` on older firmware).
- English, Polish, Spanish, and German UI in developed versions.
- Manual OTA updates and a GitHub version-check workflow.

Specific functions depend on the installed firmware version. See the [change history](CHANGELOG.md).

## Project name and compatibility

**LayerGlow** is the project identity. The repository URL is `TomaszK00/LayerGlow`. v2.43 publishes a LayerGlow-named OTA image. Firmware older than the LayerGlow updater may need one manual OTA upload before GitHub update detection works. **New printer integrations require testing on each printer model.** See [rename and OTA migration plan](docs/RENAMING_TO_LAYERGLOW.md).

## Firmware and source

| Item | Download |
| --- | --- |
| **OTA test candidate v2.48** | [LayerGlow .bin](Software/LayerGlow_v2_48_OTA.bin) |
| **Previous OTA v2.47** | [LayerGlow .bin](Software/LayerGlow_v2_47_OTA.bin) |
| **Previous OTA v2.46** | [LayerGlow .bin](Software/LayerGlow_v2_46_OTA.bin) |
| **Previous OTA v2.43** | [LayerGlow .bin](Software/LayerGlow_v2_43_OTA.bin) |
| **Previous OTA v2.42** | [LayerGlow .bin](Software/LayerGlow_v2_42_OTA.bin) |
| **Previous OTA v2.41** | [LayerGlow .bin](Software/LayerGlow_v2_41_OTA.bin) |
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
| **Source v2.43** | [Arduino sketch](Arduino/LayerGlow_v2_43/LayerGlow_v2_43.ino) · [PlatformIO project](PlatformIO/) |
| **Source v2.42** | [Arduino sketch](Arduino/LayerGlow_v2_42/LayerGlow_v2_42.ino) · [PlatformIO project](PlatformIO/) |
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

**Existing installation:** use the device's built-in update page to check GitHub for **v2.48**, or choose the compatible `_OTA.bin` file manually. Do not disconnect power during the update. The OTA image is a test candidate. Devices running older firmware may still use the former repository URL; if GitHub update detection fails there, upload a recent LayerGlow OTA BIN manually once, then use the new URL for later versions.

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
