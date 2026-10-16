# P4 motion software verification — 2026-10-15

Result: software checks pass; physical acceptance is pending. No flash, APK
installation, new recording or phone/DK bench test was performed in the initial
handoff. The app correction and installation below are subsequent evidence.
Some raw log timestamps are 2026-10-16 UTC (2026-10-15 America/New_York).

Source: uncommitted `fw/prototype-motion` worktree at `build/prototype-audio`,
based on merged PR25/main `361935a7cfaa7a55c0ee8cb020a982c6b4f4ba0b`.
The existing worktree/build directory names are retained for cache reuse.
Root owner hardware routing changes were not modified.

Target fixture: existing nRF5340 DK, HUB75 panel and ICM-42688-P breakout.
Owner reports that the IMU is loose/separate from the panel. The new right/down
axis mapping is not physically established. This is not an nRF52840/Rev A test.
Tools: NCS v3.4.1/toolchain 4f5b6ad6dd, Linux native_sim in the pinned CI
toolchain container, Flutter 3.38.5 and the installed Android SDK/JBR.

## Checks and method

| Check | Result | Local raw evidence |
|---|---|---|
| Native domain/driver regressions | 11 distinct configurations, 74 case executions passed | `build/p4-native/twister.json`, `build/p4-regression/twister.json` and corresponding logs |
| Final motion suite | 7/7 passed after startup/safe-reset correction | `build/p4-motion-final/twister.json`, `build/p4-motion-final.log` |
| Flutter | 77 tests passed | `app/build/p4-tests.log` |
| Analyzer | No issues | `app/build/p4-analyze.log` |
| Normal DK sysbuild | Passed, both cores/MCUboot | `build/p4-normal-final.log`, `build/p3-normal/` |
| Recording-bench DK sysbuild | Passed, both cores/MCUboot | `build/p4-recording-final.log`, `build/p3-recording/` |
| Debug APK with local public-link defines | Built; manifest 0.5.0/build 14 | `app/build/p4-apk-final.log`, APK below |
| Repository format/YAML/hygiene/whitespace | Passed | `build/p4-format.log`, `build/p4-check.log`, `git diff --check` |

Distinct native configurations are motion (7), settings (5), settings_store (3),
legacy modes (7), BLE protocol (11), BLE target errno (11), audio (4), audio
crypto (5), voice (5), URL update (8) and ICM42688 I²C driver (8). Motion appears
in more than one run and is counted once in the 74-case total. The final motion
run supersedes its earlier run. Android Kotlin code is unchanged; its existing
unit tests were not rerun in this task.

Motion cases cover all 24 axis maps and invalid values, gravity/dwell/flat/jerk/
diagonal handling, manual rotation, timer wrap and stale samples, gravity
direction, maximum-particle occupancy conservation over 10,000 steps, bounded
catch-up, QR entry lock/re-entry and safe restart. Settings/protocol checks cover
atomic motion payload/reload and independent motion/NFC-action staging. Flutter
checks cover old/new capabilities, validation, exact Apply/readback, status
parsing and motion controls. These are software assertions, not physical
measurements or owner-approved numerical acceptance criteria.

## Artifact and linker evidence

| Image | App FLASH | App RAM |
|---|---:|---:|
| Normal | 227356 B / 327168 B | 117044 B / 448 KiB |
| Recording bench | 227044 B / 327168 B | 117044 B / 448 KiB |

The network-core image uses 150384 B flash / 38216 B RAM; MCUboot uses 40888 B
flash / 16904 B RAM. These are separate linked images, not simultaneous RAM
totals, measured stack peaks or a single-core port forecast. Motion is enabled
in both application configurations. `APP_AUDIO_RECORDING_BENCH` is disabled
in normal firmware and enabled only in the bench configuration. Hardware stack
protection is enabled; runtime stack high-water marks remain unmeasured.

SHA-256 of the initial handoff artifacts (subsequent builds overwrite local
paths; these hashes describe the original versions):

```text
build/p3-normal/merged.hex
8c2b88669b812df08757c2629d1abed100df3b72633fe22052bbfa296aedf2ac
build/p3-recording/merged.hex
efeabb7ee4d708f4543c3bf76c7d231012300dd55796790154003a2f025edbc9
app/build/app/outputs/flutter-apk/app-debug.apk
dbecbeabc2d93037ee6fe99e8b87626512d4bbee83bb19761f7e34e40d28597f
```

The builds are incremental in already configured SDK directories; no pristine
build or hosted CI run is claimed. Logs and compiled artifacts remain ignored
locally. New sources are picked up by the existing CI discovery/build paths.

## Remaining acceptance

App follow-up: owner reports `double is not a subtype of bool` below Fluid
amount in installed 0.5.0/build 14. A full Badge-page test reproduces the exact
exception after scrolling, expanding Sensor mounting and navigating away/back.
The ExpansionTile lacked a PageStorageKey, so it read the parent ListView's
saved double scroll offset as its boolean expansion state. A dedicated tile
key fixes the failure and preserves expansion independently. The regression
passes after the change. All 78 Flutter tests, analyzer, APK build and repository
checks pass (`app/build/p4-mounting-{tests,analyze,apk}.log` and
`build/p4-mounting-check.log`). App 0.5.1/build 15 is installed/version verified
and launched on the connected Nothing phone; owner UI recheck is pending.
Firmware is unchanged and was not reflashed.

Follow [motion-prototype.md](../../../docs/motion-prototype.md): establish axes,
test fluid tilt/flip/response, four orientations and QR lock, sensor fault and
recovery, power-cycle persistence and BLE/NFC/audio regressions. Record actual
IRQ delivery and combined-load frame time, stack use and I²C contention.

Trigger registration in status does not prove interrupt delivery. Native_sim
stress execution does not measure DK frame time. No hardware risk is closed.
P3 recording visibility/security/endurance gates, Rev A integration and the
deferred nRF52840 port remain separate pending work.
