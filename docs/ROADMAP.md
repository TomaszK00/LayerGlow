# Development roadmap

This document records requested enhancements to **LayerGlow**. Items below are **planned / to verify**, not advertised as already implemented. The firmware archive and OTA release manifest remain authoritative for released features.

Last updated: 25 October 2026.

## Multi-printer support

- [ ] Abstract printer communication into adapters rather than assuming every printer supports Moonraker.
- [ ] Test and document each future printer integration before marking it as supported.
- [x] Rename the public repository to `TomaszK00/LayerGlow`.
- [ ] Verify OTA update discovery and installation from older K2-branded firmware after the repository rename, on real hardware.

## Current status

- **v2.51 is the current OTA test candidate; code audit passed, but full hardware acceptance is outstanding.** Previously requested UI changes may be implemented in current code; check the live panel before closing the tasks below.
- Keep physical OTA testing and backward compatibility checks open until verified.

## Confirmed user checks (25 October 2026)

- [x] Creality K2 printer discovery tested and working (reported by the project owner).
- [x] Adding multiple saved Wi-Fi networks tested and working (reported by the project owner).
- [ ] Test automatic Wi-Fi failover/reconnection and persistence across power cycles.
- [ ] Test real print-state/progress, LED output and OTA/recovery on ESP32-C3 hardware.
- [ ] Complete end-to-end hardware acceptance for v2.51.

See [verification status](TEST_STATUS.md).

## Web interface and LED configuration

- [x] Implement **+ / − controls** for up to **12** palette colours in the v2.37 test candidate; hardware testing remains outstanding.
- [x] Implement responsive colour rows including mobile + / − placement (1–4, 5–10, 11–12) in v2.37; verify on physical mobile devices.
- [ ] On mobile, replace the always-visible language flags with a **Languages** menu immediately to the right of **FAQ**, with a list of supported languages.
- [ ] Improve mobile navigation so the top settings/menu header behaves consistently while scrolling, without hiding access to key controls.
- [ ] Verify that changes to colour palettes, effect settings, and LED-count selection persist through save and reboot.

## FAQ and appearance

- [x] Expand FAQ with searchable setup, Wi-Fi, printer, LED and OTA guidance in four languages (v2.35–v2.36).
- [x] Integrate onboard blue status LED patterns as expandable FAQ entries (v2.36).
- [x] Implement automatic, light and dark appearance modes in the footer (v2.36).
- [ ] Verify mobile layouts, theme persistence, FAQ search and translations in a physical device/browser test.

## Network and printer status

- [ ] Expand **Network and printer status** to show meaningful connection diagnostics after joining home Wi-Fi: connection state, SSID, device IP address, subnet mask, gateway, DNS, signal strength, and connected printer host/IP and port, wherever available.
- [ ] Present separate Wi-Fi, printer API and update-server connection states; show clear failure messages.
- [x] Provide a **stable local hostname** in the current v2.37 test candidate, e.g. `http://layerglow.local/`, using mDNS where supported, even if the DHCP-assigned IP address changes.
- [ ] Verify hostname discovery and `layerglow.local` resolution on target routers and client devices; `.local` may not work across isolated networks.
- [ ] Never show saved Wi-Fi passwords or secret tokens in diagnostics, logs, exports or screenshots.

## Release requirements

- [ ] Implement and test each improvement in the current Arduino source.
- [ ] Test desktop and mobile layouts, 12/24 LED configurations, network disconnection/recovery, Wi-Fi scans, and printer status handling.
- [ ] Compile firmware for the documented ESP32-C3 target and check flash/OTA partition limits.
- [ ] Publish a tested OTA binary and release notes before changing `Software/update.json`.
- [ ] Update README, installation guides, translations and changelog when a feature is confirmed working.

## Related documents

- [README](../README.md)
- [Firmware history](../CHANGELOG.md)
- [Hardware and wiring](HARDWARE.md)
- [Release checklist](RELEASING.md)

Contributions are welcome as proposals via Issues or Pull Requests, but publishing changed copies or derivatives is subject to the repository license and existing third-party or earlier-license rights.
