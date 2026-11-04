# BLE and NFC tests

Note firmware and app versions, the phone or reader, exact steps and results.
The DK bench guides cover the basics: [BLE](../ble-bench.md), [NFC](../nfc-prototype.md).

## BLE

Test passkey pairing, a wrong passkey, an expired pairing window, bonded
reconnects, settings across restarts, an interrupted Apply or upload, and
range on and off the body. Unauthenticated links must be refused for every
protected read and write. A full bond table is a different failure from a
wrong passkey; keep them apart.

## NFC

1. Change the link from the app. Check the EEPROM write and readback, scan
   the QR code, and compare it byte for byte with the tag's URI, including
   path case. A redirect landing on the right page doesn't prove the bytes.
2. Read it powered, switched off, and with every power source removed. Use
   two different links and the full battery, magnet and carrier stack.
   Record range per phone.
3. In fresh unauthenticated RF sessions, try to change the NDEF, CC, lock bits
   and configuration with a Type 2 reader. The link must stay readable and
   unchanged. Then check that the MCU clears any extra RF lock bits.
4. Change the link again from the MCU. Reads and write protection must still
   work after a reset and after the tampering attempts.
5. Interrupt writes and move a phone in and out of the field. Errors must
   recover cleanly, and the QR and tag must agree after a restart. Measure
   field-detect timing against the [write sequence](../nfc.md#safe-writes).

Take the phone away between security tests so authentication can't carry
over. Use a spare tag for anything that might lock it for good, and never
record the password or PACK.

The optional two-session password check ([ADR 0014](../decisions/0014-positive-nfc-provisioning-proof.md))
is separate from normal sync and doesn't replace the write-rejection tests.
A transport error is inconclusive, not a rejection.
