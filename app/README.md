# Pixel Badge app

Android-first Flutter BLE client using [protocol v2](../docs/ble-protocol.md),
with basic configuration support for v1 badges. The Studio upgrade is locally
software-tested; its new features need a phone/DK bench run.
App-focused prompt: [flutter-app-prompt.md](../docs/flutter-app-prompt.md).

From the repository root:

Use Flutter 3.38.5 and the installed Android SDK (platform 36, Build Tools
36.1.0 and Flutter's NDK 27.0.12077973). Dependencies are pinned in pubspec.lock;
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
disabled while identity/display configuration still works.

Debug APK: `app/build/app/outputs/flutter-apk/app-debug.apk`, for bench use,
not store distribution. Android packaging only; iOS needs macOS/Xcode later.
Current source version: **0.2.0+3**, with system light/dark appearance and four
pages: Badge, Links, Text and Studio. Debug builds keep the display awake while
foreground and log only non-secret BLE status bytes. Release builds omit
assert-gated packet logging and debug screen-awake behavior.
No audio, app-key enrollment, cloud service or update transport.
GitHub/LinkedIn QR/NFC agreement and LinkedIn off-state/restart retention passed
by owner report on the Nothing phone/DK prototype, corroborated by restarted-DK
status; [bench record](../validation/results/2026-10-12-dk-positive-nfc-proof/README.md).
Later settings and pairing checks are in the
[P2 result](../validation/results/2026-10-13-dk-p2-settings/README.md).
After its controlled reset, recovery passed both phone checks but failed
firmware finalization. The current normal image has defaults and NFC unverified;
full recovery, unauthorized ATT writes and Rev A acceptance remain pending.

Contact-link Apply requires the badge's NFC-verified status. If verification is
missing, Pixel Badge disables Apply link and explains the computer/phone
verification workflow; identity and display controls remain available.
A factory reset clears the verification receipt while leaving tag EEPROM.
Use the [BLE provisioning instructions](../docs/ble-protocol.md#restore-nfc-verification)
before testing a different contact link.

## Studio

- Save up to 32 custom short links on the phone; Apply changes the shared QR/NFC
  destination only after exact badge confirmation. Built-in presets remain
  optional. The QR limit is 20 alphanumeric characters or 14 byte-mode bytes;
  path case is preserved. Long URLs need a short redirect you control.
- Check NFC verification and request **Sync again** with the phone away from
  the tag. This rechecks the existing protected tag against the current QR link;
  it does not provision a tag or replace the computer/phone verification flow.
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
