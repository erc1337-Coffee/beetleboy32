# Roadmap

## OTA updates (test on the real board, not the sim)

- [ ] **Check:** the About screen's "Check for updates" fetches `<repo raw URL>/project.env` (default branch) and compares its `VERSION` with the built-in one.
  - On GitHub, raw URLs look like `https://raw.githubusercontent.com/<owner>/<repo>/<branch>/project.env`.
- [ ] **Release asset:** each release attaches `firmware.bin` (`./run build` makes it: `.pio/build/waveshare/firmware.bin`). Naming convention: tag `v<VERSION>`.
- [ ] **Install:** download over HTTPS into the inactive OTA slot (Arduino `Update` / `esp_https_ota`), verify, switch the boot partition, reboot.
  - `default_16MB.csv` already has two OTA app slots.
- [ ] **Safety:** mark the new image valid only after it boots and reaches Home (app rollback), so a bad release falls back to the previous one.

- **Secure mode:** with Secure Boot, releases must be signed with the project key. Forks need their own key, or no Secure Boot.
- The repo is public, so the root CA bundle must cover `raw.githubusercontent.com` and GitHub's release CDN, not just remilia.net.

## Secure mode (optional, later)

Goal: stored secrets (cookies, refresh token, Wi-Fi password) not readable from a stolen device.

- [ ] **NVS encryption, HMAC scheme**: the key is derived by the HMAC peripheral from a read-protected eFuse key. Firmware can burn it on first use. `ConfigStore` code stays unchanged
- [ ] **Flash encryption** (XTS-AES, key in eFuse): protects firmware and other flash data
- [ ] **Secure Boot v2**: required, otherwise attacker firmware can *use* the eFuse key even though it can't read it

- Constraints:
  - eFuse burns are irreversible, so this is a per-device provisioning step, not a runtime toggle
  - Release mode blocks plaintext reflashing, so updates go through OTA
- **Toolchain**: the prebuilt Arduino core (`espressif32@6.9.0`) can't enable these. It needs ESP-IDF with Arduino as a component (e.g. pioarduino) and a custom sdkconfig
- **Threat level**: stops casual and software extraction. Lab-grade fault-injection and side-channel attacks on ESP32 chips have been published
- No extra app-level AES: NVS encryption already is AES with a hardware-held key

## UX notes

- The setup page sends the account password from the phone to the device over the setup Wi-Fi (WPA2, random password on screen), plain HTTP inside that network; the device then talks to remilia.net over TLS

- [ ] Verify the KEYCLOAK_IDENTITY length on a real account (NVS string limit is 4000 bytes)

## Misc

- [ ] Power key: hold it 2-3 s to switch off (drive SYS_EN, GPIO41, low; the key reads on SYS_OUT, GPIO40, low while pressed). Off, the board's own circuit cuts the battery; USB still charges (ETA6098 needs no firmware). Optional hard switch instead: in series on the battery's red wire, >=1 A DC (then USB can't charge while it's off)

## User-Experience

- [ ] Demo website
- [x] Web Documentation