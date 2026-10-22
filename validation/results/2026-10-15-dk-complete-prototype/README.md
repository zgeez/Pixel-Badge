# Complete prototype deployment — 2026-10-15

I requested the current APK and firmware with all changes, and removal of
the app Identity block. Source remains uncommitted `fw/prototype-motion` in
`build/prototype-audio`, base main `361935a`. Target remains nRF5340 DK/HUB75/
NTAG Click/IMU/microphone, not the deferred nRF52840/Rev A port.

## Changes and checks

Pixel Badge 0.5.4/build 18 removes the separate Identity card, Apply name button
and unused editor/dirty-state synchronization. Text composer, Send text and
preview remain; empty messages use the confirmed stored legacy badge name.
Firmware name storage/protocol compatibility remains unchanged.

The current recording-enabled configuration includes NFC/BLE, local NFC
synchronization without a verifier, Studio/text/artwork/animation, audio
visualization, Record/Stop and seekable app playback, and P4 Fluid/orientation.
The recording fragment enables `APP_AUDIO_RECORDING_BENCH`; recording visibility,
authenticated control and failure stops remain in place. This deployment does
not change the default release recording gate or close physical acceptance.

- All 80 Flutter tests pass; analyzer clean. Existing UI cases now verify text
  sending without Identity, and confirmed-name fallback after Refresh. The send
  test dismisses the editing keyboard before scrolling/tapping the button.
- APK built with the repository wrapper and existing local public-link defines;
  manifest and installed package confirm versionName 0.5.4 / versionCode 18.
- Recording-enabled DK sysbuild passes; `.config` confirms NFC, BLE, audio,
  recording and motion enabled. No new firmware source changes were necessary.
  Existing native P4/NFC evidence remains in the preceding result directories;
  native/Kotlin tests were not rerun for this app UI/configuration deployment.
- Repository formatting/YAML/hygiene and whitespace checks pass.

Local logs: `app/build/p4-all-changes-tests.log`,
`app/build/p4-all-changes-analyze.log`, `app/build/p4-all-changes-apk.log`,
`build/p4-all-changes-firmware.log`, `build/p4-all-changes-check.log`.

## Deployment and retention evidence

Before flashing, read the 32 KiB settings area with automatic device unsecuring
disabled and resume the CPU after reads. A Windows DPAPI CurrentUser-encrypted
backup is stored only at ignored `build/p4-all-changes-nvs.dpapi`; decryption
round-trip/hash was checked before deployment. No plaintext backup or key
material was printed or added to tracked files.

Checked both HEX images exclude settings storage. Flashed `build/p3-recording`
using J-Link 9.82, `--no-erase --no-rebuild`, network then application/MCUboot.
Installed the APK in place and opened MainActivity. Local flash evidence is
`build/p4-all-changes-flash.log`.

Before/after matching-ELF public snapshots show the same URL, name, brightness
128, stored mode 4 and a loaded bond. NFC result remains 0/ready, CPU running.
The raw NVS hash changed; a follow-up byte comparison against the encrypted
backup found **zero changes to previously programmed bytes** and **82 newly
programmed bytes**. This supports retention for this update, not a diagnosis
of the earlier setup/normal restoration loss. No bond key material was exposed;
bond presence uses only the DWARF-confirmed key-type flags.

Local non-secret evidence: `build/p4-all-changes-before.json`,
`build/p4-all-changes-after.json`, `build/p4-all-changes-nvs-comparison.json`.
These compiled/deployment checks do not establish new recording endurance,
REC visibility, motion timing, physical QR/NFC taps or Rev A acceptance.
No recording was initiated by automation. No NFC reconfiguration, factory
reset, commit or push. Existing hardware/port gates remain open.

## Brief bench check

Reconnect and Refresh settings. Text should open directly into the composer;
send a message. Audio should offer Record/Stop: record briefly, stop and play
back. Check Fluid and Sync again remain available. Report any failure rather
than treating installed capability as physical acceptance.
