# Pixel Badge app

An Android-first Flutter companion for the badge. It configures the display,
shares a QR/NFC contact link, edits artwork, and handles phone recordings.
It uses [BLE protocol v2](../docs/ble-protocol.md), with basic v1 compatibility.
iOS packaging and recording support are not implemented.

## Features

| Page | Controls |
|---|---|
| Badge | Connect, brightness, mode, display rotation, and motion settings |
| Links | Saved short URLs; apply a shared QR/NFC destination and check synchronization |
| Text | Compose, preview, align, scroll, or bounce lines; save local drafts |
| Studio | Draw on a 32 × 32 grayscale canvas; edit animation and import images/GIFs/video |
| Audio | Level/spectrum modes, Record/Stop, private playback, delete, and WAV export |

[Studio screenshot](../docs/images/app-studio.png)

Drafts save automatically on the phone. Saved libraries are separate from the
badge's confirmed content. Current firmware accepts up to 128 frames and
131,718 encoded bytes; older badges use their advertised limits. Image/GIF
conversion runs off the UI thread. Android video imports sample up to 128
frames from a selected clip of up to 64 seconds.

The editor was informed by the documented tools of
[GlyphMatrixEditor](https://github.com/pauwma/GlyphMatrixEditor) and
[Glyph Museum](https://app.glyphmuseum.com/editor?remix=6748).
Their source and assets are not included. Glyph image sampling supports rendered
grids; proprietary project formats are not supported.

## Connection and recording

Hold the badge's Action button to open pairing, then enter its displayed code
in the phone's Bluetooth dialog. The app waits for operation completion and
exact readback before showing changes as applied. After an interrupted upload,
Refresh before retrying; commands are not replayed automatically.

Android retains the BLE connection across activity changes through a connected
device service. Disconnect ends it explicitly; force-stop, process termination,
or radio loss can also end it.

Recording requires a matching firmware build with the development recording
option enabled. It stops when the app leaves the foreground, the link is lost,
or the badge can no longer show its recording view. Private recordings are
encrypted with keys wrapped by Android Keystore and excluded from backups.
WAV export is explicit. See [audio behavior and tests](../docs/audio-prototype.md).

## Development

Use Flutter **3.38.5**, Android SDK platform **36**, Build Tools **36.1.0**, and
NDKs **28.2.13676358** (app) and **27.0.12077973** (libraries). Dependencies are
pinned in `pubspec.lock`.

From the repository root:

```powershell
Push-Location app
flutter pub get
Pop-Location
.\scripts\app\dev.ps1 check
.\scripts\app\dev.ps1 test
.\scripts\app\dev.ps1 build
adb install -r app/build/app/outputs/flutter-apk/app-debug.apk
.\scripts\app\dev.ps1 run -Device <adb-device-id>
```

Build/run reads only the four public `BADGE_URL_*` entries from an optional
root `.env` (see [example](../.env.example)). No dotenv file is bundled.
Without these defines, presets are disabled and manual configuration works.
Use short URLs that fit the badge's QR capacity; path case is preserved.

The debug APK is for development. CI also runs Android `testDebugUnitTest` for
storage, crypto, connection, and playback logic. On a clean checkout, run
`flutter build apk --debug --config-only` from `app/` before invoking Gradle
directly so Flutter creates its local SDK and wrapper files.

See [validation](../docs/validation.md) for phone/DK evidence and remaining
physical checks. Software tests do not establish Rev A behavior.
