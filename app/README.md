# Pixel Badge app

Android-first Flutter BLE client using [protocol v1](../docs/ble-protocol.md).
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
Current bench build: 0.1.1+2, with **BLE diagnostic D2** beneath Pixel Badge in
the app bar. Debug builds keep the display awake while the app is foreground
and log only non-secret BLE status bytes for this diagnosis. Release builds
omit the label, assert-gated packet logging and debug screen-awake behavior.
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

Launcher artwork: [icon source and generation prompt](assets/branding/README.md).
The Android package uses the mint LED badge icon, with adaptive masking and
five density exports. Regenerate sizes with scripts/app/export_icons.ps1.
