# BLE checks

Works on the DK prototype or a Rev A board built with `matrix-bench.conf`.
Flash the firmware, [build the app](../app/README.md#development), hold Action
to open the 60-second pairing window, connect in the app and type the code
the badge shows. Start with **Refresh**.

| Check | Expected |
|---|---|
| Send text, change brightness and mode | The display matches the readback |
| Apply a short link | QR, NFC and the app agree |
| Restart without erasing | Settings survive and the phone reconnects |
| 32-character name `ABCDEFGHIJKLMNOPQRSTUVWXYZ123456` | Reads back exactly |
| Disconnect during Apply | Refresh shows the outcome; nothing replays on its own |
| Protected access without pairing | Refused, settings unchanged |
| Wrong passkey | Pairing fails |
| Let the pairing window run out | The code clears and pairing is cancelled |
| Factory reset, last | Settings and bond clear; the NFC tag keeps its contents |

Security tests need a fresh phone and a free bond slot. A full bond table
rejects for a different reason and doesn't test the passkey. Note firmware
and app versions, phone and OS, and what you saw versus what was logged.
[DK results](../validation/results/2026-10-13-dk-p2-settings/README.md).
