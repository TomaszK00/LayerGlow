# Release and OTA checklist

## Active firmware and candidate

`Software/update.json` on **main** advertises v2.45. A manifest on **candidate/v2.46** is only a test build and is not used by the normal updater. The active manifest format/path must remain compatible with installed devices.

## Checks before promotion

1. Run `python -m unittest discover -s tests -p 'test_*.py' -v` and `npm ci --ignore-scripts --prefix tests && npm test --prefix tests`.
2. Compile using pinned PlatformIO dependencies. Keep the Arduino sketch and bundled MQTT source identical to the corresponding PlatformIO source.
3. Use the same compatible 4 MB flash/OTA partition layout. Check the actual device's `/api/ota/capacity`; a larger binary does not resize an existing partition table.
4. Confirm the candidate binary size and SHA-256 against the candidate manifest.
5. Install on hardware and perform the acceptance scenarios in [the audit](AUDIT_2026-10-25.md). Preserve the previous file for manual recovery.
6. Only after acceptance, promote the binary and manifest together to main. The candidate workflow never promotes main automatically.
7. Update the changelog, documentation and known limitations; do not call a compiled-only candidate hardware-tested.

The workflow runs regression checks and compilation for main, candidate branches and pull requests. Only a candidate branch can commit its staged binary/manifest back to itself. Branch protection/required checks must be configured separately in GitHub settings.

## Manifest

The fields remain `version`, `file`, `size` (bytes), and lowercase `sha256`. For the exact active values read [Software/update.json](../Software/update.json). Firmware filenames use `LayerGlow_vX_YY_OTA.bin`.

The v2.46 restart check becomes available after installing v2.46: the upgrade **to** it is still driven by the old firmware's web page. Installing the same version should be tested too. A firmware image without a boot ID uses version/uptime evidence; an ambiguous reboot times out rather than being assumed successful.
