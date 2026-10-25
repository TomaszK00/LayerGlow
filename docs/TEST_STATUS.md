# Verification status — LayerGlow

Last updated: 25 October 2026. Current public OTA candidate when this report was recorded: **v2.51**.

## Reported results

| Item | Status | Scope |
| --- | --- | --- |
| Code audit and automated checks | **Passed** | Audit/regression checks; not a hardware certification |
| Printer discovery | **Passed (user reported)** | Tested with a real **Creality K2**; printer detection works |
| Multiple saved Wi-Fi networks | **Passed (user reported)** | Adding and saving multiple Wi-Fi networks works |
| Full hardware acceptance | **Not completed** | No comprehensive verification of ESP32-C3, LED strip, firmware lifecycle and long-running printer operation |
| Other printer models | **Not verified** | Adapter availability does not demonstrate compatibility |
| OTA update/recovery on hardware | **Not verified** | Build/manifest checks do not prove reliable flashing or rollback |

The two user-reported functional successes above must **not** be interpreted as a complete hardware-validation pass. In particular, a successful Creality K2 printer discovery test is narrower than end-to-end live job-state/progress tracking, Wi-Fi failover or 24-hour operation.

## Remaining acceptance work

- Install the current candidate on the target ESP32-C3 and check reboot, settings persistence and OTA update/recovery.
- Verify physical LED animations, progress mapping, brightness and power stability.
- Confirm actual live print state/progress from Creality K2 while printing, during pause, completion, disconnection and recovery.
- Test switching, reconnecting and removing saved Wi-Fi networks (not only adding them), including recovery AP and restart behaviour.
- Verify saved printer selection, restart persistence and disconnect/rescan behaviour with multiple printer profiles.
- Perform soak tests and check web UI responsiveness, heap and network stability.
- Verify each additional printer model independently.

See [the technical audit](AUDIT_2026-10-25.md) and [release checklist](RELEASING.md). Continue to label v2.51 as a **test candidate** pending full acceptance.
