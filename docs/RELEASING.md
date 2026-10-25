# Release and OTA checklist

## Active firmware and candidate

`Software/update.json` on **main** advertises v2.51 as a test candidate so existing devices can download it through the normal updater. The source and binary passed automated checks, but physical device and printer validation is still pending. The active manifest format/path remains compatible with installed devices.

## Release and acceptance checks

1. Run `python -m unittest discover -s tests -p 'test_*.py' -v` and `npm ci --ignore-scripts --prefix tests && npm test --prefix tests`.
2. Compile using pinned PlatformIO dependencies. Keep the Arduino sketch and bundled MQTT source identical to the corresponding PlatformIO source.
3. Use the same compatible 4 MB flash/OTA partition layout. Check the actual device's `/api/ota/capacity`; a larger binary does not resize an existing partition table.
4. Confirm the candidate binary size and SHA-256 against the candidate manifest.
5. Install on hardware and perform the acceptance scenarios in [the audit](AUDIT_2026-10-25.md). Preserve the previous file for manual recovery.
6. Promote the binary and manifest together to main when offering a candidate through the browser. The candidate workflow never promotes main automatically.
7. Update the changelog, documentation and known limitations; do not call a compiled-only candidate hardware-tested. Keep hardware acceptance separate from a published test candidate.

The workflow runs regression checks and compilation for main, candidate branches and pull requests. Only a candidate branch can commit its staged binary/manifest back to itself. Branch protection/required checks must be configured separately in GitHub settings.

## Manifest

The fields remain `version`, `file`, `size` (bytes), and lowercase `sha256`. For the exact active values read [Software/update.json](../Software/update.json). Firmware filenames use `LayerGlow_vX_YY_OTA.bin`.

The v2.46 restart check becomes available after installing v2.46: the upgrade **to** it is still driven by the old firmware's web page. Installing the same version should be tested too. A firmware image without a boot ID uses version/uptime evidence; an ambiguous reboot times out rather than being assumed successful.
