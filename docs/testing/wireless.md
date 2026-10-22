# BLE and NFC tests

Record firmware/app versions, phone/reader versions, exact actions, and results.
Use the [BLE checks](../ble-bench.md) and [NFC setup](../nfc-prototype.md).

## 5. BLE

Test authenticated LESC pairing, wrong passkey, closed/expired pairing window,
bonded reconnect, settings persistence, interrupted Apply/upload, and worn/unworn
range. Protected configuration/audio must reject unauthenticated links.
Distinguish full bond-table rejection from passkey rejection.

## 6. NFC

1. Apply a new URL through the app/MCU path. Verify EEPROM completion/readback,
   decode QR, and compare the actual NDEF URI, including prefix expansion and
   path case. A browser redirect alone does not prove stored bytes.
2. Repeat powered, switched off, and with every power source removed. Use two
   different URLs and the complete battery/magnet/carrier stack. Record range
   and results per phone.
3. In fresh unauthenticated RF sessions, attempt NDEF, CC, lock-bit, and
   configuration changes using a suitable Type 2 reader. Without powering the
   MCU, confirm the exact URL stays publicly readable and its interpretation
   cannot change. Then check MCU detection/clearing of extra RF lock bits.
4. Make another authorized MCU update. Public powered/passive reads and RF-write
   protection must still work after reset and attempted tampering.
5. Interrupt writes and introduce/remove RF fields. Check errors, bounded
   recovery, persisted settings, and QR/NDEF agreement after restart. Measure
   FD/SCL timing against the [NXP sequence](../nfc.md#provisioning-and-recovery-gate).

Remove/reapply the RF field between security tests; prior authentication must
not carry over. Use a sacrificial tag for tests that could lock it permanently.
Do not record PWD/PACK or credential-bearing dumps.

Optional generated-password authentication uses two positive PWD/PACK sessions
([ADR 0014](../decisions/0014-positive-nfc-provisioning-proof.md)). It is separate
from runtime local synchronization and does not replace RF-write rejection
tests. Transport errors are inconclusive, not proof of password rejection.
Read-range and phone-set pass limits remain to be agreed.
