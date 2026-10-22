# Prototype cleanup and Pixel Badge library

Personal identifiers/URLs in excerpts are redacted; raw artifacts are unchanged.

2026-10-16. Software verification on `app/prototype-polish`, based on merged
PR26 (`39593e8`). Target remains the nRF5340 DK. No badge flash, phone install,
NFC provisioning, settings erase or hardware measurements were performed.

## Firmware review and corrections

The pass covered application orchestration and recovery, BLE staging/apply,
NVS loading/schema recovery, NFC URL transactions and transport deadlines,
audio capture/visibility/queues, motion sampling/fluid rendering, and display
and IMU driver boundaries. Screening applications remain intentional diagnostic
build targets rather than dead code. This is source review and software evidence,
not exhaustive fault injection or physical timing acceptance.

- BLE staging previously reserved a maximum artwork-sized buffer for each of
  eight fields. Packed independent field-sized buffers remove 14,194 bytes
  from that staging allocation; metadata also loses redundant name counters.
  Bounds/order/authentication and Apply generation checks remain enforced.
- Scalar/text/motion setting changes reuse the existing QR bitmap when the URL
  is unchanged. URL capacity checks no longer execute QR mask selection.
- Replacing artwork that no longer supports the active Drawing/Animation mode
  now immediately falls back to QR; unavailable content cannot leave a blank
  or static animation mode active.
- Display brightness submission failures now reach display recovery and the
  recording visibility result instead of being silently discarded. A physical
  brightness fault/REC visibility test has not been performed in this run.
- Web links support Version 1 alphanumeric/ASCII byte encoding and automatic
  H/Q/M/L correction. Limits are 25/17 including the scheme, with a full quiet
  zone. The scheme may be normalized; the path is never uppercased. The
  existing encoder uses a single segment; Micro QR and larger/cropped symbols
  are not emitted. See [wire details](../../../docs/ble-protocol.md#extended-qr-links-and-staging).
- URLs over 20 bytes use bounded default-MTU fragments. Info advertises the
  maximum; the app explains when older firmware needs updating.
- NFC provisioning mailbox v4 uses 28-byte terminated URL slots and a separate
  credential region at byte124. Host reads exclude input and credentials;
  RF helper proof remains v3. Old setup firmware is explicitly rejected by
  the new host tool. Normal NFC and QR retain one shared web link.

No measured nRF52840 resource or ISR budget is inferred from these builds.
The HUB75 driver remains specific to the DK; Rev A needs its own board/display
and power integration. Shared IMU/NFC polling configuration, bounded write
completion, recording release gate and watchdog behavior remain in place.

## App changes

Pixel Badge 0.6.0/build 19 removes disconnected Create/offline-draft/title-save
chrome, centers the ready card and automatically connects when a four-second
scan finds exactly one badge. Multiple badges still require selection.

Custom-link dialogs own their controllers until unmount, including reverse
route animation. Save reads the current fields. Removal Undo messages are
explicitly finite (four seconds), including the framework's action-snackbar
persistence behavior. URL capacity and correction are visible in the editor.

Text and Studio have named local snapshot libraries, up to 64 items combined.
Old single drafts migrate; snapshots copy their payloads. Android's atomic
private draft store has a bounded 512 KiB limit for the expanded document.

Studio uses the system file picker for PNG/JPEG/GIF/WebP/BMP import. Conversion
has fit/crop, threshold, dithering and invert controls and caches converted
frames while the preview clock advances. Limits: 8 MiB, 4096-square still images,
1024-square animation, first 16 frames with an explicit truncation message.
The badge uses one interval, so source timing is averaged/clamped 50..2000 ms;
users can edit the timeline before saving or sending. Picking/conversion does
not send content to the badge or upload it.

This follows the reusable creation/preview/library approach illustrated by
[Nothing's official Glyph Matrix example](https://github.com/Nothing-Developer-Programme/GlyphMatrix-Example-Project),
without importing its hardware SDK or artwork. QR sizing/capacity follows
[Denso's table](https://www.qrcode.com/en/about/versionPage/versionPage1_10.html).

## Verification

- Flutter full regression: 93 tests passed; final import-dialog regression also
  passed with the other 7 polish tests (94 distinct tests overall).
- Flutter analyzer: no issues. Dart formatting checked. Phone/tablet light/dark
  widget screenshots inspected with real Roboto and Material Icons loaded;
  captures are local under `app/build/ui/`. No actual phone render claim.
- Firmware native_sim: 20/20 configurations and 127/127 cases passed. The final
  targeted QR run passed 4/4 cases, including one new case that compares all four
  correction levels with generated reference bitmaps (128 distinct cases).
- Host Python: 17 NFC tests (ResourceWarning treated as error), 8 QR-config tests,
  and 3 app-define tests passed.
- Normal DK, explicit recording and NFC setup sysbuilds passed. Application
  linked FLASH/RAM: normal 213164/102896 B; recording 212852/102896 B;
  setup 85736/37987 B. These exclude separately linked bootloader/radio images,
  runtime high-water marks and any later nRF52840 build.
- Repository C/C++ formatting, YAML and hygiene checks passed; final whitespace
  checks include new source files. No new Android JVM tests were added or
  rerun; the debug APK compiles the changed native draft-store bound.
- Debug APK built; aapt confirms `[app package]`, 0.6.0/build 19,
  minSDK 24/targetSDK 36. SHA256:
  `CB501AFDC1DB560E2B0262374D41EA33E753D745473B8D4B28E18A1717360191`.

Local logs are ignored build artifacts: `build/polish-*-build.log`,
`build/polish-native-verified.log`, `build/polish-qr-verified.log`,
`build/polish-repo-check.log`, `build/polish-apk-final.log`, and
`app/build/polish-{tests,analyze,visual,import}-final.log`.

## Short bench checklist

Use matching firmware and APK after preserving the existing recovery artifacts.
An incremental build is not a settings-retention guarantee; this run does not
perform deployment or claim to diagnose prior retention loss.

1. With one badge nearby, Connect should go directly to it; with two badges,
   choose from the list. Verify first-pairing controls are unchanged.
2. Add/edit/delete a custom link; close the dialog normally and via Back.
   Undo should work and the message should disappear. Apply fitting 25-character
   uppercase and 17-byte case-sensitive links; compare QR, NFC and refreshed app,
   then power-cycle and repeat. Scan level-L links on both available phones.
3. Save two Text designs and an animation to Library. Load each, edit a copy,
   reopen the app and confirm saved snapshots are unchanged. Delete/Undo once.
4. Import a photo and a short GIF. Try Crop/Fit, threshold, dither/invert;
   review frames/timing, send and compare the physical panel. Cancel an import
   and confirm the existing design is retained.
5. Smoke-check brightness, Fluid, audio visualization, recording/Stop/playback,
   and background/reconnect. Full P3/P4 physical acceptance remains in their
   existing reports; this checklist does not replace it.

Brightness remains four global on-time presets 16/64/128/255, approximately
6/25/50/100 percent of configured maximum. Pixels are monochrome, not 100 shades.

No new unresolved product choice was introduced. Existing physical NFC/QR,
recording/security, iOS, mechanical/RF/power and Rev A port acceptance stay open.

## Requested installation completed (2026-10-16)

I subsequently requested installation of firmware and app. Pixel Badge
0.6.0/build 19 installed in place on the connected Nothing A069P over wireless
ADB; package-manager version verified, MainActivity launched and process found
running. No uninstall or app-storage clearing was performed.

The nRF5340 DK probe1050067477 was identified before programming. The matching
`build/p3-recording` images retain NFC, BLE, audio/recording and motion support;
default release recording policy is unchanged. Both HEX files were checked to
exclude settings storage at0xf8000..0xfffff. Before flash, a32KiB NVS backup was
saved under ignored `build/polish-install-20261016.dpapi`, encrypted using
Windows DPAPI CurrentUser and verified by decryption comparison in RAM.

Flashed network and application/MCUboot with J-Link9.82, explicit probe ID,
`--no-erase --no-rebuild`. Post-flash NVS SHA256 equals the pre-flash value;
zero previously programmed bytes changed and zero new bytes were programmed.
Matching-ELF public state confirms a loaded bond, retained brightness128 and
Fluid mode6, NFC result0/readytrue and CPU running. Public URL/name readback is
kept only in ignored local evidence. RTT captures IMU sample state0 and the
30-second healthy-boot/reset-counter clearing message. These observations
support retention for this installation; the prior restoration-loss cause
remains unestablished.

Local deployment evidence: `build/polish-install-20261016-{flash,rtt}.log`,
`build/polish-install-20261016-{backup,verify}.json`. No tag reconfiguration,
factory reset, recording initiation, commit or push. New library/import/long
QR functional phone checks and broader physical acceptance remain pending.

## Interface follow-up installed (0.6.1/build 20)

I requested removal of Sensor mounting and all homepage Create shortcuts,
accurate already-paired connection messaging, and URL errors lasting one second.
Removed the mounting controls and obsolete axis dropdown builder; applying the
remaining motion controls preserves stored axis bytes. Removed Create for
connected as well as disconnected home. Navigation remains available below.

Connection progress queries OS bond state as a presentation hint: bonded means
Reconnecting with no code instruction, unbonded means pairing guidance, unknown
means generic secure connection. Pairing and authenticated GATT reads remain
mandatory; the hint cannot authorize access or suppress security checks.

URL capacity warnings appear while editing an overlong link and clear one
second after the latest input. Save-time URL errors also expire; validation
still runs on every Save and rejects invalid values. URLs are not silently
truncated. Timer is cancelled when the dialog unmounts.

The 16-frame limit is unchanged: 128 bytes per 32-square monochrome frame plus a
4-byte header gives 2052 bytes. It bounds the current firmware buffers, transfer
and persistent artwork format, rather than the MCU's theoretical capacity or
phone storage. Frames loop continuously. No request to change that format
was inferred from the question about its rationale.

Verification: all 97 Flutter tests pass, including paired/unpaired progress,
URL expiry/invalid-Save/valid replacement and preserved motion mapping with no
Create/mounting controls. Analyzer, Dart format, repository formatting/YAML/
hygiene (442 files), whitespace and debug APK build pass. Firmware is unchanged
in this follow-up; no firmware rebuild or flash was necessary or performed.

APK installed in place on Nothing A069P over wireless ADB, opened and installed
package verified as 0.6.1/build 20. No uninstall, app-data clear, tag provisioning,
recording, commit or push. APK SHA256:
`D58180FE3FC5D791866A87301705C3CB7A79247856F94C6DE90B4372E8E4655F`.
Local logs: `app/build/polish-followup-{tests,analyze}.log`,
`build/polish-followup-{apk,repo-check}.log`. New phone interactions remain an
bench check; source/widget tests do not prove on-phone reconnection acceptance.
Existing loose-sensor physical alignment and broader hardware/iPhone gates
remain open. No new unresolved decision.
