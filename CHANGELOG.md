# Firmware changelog

## v2.47 — interface refinements (25 October 2026)

- Opening brightness or pulse closes the previous LED adjustment, across all stages.
- Rename printer reading to live printer data in all four languages and distinguish it from test states with a dot and colour.
- Explain that Save printer persists settings across restarts, independently of Wi-Fi. No additional printer-profile manager is introduced.
- Network, printer protocols and storage format remain unchanged.


## v2.46 — audit fix candidate (25 October 2026)

- Run Moonraker, OctoPrint and PrusaLink HTTP reads in a worker task using immutable request/result snapshots. Discard results for an older printer/network configuration.
- Clear stale Bambu connection state and LEDs on Wi-Fi/MQTT loss. Avoid unsigned timeout underflow when a report arrives during the MQTT loop.
- Retry saved Wi-Fi networks every 30 seconds while a recovery phone is connected, preserving the recovery AP.
- Verify storage writes and return errors instead of false save confirmations. Translate save/restore failures in all four UI languages.
- Require a completed, nonempty upload before restarting. Accept uppercase .BIN filenames, bound browser requests and confirm OTA by boot identity or compatible legacy evidence.
- Reject browser write requests with an unrecognized Host or cross-origin Origin. This is not login/authentication.
- Add host C++ and browser regression tests plus CI checks for main, candidate branches and pull requests.
- Publish v2.46 as the normal GitHub OTA test target so it can be installed from the browser. Physical hardware acceptance is still pending. Native colour picker and last-pair pulse semantics remain unchanged.

## v2.45 — test candidate (24 October 2026)

- Revert the extra remembered-colour dialog and restore the browser's original native colour picker.
- Offer the last saved custom colour as the eighth suggested swatch in place of white, alongside seven unchanged preset colours. Store custom selections on the device and reuse them across LED fields.
- Update the FAQ in four languages; native picker presentation may vary by browser.
- GitHub Actions compilation passed; physical hardware validation remains pending.

## v2.44 — test candidate (24 October 2026)

- Add a colour chooser with eight remembered colours shared across LED swatches and stages, plus the system colour picker for a new colour.
- Store colour history on the device so it survives a restart and is available from another browser. Seed it from existing custom LED colours on first use.
- Keep automatic LED settings saving and update the FAQ in all four languages.
- GitHub Actions compilation passed; physical hardware validation remains pending.

## v2.43 — test candidate (24 October 2026)

- Selecting a test state immediately activates it on the preview and connected LEDs. Printer reading turns the simulation off.
- Printing now advances by 5% every 500 ms, holds at 100% for 1 second and repeats; other states remain selected until changed.
- Remove the automatic-progress and repeat checkboxes and the manual test-progress slider. Animate the browser preview between normal status polls and update the FAQ in four languages.
- GitHub Actions compilation passed; physical hardware validation remains pending.

## v2.42 — test candidate (24 October 2026)

- Place Test state, progress, lit LED count and current state on one quiet line in the LED preview, including the compact mobile view.
- Keep the real printer state and percentage visible when Printer reading is selected; show no live percentage while the printer is disconnected.
- GitHub Actions compilation passed; physical hardware validation remains pending.

## v2.41 — test candidate (24 October 2026)

- Add Printer reading to Test state. It disables simulation; every other state activates the test on the preview and connected strip.
- Remove the Test mode checkbox and move Play progress automatically into its place. Keep Repeat test after 100% beside it; adjust the mobile progress label for live readings.
- Update the FAQ in four languages and verify the switch between printer, test states and automatic test progress. GitHub Actions compilation passed; hardware testing remains pending.

## v2.40 — test candidate (24 October 2026)

- Keep the LED preview in one fixed-width row for every physical strip length.
- Show up to 48 preview lights; above 48 physical LEDs, sample colours, progress and effects across the complete strip. Show a short explanation only when the preview is scaled.
- The configured physical LED count and connected strip remain unchanged. Check preview logic at 5, 24, 25, 48, 49 and 300 LEDs and all 14 effects. GitHub Actions compilation passed; hardware testing remains pending.

## v2.39 — test candidate (24 October 2026)

- Replace the footer's three round theme controls with labelled Auto, Light and Dark checkboxes in one row.
- Keep only one mode selected at a time. Preserve saved browser preferences and automatic response to the device appearance.
- Interface logic checks and GitHub Actions compilation passed; physical device testing remains pending.

## v2.38 — test candidate (24 October 2026)

- On phones, hide the top menu when the Colours and LED settings heading goes beneath the sticky LED preview; show it again when the heading reappears. The desktop menu stays fixed.
- Replace the footer theme dropdown with three selectable dots on one line: automatic, light and dark. Keep the selected mode labelled, translated and saved in the browser.
- Interface logic checks and GitHub Actions compilation passed; physical device testing remains pending.

## v2.37 — test candidate (24 October 2026)

- On phones, place the colour add/remove controls in row one for 1–4 colours, row two for 5–10, and row three for 11–12. Keep the controls within the card on narrow screens.
- Show the selected effect name next to the Effect label above its selector.
- Interface logic checks and GitHub Actions compilation passed; physical device testing remains pending.

## v2.36 — test candidate (24 October 2026)

- Move blue status LED signals into three expandable FAQ questions, with compact visual blink patterns. Remove the separate LED information block.
- Add a footer appearance control with automatic system, light and dark modes for the full interface. Save the choice in the browser and respond to system appearance changes in automatic mode.
- Update all four translations and check FAQ search and theme logic. GitHub Actions compilation passed; physical device testing remains pending.

## v2.35 — test candidate (24 October 2026)

- Expand FAQ to 29 searchable questions across setup, Wi-Fi, printer connections, LED settings and colours, test mode, backups and OTA issues.
- Translate the complete new guide into English, Polish, Spanish and German. Present answers as compact expandable rows in the existing mobile full-screen FAQ panel.
- UI logic checks and GitHub Actions compilation passed; physical device testing remains pending.

## v2.34 — test candidate (23 October 2026)

- Replace user-facing ESP32/ESP32-C3 labels with LayerGlow in the network panel, OTA instructions, FAQ and restart messages across English, Polish, Spanish and German.
- Preserve the precise ESP32-C3 board and chip names in technical build and flashing instructions.
- GitHub Actions build passed; physical device testing remains pending.

## v2.33 — test candidate (23 October 2026)

- Set the Wi-Fi DHCP hostname before starting the radio. Routers can now show `LayerGlow` in their device lists instead of the ESP32-generated name.
- Preserve the existing `layerglow.local` mDNS address and `LayerGlow-Setup` recovery AP. A custom .local device name is sent as the router name on the next boot.
- GitHub Actions build passed; physical router verification remains pending.

## v2.32 — test candidate (23 October 2026)

- Widen the desktop colour column so 12 swatches and +/- actions do not overlap Brightness; increase stage name text by 1 px and shorten Brightness labels.
- Keep the LayerGlow name in English, Polish, Spanish and German; add FatGiantCat credit to the page title, header and footer.
- Add the new LED favicon to the embedded page and the repository.
- Publish only `LayerGlow_v2_32_OTA.bin` in the new OTA build and remove duplicate K2-named aliases from v2.27–v2.31. Older K2 firmware binaries remain historical artifacts.
- UI checks and GitHub Actions compilation passed; physical ESP32-C3 and printer testing remain pending.

## v2.31 — test candidate (23 October 2026)

- Keep collapsed Brightness and Pulse controls on the same row, including phone widths below 380 CSS pixels.
- Collapse the expanded mobile slider about 800 ms after its value is changed; its chosen value remains visible.
- JavaScript UI checks and GitHub Actions compilation passed; hardware testing remains pending.

## v2.30 — test candidate (23 October 2026)

- Put mobile navigation text and language flag in one row while preserving tappable controls.
- Show a compact test progress, lit LED count and printer state line; hide the manual progress slider and large percentage on mobile.
- Keep Brightness and Pulse side by side even when expanded; display the selected effect beside Effect.
- DOM interaction checks and GitHub Actions build passed; device testing is still pending.

## v2.29 — test candidate (23 October 2026)

- Make mobile navigation buttons, card spacing, typography and form controls consistent across the panel.
- Keep the sticky LED preview compact when scrolling and make stage controls clearer on narrow phones.
- Expand opened brightness and Pulse controls across the LED card; keep six palette colours per row where space permits.
- CSS parsing, interaction checks and GitHub Actions compilation passed; physical device testing is still pending.

## v2.28 — test candidate (23 October 2026)

- Mobile Network, Printer, Update and FAQ panels open full screen and close with their Close buttons; Language remains a popup.
- LED brightness uses an expandable 0–100% slider, and Pulse uses an expandable checkbox with a 0–5000 ms slider in every stage.
- Keep the selected effect visible in each mobile row and shorten the Test mode label so Repeat test fits beside it.
- JavaScript UI checks passed. GitHub Actions compiled the OTA binary; device and printer testing remains pending.

## v2.27 — test candidate (23 October 2026)

- Split Network and Printer into separate panels; Wi-Fi Save & Connect uses one button.
- Add live printer readout, multiple discovery results, disconnect, rescan, and local Moonraker scanning.
- Show 0–100% printer discovery progress and mobile language flags.
- Rename panel and OTA links to LayerGlow; keep a legacy filename alias for older updater compatibility.
- Compiled with GitHub Actions; requires testing on the physical ESP32-C3 and printer.

Release descriptions are based on the README files included with the original firmware packages and on preserved repository history. They document **why each build was prepared**, not a claim that every feature was independently tested on physical hardware.

**Published automatic OTA target:** v2.47 test candidate via [`Software/update.json`](Software/update.json). v2.16 is a separately archived historical build.

| Version | Why it was created / main changes | Files |
| --- | --- | --- |
| **2.16** | Improved Wi-Fi scanning while in `K2-LED-Setup` AP mode; suspend reconnect attempts during scans; distinguish no networks from scan failures; added internal save/restore backup for LED configurations. **Archived, not advertised by current OTA manifest.** | [source](Arduino/archive/K2_Status_Light_v2_16.ino) · [OTA](Software/archive/K2_Status_Light_v2_16_OTA.bin) |
| **2.15** | Fixed Wi-Fi recovery: auto-start AP after ~8 seconds without home Wi-Fi, background reconnect attempts, radio recovery, delayed AP shutdown and automatic reconnection after changing networks. | [source](Arduino/archive/K2_Status_Light_v2_15.ino) · [OTA](Software/K2_Status_Light_v2_15_OTA.bin) |
| **2.14** | Red **Update** button with white lettering when a GitHub update is detected. New effects: comet, traveling fill and alternating LED pairs. | [source](Arduino/archive/K2_Status_Light_v2_14.ino) · [OTA](Software/K2_Status_Light_v2_14_OTA.bin) |
| **2.13** | Added independently timed **Random Pairs** LED effect with adjustable speed, also reflected in LED preview and test. | [source](Arduino/archive/K2_Status_Light_v2_13.ino) · [OTA](Software/archive/K2_Status_Light_v2_13_OTA.bin) |
| **2.12** | Changed onboard blue LED's update-available signal to two long flashes followed by one short flash; documented pattern in multilingual FAQ. | [source](Arduino/archive/K2_Status_Light_v2_12.ino) · [OTA](Software/K2_Status_Light_v2_12_OTA.bin) |
| **2.11** | Added per-status **LED On** toggles and automatic saving; removed the edges-center-edges effect with settings migration; improved phone layout. | [source](Arduino/archive/K2_Status_Light_v2_11.ino) · [OTA](Software/archive/K2_Status_Light_v2_11_OTA.bin) |
| **2.10** | Addressed failures detecting GitHub updates when `update.json` was missing; added fallback binary discovery and new onboard LED Wi-Fi/update signals. A manifest was published alongside this version. | [source](Arduino/archive/K2_Status_Light_v2_10.ino) · [OTA](Software/archive/K2_Status_Light_v2_10_OTA.bin) |
| **2.9** | Streamlined update panel, added explicit **Update** dialog, refreshed header/footer layout and improved GitHub update checking. | [source](Arduino/archive/K2_Status_Light_v2_9.ino) · [OTA](Software/archive/K2_Status_Light_v2_9_OTA.bin) |
| **2.8** | Introduced GitHub manifest-based updates (version, binary size and SHA-256 verification); expanded/finished interface translations and animated effects. | [source](Arduino/archive/K2_Status_Light_v2_8.ino) · [OTA](Software/archive/K2_Status_Light_v2_8_OTA.bin) |
| **2.7** | Automatic web-panel refresh after OTA, persistent connection warnings and new centre/edge/wave-style LED effects. | [source](Arduino/archive/K2_Status_Light_v2_7.ino) · [OTA](Software/archive/K2_Status_Light_v2_7_OTA.bin) |
| **2.6** | Reworked dashboard and LED settings layout; added individual lighting effects per status (including Knight Rider), toasts and clearer progress display. | [source](Arduino/archive/K2_Status_Light_v2_6.ino) · [OTA](Software/archive/K2_Status_Light_v2_6_OTA.bin) |
| **2.5** | Added per-status pulse settings, automatic LED progress test, reset of full LED settings and clearer OTA progress notices. | [source](Arduino/archive/K2_Status_Light_v2_5.ino) · [OTA](Software/archive/K2_Status_Light_v2_5_OTA.bin) |
| **2.4** | Enabled automatic saving of LED settings, restoration of default colours and clearer instructions for changing Wi-Fi. | [source](Arduino/archive/K2_Status_Light_v2_4.ino) · [OTA](Software/archive/K2_Status_Light_v2_4_OTA.bin) |
| **2.3** | Expanded 12/24 LED configuration, pulsation, looping tests and status LED; packaged a first-install USB image alongside OTA and four language options. | [source](Arduino/archive/K2_Status_Light_v2_3.ino) · [OTA](Software/archive/K2_Status_Light_v2_3_OTA.bin) · [USB](Software/archive/K2_Status_Light_v2_3_USB.bin) |
| **2.2** | An earlier ZIP is listed in the personal file archive but its original bytes could not be retrieved; no faithful copy or verified change description is published here. | *Unavailable* |
| **2.1** | Added browser Wi-Fi network selection, English/Polish controls, web OTA upload and LED preview/test functionality. | [source](Arduino/legacy/K2_Status_Light_v2_1.ino) |
| **2.0** | Initial ESP32-C3 web/network and Moonraker print-status prototype, intended for testing without physical LED strip. | [source](Arduino/archive/K2_Status_Light_v2_0.ino) |

## Important release distinctions

- **OTA binaries** (`_OTA.bin`) update compatible already-installed firmware; they are **not automatically safe for first-time USB flashing**.
- The **v2.3 USB image** is provided as a historical combined flash image intended for ESP32-C3 4 MB at address `0x0`. Using it can erase saved Wi-Fi/printer configuration. Read [installation instructions](docs/INSTALLATION.md) before proceeding.
- Old binaries are saved under `Software/archive/`; previously published v2.12/v2.14/v2.15 images stay in `Software/`. The automatic update manifest is deliberately unchanged.
- Firmware notes are from original package documentation. Testing of each compiled image on real user hardware has **not** been independently re-performed during archiving.
- Earlier copies distributed under MIT preserve their existing MIT license rights. See [licensing notes](docs/LICENSING.md).
