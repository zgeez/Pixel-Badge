# 128-frame Studio and fixed workspaces

Date: 2026-10-16 owner session. Worktree `app/prototype-polish`, base `39593e8`
with uncommitted changes. This includes the earlier prototype-polish/grayscale
work and the owner-approved ADR0019 extension; no commit or push performed.

## Implementation

Pixel Badge 0.8.0/build 22 uses fixed Badge/Links/Text/Studio/Audio pages. Only
libraries, horizontal frame strips, text fields and focused settings/tool panels
scroll. Duplicate page headings and card copy are removed. Actions sit beside
URLs; Studio prioritizes the canvas with compact intensity, frame and tool rows.
Phone light/dark preference, editable libraries and automatic drafts remain.

The DK accepts 128 frames / 131718 encoded bytes, enough for 128 raw 1024-byte
L8 frames plus headers. Two 132 KiB raw banks replace 264 KiB of the unused
network-update reservation. MCUboot/application/update slots and settings at
0xf8000 remain unchanged. 24 KiB remains reserved; a complete network-update
reserve is no longer present. This is not the nRF52840 partition plan.

Mapped artwork views, a bounded playback cache and a flash worker replace large
RAM copies. Wide cursors/fragments cross 64 KiB; metadata/payload CRC and full
post-Apply readback validate transfers. Failed manifest acknowledgement pins both
banks until reload. Legacy inline/chunk artwork loads without a factory reset.
Recording/upload/NFC gates and PDM quiescence prevent overlapping flash work.

## Software verification

- Native Twister: **142 cases / 21 configurations passed**, no warnings.
  Includes maximum raw artwork, aligned storage, erase/write cuts, CRC corruption,
  legacy migrations and committed-manifest/lost-ack recovery. Test report:
  ignored `build/large-native-final/twister.json`.
- Flutter: **114 tests passed**; analyzer clean. Includes wide transfer/readback,
  IEEE CRC, old-badge limits, libraries, lifecycle/NFC/text recovery and layouts.
  Layouts: 390x844, 320x640, large type through 2x, tablet and keyboard; full
  128-frame timeline and every Studio tool panel exercised on a narrow phone.
  Real-font captures inspected in `app/build/ui/compact-*.png`.
- Android app unit tests: **21 passed**, storage/audio/library/playback/leases and
  128-frame media sampling. APK assembled successfully with configured presets.
- Normal, recording-enabled and NFC setup target builds passed. Application
  FLASH/RAM bytes: normal **206608/104497**, recording **206296/104497**,
  setup **85996/47235**. Separate radio **150384/38216**, MCUboot **40904/16904**.
  These are DK linker allocations, not peak stack or nRF52840 fit measurements.
- Repository C/Dart format, YAML/hygiene and `git diff --check` passed.

## Deployment

Probe 1050067477, nRF5340 DK with the existing HUB75/IMU/NFC/microphone setup.
Encrypted DPAPI backup of 0xb0000..0x100000 verified in memory before flashing.
Both HEX images checked to exclude that persistent span. Recording-enabled
firmware installed through J-Link with no erase. All 327680 persistent bytes
match the backup: zero previously programmed bytes changed and zero new bytes.
Public readback confirms name/mode/brightness/URL retained, bond loaded, NFC
ready/result 0 and CPU running. Healthy boot captured; no automatic tag setup,
factory reset, pairing reset or recording.

Pixel Badge 0.8.0/build 22 installed in place over wireless ADB on Nothing
A069P after the owner reconnected it. Package version and foreground process
verified; screenshot inspected and no fatal/unhandled/overflow errors in the
launch log. Automatic bonded reconnect produced a badge status read and the
connected dashboard was observed. App data retained by install -r. No recording
or content Apply was
automatically started. Recovery backup remains
local and encrypted; do not publish it or credentials. Older firmware rejects
manifest format 3; supported downgrade requires its matching previous backup.

## Artifacts

- `app/build/app/outputs/flutter-apk/app-debug.apk` SHA256 `99781f296816be901e697768569a858ee63c06632ff62d2854683fd5330dd6f0`
- `build/p3-recording/merged.hex` SHA256 `15b64483240cd37805690f216b274f93c94299e1eca3f18b494f89521f63ab6b`
- `build/p3-recording/merged_CPUNET.hex` SHA256 `66ed99eeebcf3ab93449c8ecf356aad6f13eaadaab4f7f87ddc11c158e871eec`

Ignored evidence: `build/large-*` build/check/flash/capture logs, encrypted backup
and state JSON. Public evidence includes [a filtered healthy-boot excerpt](healthy-boot-rtt.log),
not settings or credential data.

## Remaining owner bench checks

1. Install the matching APK, reconnect with the existing bond, Refresh. Visit all
   pages, edit a custom link, save/rename/load a library entry, and open the
   keyboard. Confirm controls fit and link actions/validation behave correctly.
2. Import or create more than 32 gray frames, Send animation, wait for confirmed
   readback, then reboot and verify retained timing/direction. Exercise 128 frames
   and retry/disconnect during upload; compare app and panel after reconnect.
3. Recheck gray ramps, QR scan, NFC taps and recording/playback. Measure actual
   flash latency, grayscale OE/LAT/CLK/refresh, ghosting and combined audio/BLE/
   motion behavior. Changed recording visualizer visibility still needs physical
   acceptance. Recording stays disabled in the default release configuration.

No hardware risk is closed by software checks. Rev A single-core code/controller,
RAM, scan timing and update/asset allocation remain port gates. No new owner
decision; existing OD22 carrier fit and OD60 iPhone redirect remain unrelated/open.
