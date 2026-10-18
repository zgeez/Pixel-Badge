# Studio grayscale and animation extension

Date: 2026-10-16. Base revision `39593e8`, dirty `app/prototype-polish` worktree.
Source is the uncommitted coordinated update; these results are not a merge or
Rev A hardware release. See [ADR0018](../../../docs/decisions/0018-grayscale-studio-recording-view.md).

## Method and equipment

Windows host, NCS v3.4.1 / toolchain 4f5b6ad6dd, Flutter 3.38.5, installed Android
SDK36, J-Link9.82, nRF5340 DK probe1050067477 with the existing HUB75/IMU/NFC/PDM
bench connections. Nothing A069P on wireless ADB. No claim about its measured
supply, panel optical response, microphone calibration or Rev A peripherals.
The physical nRF52840 Rev A port has not been performed.

## Software results

- PASS: 135 firmware native cases across 20 configurations, no failures/errors.
  Includes maximum 8192-byte banked artwork, flash-write interruption at selected
  byte positions, 16 repeated replacements/GC, schema recovery across 269 byte
  positions, malformed gray frames, raw/RLE/XOR fixtures, 32-frame BLE staging,
  individual timing/playback, recording-view pixels and all plane/brightness
  timer bounds. Native flash simulation is not physical power-cut evidence.
- PASS: 103 Flutter tests, analyzer and Dart formatting. Includes exact gray
  values, delta fixtures/truncation, oversized local projects with send rejection,
  library rename retaining IDs/bytes, editing/undo, raster adjustments/timing,
  legacy capability handling and existing audio/lifecycle/regression tests.
- PASS: 21 Android JVM tests, `:app:testDebugUnitTest`; APK compiles the native
  video/clipboard/export handler. Media sampling bounds and ARGB/RGBA channels
  tested. Unqualified Gradle plugin tests encounter a Windows cross-drive plugin
  path issue; the app-qualified task passes. This does not change CI's Linux task.
- PASS: normal, recording and NFC setup DK sysbuilds; development signing unchanged.
- PASS: C/C++ format, strict YAML, repository hygiene and `git diff --check`.
- PASS: narrow dark/light and tablet UI tests/captures with real fonts; inspected
  Studio and Library captures. No optical comparison to the physical panel.

| Application configuration | Flash bytes | RAM bytes |
|---|---:|---:|
| Normal | 236008 | 163705 |
| Recording bench | 235676 | 163705 |
| NFC setup | 100348 | 86571 |

The radio image separately uses 150384 flash / 38216 RAM bytes; MCUboot uses
40888 / 16904. Do not add these into an nRF52840 fit claim: its controller and
matrix backend differ. Target main stack remains 16 KiB; larger stacks in native
unit configurations only accommodate their local test fixtures.

## Installation

Verified both recording HEX files exclude the unchanged 0xf8000..0xfffff NVS
partition. Made a CurrentUser-DPAPI encrypted 32 KiB backup and verified its
in-memory decryption. No plaintext settings/key dump written. Flashed both cores
with `west flash --runner jlink --no-rebuild --dev-id 1050067477 --no-erase`.
No tag setup, erase, bond reset or recording started. Readback found zero changed
previously programmed NVS bytes and 23 newly programmed bytes; existing URL,
name, brightness and mode retained, NFC ready/result0 and BLE bond loaded.
Installed APK in place, verified version0.7.0/build21, launched MainActivity and
confirmed its process. No recent AndroidRuntime/Flutter error in the narrow log
capture. The initial RTT buffer contained startup and dropped later output;
a fresh active capture after a controlled reset confirms healthy boot at30.135s,
BLE advertising and authenticated level4 reconnect. See the filtered non-secret
[RTT excerpt](healthy-boot-rtt.log). CPU left running.

## Artifacts and raw data

Local ignored `build/gray-*` logs: native-final, flutter-tests, app-analyze,
android-tests, normal/recording/setup-build, hygiene, flash, apk-install,
backup/verify, healthy capture. Test reports: `build/gray-native-final/twister.json`
and `app/build/app/test-results/testDebugUnitTest/`. UI PNGs: `app/build/ui/`.
Encrypted recovery backup and public-state JSON remain local ignored artifacts.
Do not publish settings backups or credentials.

- `app/build/app/outputs/flutter-apk/app-debug.apk` SHA256 `e3e90023db71077f6c5e5214981392f794c3427f05aa5c091ab9a289ae2a92b7`
- `build/p3-recording/merged.hex` SHA256 `31e2eb813d4d173b93a33281cdda05bfd4fbe339f65d881345eb6c3efc1bc4bd`
- `build/p3-recording/merged_CPUNET.hex` SHA256 `66ed99eeebcf3ab93449c8ecf356aad6f13eaadaab4f7f87ddc11c158e871eec`

## Remaining bench checks (not passed by software tests)

1. Reconnect/Refresh with the existing bond. Paint a grayscale ramp, Send drawing;
   check dark/mid/bright shades. Import a short GIF, adjust conversion, Send
   animation; check per-frame timing/direction and retained playback after reboot.
   Save/rename/load a Text and an Animation library item and reopen the app.
2. Record 10 seconds and Stop. Confirm full spectrum plus the two-row level meter,
   fixed lit corners in silence and brightness255 without REC lettering; playback
   still seeks/plays normally. Repeat Home/lock/link/display stops. No recording
   was automatically started for this installation.
3. Scope OE/LAT/CLK and measure actual refresh/low pulses and ghosting under
   audio/BLE/motion load. Recheck QR scanning and visualizer visibility at wear
   distances and lighting. Encoded 256 values are not 256 calibrated optical steps.
4. Test real phone media providers/video trim/export and large artwork transfer
   under combined load. Resource limits: 32 frames, 8192 encoded badge bytes;
   complex local projects can exceed the transfer budget and remain saveable.

Release recording remains disabled by default. New-view visibility, grayscale
optics, power and Rev A resources are open acceptance, not owner decisions left
unanswered. No new unresolved owner decision; existing OD22/OD60 unchanged.
