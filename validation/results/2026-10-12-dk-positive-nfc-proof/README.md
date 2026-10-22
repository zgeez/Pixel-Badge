# DK positive NFC provisioning proof — 2026-10-12

Personal identifiers/URLs in excerpts are redacted; raw artifacts are unchanged.

Result: **provisioning completed; verification retained after restoring normal
BLE firmware; GitHub/LinkedIn QR and NFC agreement and LinkedIn power-off/restart
retention passed by manual observation**. Broader P1/P2 acceptance remains pending.

Hardware: nRF5340 DK, NFC Tag 2 Click and HUB75 panel. My Nothing 4a Pro
reports model A069P / Android 16 through ADB. Helper installed version0.5/code5;
proof/mailbox/receipt version3 per accepted ADR0014. Source is main11cfc23 plus
uncommitted local development on fw/prototype-ble, not a published release.

After an earlier session expired without a challenge fetch, I unlocked
the phone and the host started a fresh one-use session. Reported both
checks passed and phone removed. Direct host output then showed:

```text
RF proof accepted. Remove both phones from the tag, then press Enter to finish the guarded I2C check.
Sending final confirmation to the DK...
NFC provisioning verified; no irreversible configuration locks set.
```

These are transcribed tool outputs, not a complete archived raw host capture.
Confirmation was sent only after host acceptance and manual field-removal report.
Cached non-secret status before normal restore:

```json
{"result": 0, "ready": true, "address": 2, "desired": "[redacted URL]", "verified": "[redacted URL]"}
```

After flashing both normal BLE domains, matching-ELF read-only control metadata
shows prooftrue, ready_state1, last_result0 and operation_active0. This confirms
receipt loading/clean verification across firmware restoration. Normal boot
reached its30-second healthy-counter clear; [RTT excerpt](normal-boot-rtt.log).
Helper closed and Pixel Badge brought to foreground for URL Apply testing.

Artifact SHA256:

| Artifact | SHA256 |
|---|---|
| Normal BLE application ELF | 1D811C6309D5DA75B6F8F731CF3B89A2EE1334B1BB499AE00646C43E95A8D71F |
| Provisioning application ELF | 102293E3F48A85D10E788CAD7DB15C3B3E39D971B1C397A8EDF24D03242A8B81 |
| NFC helper APK | FAAEF7FA3E1CB2EE235094DB72A1B97CF46668445F98456A21EC6E0D08A0183F |
| Pixel Badge APK | 231802FA7FDA7C1554DA4BCAC7B9CE31CA5A803E018B7647FE11BDE38B6291BA |

Limits: field removal is manually reported, with Android removal gating; no RF gap
or FD latency measurement. This method establishes two positive generated-PWD/
PACK sessions under NXP's component model, not directly observed factory-PWD
rejection. No credentials/PACK dumps, bearer nonce or BLE passkey recorded.
Unauthorized RF-write/metadata/bypass tests, interrupted recovery, phone security
rejection and full P2 acceptance remain pending. Original watchdog trigger remains unproven. R023
and other hardware/security risks stay open.

## Later BLE status correction

The DK's picolibc EINPROGRESS is119 while protocol v1 pending is-115.
Firmware previously serialized-119, which the app rejects as malformed.
The SDK ARM object confirms that value; Linux-only tests use115 and missed
the mismatch. A simulated-target-errno variant fails the existing packet test
before correction. Firmware now uses a fixed wire constant; both variants
pass14 cases and the17 app tests pass. Both BLE builds/checks pass.

Corrected normal ELF SHA256:
5836A2E2C5F2A02EB9C02733D062F46E4E3E29649978FFD6D0CCCA5709C30A96.
Installed D2APK0.1.1/code2 SHA256:
2B73B7286318EA336BF95E2CA93229C873BA73D32AE6DC8035334D209AD102CA.
Android installed base.apk hash matched the local artifact; I see D2.

After correction, manual observation: Applied and confirmed with QR changed. Direct
NFC runtime desired=verified [redacted URL] with prooftrue/ready1/result0.
Last sampled completed request id1/field4/result0 was a mode request, so it is
not evidence of the earlier reported URL command's ID. No raw pending RF/ATT
packet was captured.

## Contact-link and power-cycle checks

On the corrected normal firmware and installed D2 app above, I used
the Nothing phone to scan the QR and tap NFC for GitHub, then selected LinkedIn
in Pixel Badge, applied it and repeated both checks. Manual observation: both QR
scans and NFC taps opened the correct selected destination.

With LinkedIn selected, I powered the DK off with both buttons released,
tapped NFC, removed the phone from the tag, restored power and checked the QR
and refreshed app link. Manual observation: off-state NFC and restored QR/app were
correct. This passes the two-destination agreement and LinkedIn persistence
checks for this DK/Click/panel setup; supply details and a raw decoded QR/NDEF
phone trace were not captured. It does not establish Rev A enclosure/coil or
all-link/all-phone acceptance.

After the reported restart, read-only J-Link access with the matching corrected
ELF confirmed:

```text
NFC proof True
NFC ready_state 1
NFC last_result 0
NFC operation_active 0
NFC public URL desired [redacted URL]
NFC public URL verified [redacted URL]
```

These are transcribed non-secret tool outputs. No reset, halt, flash, setting
write or reprovisioning was performed during this read. Physical wrong-passkey/
unauthenticated BLE rejection, unauthorized RF writes, interrupted recovery,
remaining settings/reset checks and FD timing remain separate acceptance gates.
