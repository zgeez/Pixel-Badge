# Local NFC synchronization — 2026-10-15

Change: remove the NFC verifier requirement. Accepted
[ADR 0017](../../../docs/decisions/0017-local-nfc-synchronization.md) replaces the
runtime RF receipt/two-tap prerequisite with guarded local policy/NDEF checks.
This does not prove RF password authentication or close physical security gates.

Source: uncommitted `fw/prototype-motion`, base main `361935a`, worktree
`build/prototype-audio`. Target remains nRF5340 DK/HUB75/NTAG Click; no nRF52840
or Rev A claim. Tools are the existing NCS v3.4.1/toolchain 4f5b6ad6dd, pinned
Linux native_sim container, Flutter 3.38.5, installed Android SDK and J-Link 9.82.

## Software checks

- Four native configurations / 58 cases pass: NTAG/local session, URL update,
  BLE protocol and target errno. Final report: `build/p4-nfc-final/twister.json`,
  log `build/p4-nfc-native-final.log` (119.62 seconds).
- New local-session tests establish synchronization without any RF receipt,
  fresh-session restart without EEPROM/credential rewrite, UID/profile rejection
  before updates and recovery after a reader-field failure without a verifier.
  Public URL reads now attempt I²C lock release on completion or failure.
  Existing fault/deadline/rollback cases remain passing.
- Flutter 79 tests pass, analyzer clean (`app/build/p4-nfc-tests.log`,
  `app/build/p4-nfc-analyze.log`). Apply can attempt recovery with a false old
  status flag; success still requires exact URL and synchronized readback.
  A nominal success response without synchronized readback is rejected.
- NFC Python host tests: 16 pass with ResourceWarning treated as error.
  Wired configure never invokes PhoneProof; credentials are wiped after
  success, setup failure and finalization failure. Optional RF diagnostic tests
  still require actual accepted proof and never convert I/O errors into proof.
- Normal and recording-bench DK sysbuilds pass (`build/p4-nfc-normal-final.log`,
  `build/p4-nfc-recording-final.log`). Normal app linker use: 227148 B flash /
  117044 B RAM; recording bench: 226832 B / 117044 B. No stack timing claim.
- Fresh non-BLE wired-setup sysbuild passes (`build/p4-nfc-setup.log`), including
  command 6's explicit local finalization. It is not flashed.
- APK 0.5.2/build 16 built through the wrapper with local public-link defines;
  repository format/YAML/hygiene and whitespace checks pass.

## Deployment

`west flash --runner jlink --no-erase --no-rebuild` completed for the normal
image, programming network core then application/MCUboot with reset. No mass
erase, settings reset or wired credential setup was issued. Pixel Badge APK
installed in place on the connected Nothing phone; package metadata confirms
versionName 0.5.2 / versionCode 16 and its activity launched.

Local raw deployment evidence: `build/p4-nfc-flash.log`; APK build log
`app/build/p4-nfc-apk.log`. SHA-256:

```text
build/p3-normal/merged.hex
d41e16d227095afc92dc567045c4ab726d50381c6475f80856c7f2dad84c4f80
app/build/app/outputs/flutter-apk/app-debug.apk
2aabf873aea347cf9fe27c610d194a5aabd83e8693c4a96c98dfab8187a488e4
```

## Bench check still needed

Keep the phone away from the tag. Reconnect Pixel Badge, Refresh settings and
apply a different contact link (or Sync again if needed). Confirm success, scan
QR and tap NFC for the same destination, then check passive read while off and
retention after restart. A reader-field or policy error must remain a failure;
there is no helper prerequisite. No fresh physical taps, actual RF protection,
FD/EEPROM timing or interrupted-power tests were performed by automation.
Existing P3/P4 and Rev A hardware gates remain open. No commit or push.

## Subsequent sync-failure diagnosis

Manual observation: Sync again fails even away from the tag. RTT captures the normal
image's boot `NFC boot result -13` at 202 ms. This indicates local readable policy
rejection; the exact offending field has not been identified. Raw capture is
`build/p4-nfc-sync-error-rtt.log`. It does not establish a nearby reader cause or
an RF authentication result. The probe was resumed in cleanup; its library
emitted a destructor warning after capture, so no extra liveness claim is made.

Automatic approval review rejected flashing the wired setup image and running
configure because replacing persistent tag credentials/protection was not
explicitly authorized by the failure report. The command did not execute;
no setup flash, tag repair or credential change occurred in this follow-up.
Approval is pending. Normal firmware remains installed.

Independent app correction: policy error -13 now says setup needs repair,
missing-tag error -19 points to power/wiring, and the returned error is retained.
The app no longer treats all policy failures as phone-distance retries. New UI
and controller checks pass: 80 Flutter tests, clean analyzer and APK build.
Logs: `app/build/p4-nfc-repair-message-{tests,analyze,apk}.log`. Version 0.5.3/build
17 is installed/version verified and launched on the Nothing phone. This fixes
the explanation, not the unresolved tag configuration. No firmware source change
or new native/build run is claimed in this follow-up.

## Approved wired repair

I explicitly approved the repair. Existing wired-setup image flashed with
`--no-erase --no-rebuild`; configure generated transient credentials, completed
commands 3/6 and reported local policy/URL success. Normal firmware was restored
in finally, network core then application/MCUboot. No phone helper/taps or prompt.

Live public state from the matching normal ELF confirms `nfc_result=0` and
`nfc_ready=true`, CPU running. However, settings are defaults (Portfolio, BADGE,
brightness 128, QR) and no Bluetooth bond is loaded. The promised preservation
was not met. This is an unresolved restoration issue, not a preservation pass.
I was informed and asked to re-pair, retry Sync and reapply preferences.

Both image segment maps exclude settings storage at 0xf8000–0x100000; partition
and NVS CRC/sector settings match. No explicit mass erase or factory reset was
requested. The actual cause of lost state has not been established. No prior
NVS backup was available; stale public preferences were not guessed/restored.
The bond check read only key-type flags at a DWARF-confirmed offset, not key
material. Reads resumed the CPU immediately and confirmed it was running.

Local evidence: `build/p4-nfc-approved-setup-flash.log`,
`build/p4-nfc-approved-configure.log`, `build/p4-nfc-approved-normal-restore.log`,
`build/p4-nfc-approved-restore-rtt.log` and
`build/p4-nfc-approved-current-public-state.json`. The buffered RTT capture had
a probe-library destructor warning; subsequent public-state checks completed
with explicit cleanup and no such warning. No new source/build/test changes.
Physical app Sync/QR/NFC/passive checks and RF protection acceptance remain
pending. A local ready flag does not prove radio password authentication.

## App-sync result

After the requested Android forget/re-pair, physical Button 2 pairing, Refresh
settings and Sync again sequence, my manual notes record **Applied and confirmed /
Link synchronized**. This is a functional app-driven synchronization pass on
the repaired DK/Click with Pixel Badge 0.5.3/build 17 and normal firmware. It is
consistent with the earlier direct local result 0/ready state.

No fresh QR scan, NFC destination tap, passive power-off read or restart result
was supplied in this reply. These checks and RF protection acceptance remain
pending. The reported pass does not resolve the loss of prior settings/bond or
confirm preferred settings were reapplied. No new runtime source, build, test,
flash, install, commit or push in this recording step.
