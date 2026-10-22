# BLE checks

Use the nRF5340 DK, display, configured NFC tag, and Pixel Badge Android app.
Build/flash the [Prototype image](../firmware/nrf5340_prototype/README.md),
then [build the app](../app/README.md#development).

Hold Action to open the 60-second pairing window. Connect in the app and enter
the code shown on the badge. Start with **Refresh**.

| Check | Expected result |
|---|---|
| Send text and change brightness/mode | The display matches the confirmed readback |
| Apply a short contact URL | QR, NFC, and app readback agree |
| Restart without erasing flash | Settings survive; the bonded phone reconnects |
| Test the 32-byte legacy name field | Exact readback of `ABCDEFGHIJKLMNOPQRSTUVWXYZ123456` |
| Disconnect during Apply | Refresh resolves the result; no automatic replay |
| Attempt protected access without pairing | Reads/writes are rejected; settings stay unchanged |
| Enter a wrong passkey | Pairing fails |
| Leave pairing unanswered until expiry | The code clears and pairing is cancelled |
| Factory reset, last | Settings/bond clear; wired NFC setup and pairing restore operation |

Security tests need a fresh central and an available bond slot. Rejection because
the bond table is full does not test the passkey. For expiry, connect late in
the window; distinguish badge expiry from a phone-side timeout.

Record firmware/app versions, phone/OS, actions, readback, and non-secret logs.
Keep manual observations separate from captured results. Factory reset erases
settings and the bond, but leaves the external tag's EEPROM.

[Recorded results](../validation/results/2026-10-13-dk-p2-settings/README.md)
include persistence and pairing checks. Unauthorized ATT writes and complete
reset/recovery testing remain open. Current NFC setup uses
[local synchronization](nfc-prototype.md), without a phone-verifier receipt.
