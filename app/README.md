# Pixel Badge app

Android-first Flutter BLE client using [protocol v2](../docs/ble-protocol.md),
with basic configuration support for v1 badges. The Studio upgrade is locally
software-tested; its new features need a phone/DK bench run.
P3 adds an Audio page: Level/Spectrum controls, automatic audio setup over the existing secure Bluetooth bond, Record/Stop, encrypted private playback, deletion and explicit
WAV export. Android first; iOS recording is unavailable. Recording is disabled in
normal firmware until visibility/rail/security acceptance.
See [P3 bench steps](../docs/audio-prototype.md).

P4 adds Fluid and a Motion card on Badge: automatic/manual rotation, response,
particle amount and sensor status on Refresh. Sensor mounting controls have
been removed; applying these controls retains the existing sensor-axis mapping.
Physical alignment of the loose sensor remains a bench dependency.
Motion settings persist on the badge and work offline. These controls appear
only with the matching capability; older badges retain their existing controls.
See [P4 build and bench steps](../docs/motion-prototype.md). App 0.6.0/build 19
and matching recording-enabled DK firmware were deployed before this UI follow-up; physical P4 acceptance
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
  optional. The QR limit is 25 QR alphanumeric characters or 17 ASCII bytes on current firmware (20/14 on older firmware);
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

## Library and image import (0.6.0/build 19)

Text and Studio each have a Library: save a named snapshot, load it into the
editor, rename, or delete with a four-second Undo message. Up to 64 items share the
local library. Existing single-draft files migrate without losing text,
artwork or custom links. Drafts/library remain private to this installation;
uninstall deletes them. The app requests no cloud account or photo-library
permission; import uses the system file picker.

Studio can import PNG, JPEG, GIF, WebP or BMP supported by Flutter's codec.
Preview Fit/Crop, light threshold, dithering and inversion before replacing
the current editor. Files are limited to 8 MiB and 4096-square dimensions;
animated sources to 1024-square dimensions. At most the first 32 frames are
imported, visibly reported; individual source durations are clamped to the badge's
50..2000 ms interval. Edit speed/frames in the timeline, then Send normally.
No original image or audio is sent to a server.

Connect scans for four seconds: exactly one badge connects directly; multiple
badges require selection. It never chooses the strongest signal arbitrarily.
First-time pairing still requires the physical badge window and code.

Brightness is a global LED on-time setting, not per-pixel grayscale. Current
presets are 16/64/128/255 (about 6/25/50/100 percent of the configured maximum
on-time). Off/blanking is separate. Updated Studio firmware also supports 0..255 per-pixel intensity.

## Interface follow-up (0.6.1/build 20)

The homepage no longer contains a Create card, whether connected or offline.
Use the bottom navigation for Links, Text, Studio and Audio. Motion keeps
Auto rotate/manual rotation, Tilt response and Fluid amount; Sensor mounting
controls are removed without resetting the stored axis mapping.

Connection progress uses OS bond state only for wording: bonded devices show
Reconnecting, unbonded devices receive pairing-code guidance, and unknown state
uses a generic secure-connection message. Authenticated pairing/read checks
remain mandatory regardless of that hint. URL length/validation messages clear
after one second; invalid values remain unsavable and typed URLs are not
silently truncated.

## Grayscale Studio (0.7.0/build 21)

Studio holds up to 32 frames with individual durations and loop, once, reverse
or ping-pong playback. The badge accepts 8192 encoded bytes; binary designs
always fit (32 frames = 4100 bytes), while detailed gray animations may need
fewer frames. A visible counter shows the byte budget. Oversized projects stay
editable and saveable locally; sending rejects them before staging. Updating
firmware is required for gray or more than 16 frames. Existing designs survive.

Brush intensity is 0..255, with sizes 1/2/3, eraser, line, rectangle, ellipse,
fill, picker, rectangular selection/move/copy/cut/paste, undo/redo, rotate,
mirror/flip, and onion skin. Text and design libraries support Rename. Edits
persist automatically; explicit Save draft buttons are removed. Save to library
creates a named snapshot rather than replacing the current working draft.

Image/GIF conversion supports grayscale or threshold/dither, brightness,
contrast, inversion, fit/crop, zoom and positioning. Android video import uses
the system picker: up to 128 MiB, a selected clip up to 64 seconds, 5..20 requested
samples per second, and at most 32 samples spread across the selected clip.
Long clips are sampled more sparsely to retain duration, not imported at full FPS.
Import system-font text/emoji or the pixel font; paste a clipboard image when the
provider exposes an image URI. Import/export Pixel Badge JSON; square 20/25/32
pixel arrays are resampled independently. Export the selected frame as PNG.
No direct proprietary `.glyph` compatibility is claimed. Desktop image/data
workflows remain available; video/clipboard-image are Android-specific.

This is an independent Flutter implementation informed by the documented tools
of [GlyphMatrixEditor](https://github.com/pauwma/GlyphMatrixEditor) and
[Glyph Museum](https://app.glyphmuseum.com/editor?remix=6748); no source or assets
from those projects are copied. The DK grayscale scan requires optical/timing
bench checks; 256 encoded intensities are not a claim of calibrated optical steps.


## Fixed workspaces and 128-frame clips (0.8.0/build 22)

Badge, Links, Text, Studio and Audio now use fixed main layouts. Redundant page
headings and long card copy are removed. Link send actions sit beside each URL;
custom links and saved Text/design/recording libraries open separately and scroll.
Text layout/alignment/size/speed are compact controls. Studio uses an expanding
square canvas, brightness/size tools, a horizontal draggable frame strip and
Brush/Eraser/Select/Actions/Media groups. Secondary tools open focused panels.
Libraries retain Save/Rename/Load/Delete. Drafts still save automatically.

The updated badge accepts128 frames and131718 encoded bytes (128KiB of raw gray
pixels plus646 bytes of headers). Detailed full128-frame clips fit without relying
on compression. The existing binary and gray formats remain; duration/playback
are preserved. The byte counter and confirmed readback remain. Older badges keep
their advertised16/32-frame and2052/8192-byte bounds; update matching firmware to
use the larger store. Project/library bound is16MiB, at most64 snapshots.

Image/GIF and Android video conversion now sample/import up to128 frames. Video
still uses at most64 selected seconds and5..20 requested samples/second; longer
clips sample more sparsely when capped. Existing file/dimension limits remain.
Artwork transfer negotiates a larger MTU where supported and uses32-bit cursors.
Unchanged assets are cached after metadata checks for faster scalar refreshes;
new artwork is always read back in full. Recording cannot overlap uploads, and
local microphone visualization briefly pauses for flash writes. The updated app
and firmware must be installed together for the new capacity. Physical latency,
optics and Rev A resources remain bench/port work.


### Large-animation reconnect repair (0.8.2/build 24)

Negotiate the artwork MTU before initial settings readback. A large stored clip
can take time to download even after pairing completes; Loading settings now
shows read progress. The connection keeps its metadata/CRC checks and exact
post-Apply readback. Status punctuation and compact text sizes use plain labels.
The matching DK firmware schedules grayscale timer slots after row shifting to
avoid starving the main loop. See the2026-10-17 upload-recovery validation.


## Glyph imports and visual cleanup (0.9.0/build 25)

Image/GIF import opens a full-screen preview with Source, Tone and Place controls
and a fixed Use design action. Rendered 25 x 25 glyph grids are recognized
conservatively; Source can always switch between Image/photo and Glyph. Glyph
mode samples LED centers and preserves one source LED per badge pixel, centered
on the 32 x 32 display. Grid inset adjusts source borders. This logical size
follows the [official Nothing developer guide](https://github.com/Nothing-Developer-Programme/GlyphMatrix-Developer-Kit).
No proprietary project-file compatibility is implied.

Ordinary images use area filtering, preserving alpha and reducing sampling
aliases. Decoded frames retain at most 256 x 256 pixels without upscaling native
pixel art; animated codecs are explicitly resized when they ignore target size.
GIFs longer than 128 frames still import their first 128, clearly labeled; timing
clamps remain 50-2000 ms. Conversion and byte-budget encoding run off the UI
isolate, once per adjustment rather than on every preview tick. Rapid adjustments
invalidate earlier results; Use design waits for the current conversion.

Previews use white intensities on black. Owner follow-up in 0.9.1/build 26
restores the always-visible grid throughout previews and editing, using neutral
dark-gray unlit cells and white/grayscale lit cells. Import uses the same grid
without a visibility toggle; glyph sampling remains unchanged. Main pages remain fixed.
Studio tool panels use evenly sized buttons, vertical flip has a matching icon,
Motion has a focused dialog, and text speed opens a compact panel with its value.
The duplicate Audio library shortcut is removed; the main playback panel includes
play/pause without duplicating the recording row's control. Dialogs/popups share
consistent shapes and spacing. Large text uses accessible import tabs and controls.

See [visual review and checks](../validation/results/2026-10-17-app-visual-cleanup/README.md).


### Selection menus (0.9.2/build 27)

Display mode, text layout/alignment/size, brush size, playback and import source
share a compact trigger and a labeled choice sheet. A subtle rounded highlight
marks the current value; long lists scroll within the sheet at larger text sizes.
Close/cancel keeps the existing setting. Studio tool buttons have lighter fills
and fine outlines. Action menus use matching icons and a subtle border, with no
heavy shadow. Main pages stay fixed, and the restored black/white preview grid
remains visible. [Menu preview](../validation/results/2026-10-17-app-visual-cleanup/selection-menus.png).

### Editor controls and transfers (0.10.0/build 28)

All five bottom-navigation labels remain visible. Text size has a smaller,
unfilled trigger with separation from the text field. Focused text has left/right
caret arrows for precise movement without dragging a selection handle. Text layout
adds **Bounce lines**: explicit lines move back and forth independently when too
wide, with pauses at each end. Short lines retain alignment; extra rows page.
The speed control now matches the existing50..500ms wire limit.

Studio **Actions > New design** confirms replacing every frame with one blank
frame. **Clear frame** still clears only the selected frame. Saved library items
are retained. The lowest global brightness preset is now5% with matching firmware;
older connected firmware keeps its6% preset. Artwork pixel values remain0..255.

Matching firmware advertises the editor extension. Artwork readback uses one
sequential cursor setup instead of a cursor write before every page. Android
requests higher connection priority during transfer, then restores balanced
priority. Uploads still use acknowledged writes and complete verified readback;
no confirmation checks are skipped. A fresh bonded read of3551 bytes took786ms
at MTU247 on the connected Nothing phone. Larger clips still take proportionally
longer; staging-upload throughput was not measured in this check.

See [checks, deployment and limits](../validation/results/2026-10-17-editor-transfer/README.md).

### Synchronized bounce and expanded typing (0.10.1/build 29)

Bounce lines now reverses all scrolling lines together. Shorter overflowing lines
move more slowly; each end overshoots by3 pixels instead of pausing. Fitting lines
stay aligned. The preview and matching firmware share elapsed-time positioning
and nearest-pixel rounding. Opening the keyboard expands the text box into the
preview area, keeping layout controls and Send available; closing it restores the
preview without losing focus, selection or draft text. See
[validation](../validation/results/2026-10-17-bounce-lines/README.md).

### Large image imports (0.10.2/build 30)

Source photos are automatically resized rather than rejected above4096 pixels
or8MB. Image/GIF import requests a proportional preview of at most256 pixels
per side, then the existing Fit/Crop controls convert it to32x32 for the badge.
Small pixel art is not upscaled during decoding. Animated imports retain up to
128 frames and their timing; frames are explicitly resized when a codec ignores
the requested preview dimensions. Android's picker accepts image MIME types.
Files use an engine file buffer, avoiding an additional whole-source Dart copy.
Clipboard images stream to a temporary file which is deleted after decoding.

Supported formats still depend on the phone/image decoder; corrupt or unreadable
sources report an import error. There is no fixed source-dimension or8MB image
rejection, but decoding still needs available device resources. The badge's
existing frame and encoded-byte budgets are unchanged. See
[checks](../validation/results/2026-10-17-large-image-import/README.md).

### Bounded imports and editing (0.10.3/build 31)

GIF files use a streaming sampled decoder on a worker isolate, including large
logical canvases: 4096-entry LZW dictionary, at most two 256x256 RGBA working
canvases and up to128 retained preview frames (32MiB pixel data). File input is
buffered in64KiB chunks. Transparent patches, local palettes, interlacing and
restore-background/previous disposal are composed on that sampled canvas;
large sources use nearest-center samples. Timing and source frame count remain.
A256-million expanded-pixel work budget rejects decompression bombs. Other
animated formats above4 million source pixels request conversion to GIF before
native frame decoding. Still formats remain subject to platform decoder resources.
See the [GIF89a specification](https://www.w3.org/Graphics/GIF/spec-gif89a.txt).

Legacy firmware shows a saved Bounce selection as unavailable without discarding
the draft; another supported layout can be selected. Studio pointer updates check
only the edited frame for presence. Compression for the displayed storage budget
runs off-thread after the stroke; automatic saves resume after the stroke ends.
Undo still takes one snapshot per stroke. Matching firmware schedules both Scroll
and Bounce at the normal20ms render cadence, retaining100ms recovery retries.
