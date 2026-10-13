# Prototype BLE configuration protocol v1

Status: implementation for the nRF5340 DK; owner-reported Android contact-link
agreement and power-cycle checks passed. Broader phone/security acceptance pending.
Android first. Audio/app-key enrollment is later ADR 0011 work; no recording
or firmware-update commands are exposed here. The badge is a peripheral/GATT
server; its network core runs IPC radio, application core runs host/GATT and
badge logic. No UART transport, credentials or personal identity in advertising.

UUIDs use the suffix `-6e9b-4f4c-a8f8-731a9d46e1c0` below.

| UUID prefix | Purpose | Properties | Encoding |
|---|---|---|---|
| f3b00001 | Service | — | Custom 128-bit UUID |
| f3b00002 | Info | Read | 8 bytes: version=1, URL max=20, name max=32, brightness count=4, modes=2, flags, reserved=0, reserved=0 |
| f3b00003 | Name | Read / Write | Read applied ASCII; writes use fragments below |
| f3b00004 | URL | Read / Write | Exact ASCII HTTP(S), 1–20 bytes, V1-M QR fit required |
| f3b00005 | Brightness | Read / Write | One byte: 16, 64, 128 or 255 |
| f3b00006 | Mode | Read / Write | QR=0, name=1; Apply also stores the boot default |
| f3b00007 | Apply | Write with response | `[1, request_id:u32le, field:u8]`, six bytes |
| f3b00008 | Status | Read / Notify | 12 bytes, layout below |

Info is public; all other reads, writes and CCC subscriptions require
authenticated LE Secure Connections, security level 4. Just Works and legacy
pairing are refused. New pairing requires an Action/Button 2 long press,
healthy display and a 60-second window. The six-digit random passkey is shown
in two rows of three digits, with leading zeroes, never logged. Bonded
reconnects do not require a new window. One connection and one bond slot;
no oldest-bond overwrite. Factory reset clears Bluetooth keys and the NFC
receipt with the settings partition; NFC then requires explicit provisioning.
The window is an implementation timeout, not a measured acceptance criterion.
Button 1 keeps mode cycling; short button actions are held during Apply or
passkey display.

## Stage and apply

Setting writes stage in the current connection; reads return applied state.
Name is 1–32 printable ASCII bytes. Each name write is
`[offset:u8, total:u8, ASCII...]`, with 1–18 data bytes. Offset 0 starts/replaces
the name; later chunks must be contiguous with the same total. Incomplete
names cannot be applied. Other writes contain raw values. Prepared writes,
nonzero ATT offsets and write-without-response are rejected. Write/status
packets fit ATT MTU 23; ordinary long reads handle names longer than 20 bytes.

Apply fields: URL=1, name=2, brightness=3, mode=4. Apply changes **one staged
field**, with no multi-field transaction. IDs are nonzero, strictly increasing
within a connection. Duplicate/older IDs are rejected without replay; read
Status to choose the next ID. Reconnect at ID exhaustion. Staging is consumed
on acceptance. Only one Apply is outstanding globally, including across reconnects.

The ATT response means acceptance, not completion. Work is queued; Bluetooth
callbacks never perform flash or I2C operations. Main owns settings and display.
URL uses P1 preview/guarded NFC/persistence/rollback; success requires QR
submission and tag agreement. Other fields submit to the display before saving,
and restore prior state on submission/storage failure. Mode reads describe
the active mode, including Button 1 changes. Stored settings work offline.

## Status

| Offset | Size | Meaning |
|---|---|---|
| 0 | 1 | Version=1 |
| 1 | 4 | Unsigned request ID, little endian |
| 5 | 1 | Field; 0 only while idle |
| 6 | 1 | Phase: idle=0, pending=1, complete=2 |
| 7 | 4 | Signed errno, little endian: pending=-115, success=0, failures negative |
| 11 | 1 | Flags: bit 0 writes available; bit 1 NFC synchronization verified; bit 2 pairing window; others zero |

The pending value is a fixed protocol constant, independent of the C library's
EINPROGRESS number (DK picolibc uses119; Linux host uses115). Firmware normalizes
this value; the app continues to reject a noncanonical pending packet.

Idle has ID=0, field=0, result=0. Representative errors: invalid=-22,
provisioning/access=-13, busy=-16, storage/transport=-5, timeout=-110,
cancelled-before-execution=-125. ATT rejects malformed/unauthenticated/unstaged
or busy requests before acceptance. Read Status if a notification is dropped.
Readback is published before completion. Match version, ID, field, complete
phase and result zero; then read the exact applied bytes. URL activation also
requires the NFC-verified flag. Normalize only the HTTP(S) scheme, never a path.

Disconnect discards staging/status. Queued work not yet begun is cancelled;
an already-running URL request may finish after disconnect. Its completion
cannot attach to a new session. The client marks this indeterminate and
refreshes after reconnect, **never automatically replays Apply**. Failed or
uncertain readback also requires Refresh before another Apply. Separate
settings/tag storage does not provide physical power-loss atomicity.

Golden Apply URL ID `12345678h`: `01 78 56 34 12 01`.
Completed URL ID 1, access denied, writes available:
`01 01 00 00 00 01 02 F3 FF FF FF 01`.

## Bench

Follow the [remaining P2 bench checklist](ble-bench.md) for settings,
persistence, connection interruption and security/reset checks.

```powershell
.\scripts\dev.ps1 build -BuildDir build/p2-ble -SysbuildConfFile firmware/configs/ble-sysbuild.conf
west flash -d build/p2-ble --runner jlink
.\scripts\app\dev.ps1 build
adb install -r app/build/app/outputs/flutter-apk/app-debug.apk
```

Sysbuild produces `merged.hex` for MCUboot/application and `merged_CPUNET.hex`
for the network radio. Use `west flash` with this build directory so its domain
flash order programs both cores; application-only flashing does not install
the radio. Keep the
provisioned NFC tag; a missing/invalid receipt needs the matching NFC verifier
workflow below (proof3, accepted ADR0014; completed DK proof is recorded below).
For that development operation the BLE build may additionally take
`-ExtraConfFile firmware/configs/nfc-provisioning.conf`. No BLE provisioning bypass.

Install `app/build/app/outputs/flutter-apk/app-debug.apk`. Wait for the boot QR,
hold Action/Button 2 for about one second, then release it. The QR remains
visible while the 60-second pairing window is open. Tap Find badge in Pixel
Badge, select Smart LED Badge, then enter the passkey in the phone dialog;
the two-row code appears when the phone starts pairing. Holding Button 2
through a reboot instead triggers factory reset and removes settings/bond/NFC
receipt. Verify URL readback,
scan QR and tap NFC, then repeat after disconnect/reboot. Test unpaired writes,
wrong passkey, pairing expiry, interrupted connection, dropped notification,
tag contention, long names, persisted brightness/mode and factory reset.
Software tests do not establish phone/radio/QR/NFC/security or RF-timing acceptance.

### Restore NFC verification

When NFC is unverified, contact-link Apply is unavailable. Name, brightness and
mode can still be configured. A badge factory reset clears the NFC receipt;
ordinary link-opening taps cannot restore it. Helper v0.5/proof3 uses the
accepted [ADR 0014](decisions/0014-positive-nfc-provisioning-proof.md) amendment.
Previously flashed proof2 firmware
and v0.4 cannot be used with the new host tools.
Keep the phone away from the tag while building, flashing and booting:

```powershell
.\scripts\dev.ps1 build -BuildDir build/p2-ble-provisioning -SysbuildConfFile firmware/configs/ble-sysbuild.conf -ExtraConfFile firmware/configs/nfc-provisioning.conf
west flash -d build/p2-ble-provisioning --runner jlink
.\scripts\nfc\build_android.ps1 -Jdk 'C:/Program Files/Android/Android Studio/jbr'
adb devices
adb -s PHONE_SERIAL install -r build/p1-nfc-helper/badge-nfc-helper.apk
$nfcPython = 'C:/ncs/toolchains/4f5b6ad6dd/opt/bin/python.exe'
& $nfcPython scripts/nfc/badge_nfc.py provision --elf build/p2-ble-provisioning/firmware/zephyr/zephyr.elf --android-serial PHONE_SERIAL
```

Replace PHONE_SERIAL with the connected Nothing phone from `adb devices`;
wireless ADB is supported. Keep the DK connected. The explicit ELF argument
must match this BLE/provisioning image. In the helper, authenticate and tap,
lift the phone fully away, then start check 2 and tap again. After **Both RF
checks passed** and the computer's **RF proof accepted**, remove the phone and
press Enter on the computer. Both checks authenticate the generated password
in separate sessions. A transport error or failed check cannot
complete verification. See [the NFC bench workflow](nfc-prototype.md#provision-once-then-check-the-status).

Once verified, restore the normal BLE image to turn off the provisioning
mailbox, then reconnect Pixel Badge and refresh before Apply:

```powershell
west flash -d build/p2-ble --runner jlink
```

Do not hold Button 2 during boot, which would erase the receipt again.

## Software verification (2026-10-12)

- Initial NCS 3.4.1 integration: DK default, BLE/provisioning and Linux
  CI-wrapper BLE builds passed. Latest status correction: normal BLE and BLE
  plus provisioning builds pass, generating both flash domains.
- Full native_sim baseline: fourteen suites / 77 cases. Latest BLE protocol
  check: two configurations / 14 cases pass, including simulated DK errno119;
  its canonical pending-packet regression failed before the correction.
- Flutter 3.38.5: 17 protocol/controller/screen tests and clean analyzer;
  debug APK build and v2 signature verification pass.
- Eight QR link configuration and two app define-whitelist tests pass;
  repository format/YAML/hygiene and whitespace checks pass.

These are local software results; unpublished CI changes have no remote result.
On 2026-10-13 the full current native suite passed15 configurations/84 cases;
Flutter17/analyzer, QR8/app-defines2 and NFC host12 also passed.
[Remaining-check record](../validation/results/2026-10-13-dk-p2-settings/README.md)
separates these results from the pending physical settings/security/recovery checks.
Later DK/phone work completed [proof3 provisioning and receipt retention](../validation/results/2026-10-12-dk-positive-nfc-proof/README.md).
Pixel Badge D2 (0.1.1/code2) and the pending-status fix are installed. Owner
reports Applied and confirmed/QR changed, matching GitHub and LinkedIn QR/NFC,
and correct LinkedIn NFC while off plus QR/app after restart. Read-only matching
ELF metadata confirms proof retained, ready/no error and desired=verified
HTTPS://ZIADG.COM/L after restart. These DK/Click/panel results do not establish
broader security, interrupted recovery, RF timing or Rev A acceptance.

References: [Zephyr GATT](https://docs.zephyrproject.org/latest/services/connectivity/bluetooth/api/gatt.html),
[Zephyr LE host](https://docs.zephyrproject.org/latest/services/connectivity/bluetooth/bluetooth-le-host.html),
[Universal BLE](https://pub.dev/packages/universal_ble), [NFC policy](nfc.md).
