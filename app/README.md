# Pixel Badge app

Android-first Flutter BLE client using [protocol v2](../docs/ble-protocol.md),
with basic configuration support for v1 badges. The Studio upgrade is locally
software-tested; its new features need a phone/DK bench run.
P3 adds an Audio page: Level/Spectrum controls, automatic audio setup over the existing secure Bluetooth bond, Record/Stop, encrypted private playback, deletion and explicit
WAV export. Android first; iOS recording is unavailable. Recording is disabled in
normal firmware until visibility/rail/security acceptance.
See [P3 bench steps](../docs/audio-prototype.md).

P4 adds Fluid and a Motion card on Badge: automatic/manual rotation, response,
particle amount, mounting axes and sensor status on Refresh. The loose sensor's
mounting axes must be checked against the panel; defaults are unverified.
Motion settings persist on the badge and work offline. These controls appear
only with the matching capability; older badges retain their existing controls.
See [P4 build and bench steps](../docs/motion-prototype.md). App 0.5.4/build 18
and matching recording-enabled DK firmware are deployed; physical P4 acceptance
is pending. Text opens directly into the composer; the separate Identity block
has been removed. The stored legacy name remains the empty-message fallback.

App-focused prompt: [flutter-app-prompt.md](../docs/flutter-app-prompt.md).

From the repository root:

Use Flutter 3.38.5 and the installed Android SDK (platform 36, Build Tools
36.1.0, Flutter 3.38.5's app NDK 28.2.13676358 and AGP 8.11's library-default
NDK 27.0.12077973). CI installs both NDKs. Dependencies are pinned in pubspec.lock;
restore with `flutter pub get` from app/ before the first build.

```powershell
.\scripts\app\dev.ps1 check
.\scripts\app\dev.ps1 test
.\scripts\app\dev.ps1 build
adb install -r app/build/app/outputs/flutter-apk/app-debug.apk
.\scripts\app\dev.ps1 run -Device <adb-device-id>
```

Build/run imports only four validated `BADGE_URL_*` entries from local root
`.env` through ignored `build/p2-app-links.json`. These are public contact links,
not secrets. No dotenv file is bundled. A missing `.env` generates four empty
defines; invalid or incomplete existing input still fails. Without defines, link presets are
disabled while display configuration still works.

Debug APK: `app/build/app/outputs/flutter-apk/app-debug.apk`, for bench use,
not store distribution. Android packaging only; iOS needs macOS/Xcode later.
Current source version: **0.5.4+18**, with system light/dark appearance and five
pages: Badge, Links, Text, Studio and Audio. Debug builds keep the display awake while
foreground and log only non-secret BLE status bytes. Release builds omit
assert-gated packet logging and debug screen-awake behavior.
No cloud service or update transport. Audio has no selectable timeout; phone
storage (16MiB reserve) or the classic WAV boundary (~37.28hours) bounds a file.
Private storage is40.32MB/hour plus a64-byte wrapped-key header; exported WAV is115.2MB/hour.
GitHub/LinkedIn QR/NFC agreement and LinkedIn off-state/restart retention passed
by owner report on the Nothing phone/DK prototype, corroborated by restarted-DK
status; [bench record](../validation/results/2026-10-12-dk-positive-nfc-proof/README.md).
Later settings and pairing checks are in the
[P2 result](../validation/results/2026-10-13-dk-p2-settings/README.md).
The earlier controlled-reset recovery failed firmware finalization; that remains
historical evidence, not an acceptance pass. Current firmware no longer requires
an RF-proof receipt or phone verifier. Apply link and Sync again request guarded
on-device policy/URL checks and only confirm after QR/NFC agreement and readback.
Keep the phone away from the tag during MCU updates. A reset preserves external
tag configuration, which firmware checks again automatically. New/unconfigured
tags use explicit wired setup, without phone taps; see
[ADR 0017](../docs/decisions/0017-local-nfc-synchronization.md).
Local synchronization does not establish RF password authentication or replace
physical unauthorized-write/Rev A acceptance tests.

## Playback and connection

Tap a recording's play button for play/pause, a seek slider, elapsed/total time,
and ten-second skips. Seeking reads authenticated encrypted blocks directly;
legacy recordings remain supported. Playback pauses on leaving the app or
losing audio focus. Short tails receive silent startup padding; writes/drains
fail after two seconds without progress. Export and Delete remain in the recording's menu.

Android keeps the bonded BLE connection when you switch apps, lock the phone,
or close its activity. A connected-device foreground service holds the process,
and the app-owned Flutter engine retains the GATT client across activity
recreation. Service leases are scoped to each link so old cleanup cannot stop
a replacement connection. The old GATT client closes before reconnecting.
Disconnect in Badge controls to end the connection. Force-stop,
Android stopping the process, Bluetooth loss or badge power-off still end it;
this does not promise reconnection after a killed process. Recording always
stops when the app leaves the foreground, even though BLE stays connected.

The main pages use concise controls and contextual help buttons. Connection,
verification errors and upload progress remain visible.

Library refresh skips malformed, empty and currently open recording files without
changing capture or session keys. Refresh is disabled during recording so directory
scans cannot compete with incoming packets on the native capture I/O executor.
Stop and detach share the same cleanup future; a replacement audio session waits
for the previous one to finish.
Returning to the app restores the idle audio subscription after cleanup so
button recording remains available. Returning never restarts a recording;
Stop cancels pending setup even across a quick background/foreground cycle.

CI explicitly sets up Android command-line tools before installing SDK packages.
It runs Dart checks and Android `testDebugUnitTest`, including native storage,
crypto, recording-library, connection and playback tests. For a clean checkout,
`flutter build apk --debug --config-only` generates the ignored Gradle wrapper
and local SDK configuration before running the Gradle task from `app/android`.

## Studio

- Save up to 32 custom short links on the phone; Apply changes the shared QR/NFC
  destination only after exact badge confirmation. Built-in presets remain
  optional. The QR limit is 20 alphanumeric characters or 14 byte-mode bytes;
  path case is preserved. Long URLs need a short redirect you control.
- Check NFC synchronization and request **Sync again** with the phone away from
  the tag. This rechecks the existing protected tag against the current QR link;
  it does not change credentials. No phone verifier is required.
- Compose 96 printable ASCII characters plus line breaks with case-sensitive
  text, auto fit, word wrap/pagination or scrolling, three scales, alignment
  and pace. Empty messages use the separately stored 32-character badge name.
- Edit a 32 × 32 monochrome canvas with pen, eraser, line, rectangle, flood fill,
  undo/redo, invert, flips and zoom. Add, duplicate, reorder or remove up to
  16 animation frames and preview playback before sending.

Custom links, the text draft and the full current animation save atomically in
Android app-private storage excluded from backup. They work offline and are
separate from the badge's confirmed settings. Loading from the badge asks before
replacing a different draft. Uninstalling the app deletes these phone drafts.
There is one current text draft and one current design, rather than a project
library. Pixel previews show content; hardware brightness and exact scan timing
still require physical validation.

**Send text/drawing/animation** first applies and confirms content, then applies
its display mode. These are two separate operations. On interruption, reconnect
and refresh: content may be saved while the old mode remains. The app never
replays an interrupted Apply automatically. The badge persists confirmed
content and runs without the phone, including after a power cycle.

v1 firmware keeps link/name/brightness/QR-text controls, with Studio sends and
NFC resync disabled. Update both app and firmware for v2. Old D2 apps cannot
decode v2. Android GATT caching may require Bluetooth to be toggled and services
rediscovered after flashing an image with added characteristics.

## Release signing

Debug APKs are for bench use. Release builds no longer fall back to a debug
key. Without local owner credentials, release artifacts remain unsigned.
For owner signing, create ignored `app/android/key.properties` with `storeFile`
(relative to `app/android/` or absolute), `storePassword`, `keyAlias` and
`keyPassword`, then run `flutter build appbundle --release` from `app/`.
Keep keystores/passwords outside tracked files. Store release, iOS packaging
and the Rev A single-core hardware port remain separate acceptance work.

See [Studio validation](../validation/results/2026-10-13-pixel-studio/README.md)
for software evidence and the short hardware checklist.

Launcher artwork: [icon source and generation prompt](assets/branding/README.md).
The Android package uses the mint LED badge icon, with adaptive masking and
five density exports. Regenerate sizes with scripts/app/export_icons.ps1.
