# Security policy

## Current status

The public OTA manifest advertises v2.51 as a test candidate. It adds same-origin/Host checks and fixes update completion handling, but **it is not an authenticated or security-certified product**.

The local web/OTA interface has no administrator login, the recovery AP uses a shared default password, and Bambu MQTT currently disables server-certificate validation. Same-origin checks reduce browser cross-site writes; they do not stop an untrusted client already on the local network. No signed-firmware verification or automatic rollback acceptance gate is configured by this project.

The unreleased v2.52 candidate adds physical BOOT pairing, administrator login, a user-chosen AP password, and verification of signed OTA images. It is **not yet hardware validated or advertised by the public updater**. Existing v2.51 devices retain the limitations above until upgraded and paired. Even after pairing, Basic credentials travel over HTTP, not HTTPS, and Bambu certificate validation and physical flash protection remain separate work. See [the secure upgrade guide](docs/SECURE_UPGRADE.md).

Use a trusted, access-controlled local network and do not expose the panel to the public Internet. Hardware, phone-browser, long-duration and printer compatibility tests are still required. See [the audit](docs/AUDIT_2026-10-25.md) for scope and unresolved items.

## Reporting security concerns

Do not publish exploit details, credentials or private device information in a public issue. Use GitHub private vulnerability reporting if enabled, or request a private reporting channel without disclosing details publicly.

Never commit Wi-Fi passwords, access tokens or other credentials.
