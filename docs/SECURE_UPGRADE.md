# Security candidate v2.52: pairing and signed OTA

This candidate is for controlled testing on an ESP32-C3 SuperMini. Keep a USB recovery path and the previous v2.51 OTA file. A successful build does not establish physical button, radio or printer behaviour.

## Upgrade an existing device

1. Confirm the device runs v2.51, note its Wi-Fi IP and ensure you can press its physical **BOOT** button (GPIO9 on the reference SuperMini).
2. Upload the v2.52 `_OTA.bin` from the existing v2.51 update page. This is the **one legacy unsigned transition**: v2.51 does not yet verify signatures. Use only a trusted network and the file from the project repository.
3. After restart, open the device page. Enter a **new administrator password** and **a new LayerGlow-Setup Wi-Fi password**, both at least 12 characters. Hold **BOOT** while pressing Save. Write down both passwords before proceeding.
4. The device restarts. The browser will ask for username `admin` and the new administrator password. If you need the setup AP, reconnect using the new Wi-Fi password. Saved Wi-Fi/printer and LED settings are preserved.
5. Subsequent manual updates require **both** `_OTA.bin` and the matching `_OTA.bin.sig` file. GitHub updates use the manifest's signature automatically. A missing or invalid signature cannot activate the new firmware.

Before step 3, devices upgraded with previously saved Wi-Fi may temporarily use the old published setup AP password during recovery. Pair promptly. Pairing itself requires the physical BOOT button. Do not expose the panel on the public Internet.

## First flash with no saved Wi-Fi

On first boot a unique 20-character setup AP password is generated, saved on the device, and printed on the **115200 baud USB serial console**. Record it before disconnecting USB. The browser still requires physical BOOT pairing and a new password of your choice. If storage of the unique password fails, the setup AP stays off instead of reverting to a published password.

## Lost password and signing key

- Hold **BOOT** for 10 seconds while the firmware is running to clear the administrator login, then pair again. The chosen setup AP password remains in place.
- The signer private key must be kept offline and backed up securely. Only `security/ota_public.pem` belongs in the repository. Set repository secret `LAYERGLOW_OTA_SIGNING_KEY` to the private PEM to let candidate CI stage future signed images. Without it, CI still tests and compiles but does not publish an OTA file.
- For an offline release, run `python security/sign_ota.py --image PlatformIO/.pio/build/esp32c3/firmware.bin --key /secure/path/private.pem --version 2.52`. Keep the private PEM out of git. The command checks that the signing key matches the pinned public key and writes BIN, SIG and `update.json` together.
- Losing the signer prevents future OTA updates. A compatible USB flash can recover the device because this candidate does not enable hardware Secure Boot. USB flashing can erase saved settings.

## Limits

HTTP Basic authentication protects panel operations from someone who does not know the password; **HTTP does not encrypt the password or session traffic**. Use a trusted WPA-protected LAN/AP and do not forward port 80. This candidate does not yet solve the separate Bambu `setInsecure()` certificate issue, encrypt NVS data, or provide signed boot/anti-rollback protection against physical flash replacement.
