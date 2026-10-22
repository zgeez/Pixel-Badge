# Pixel Badge import and visual review - 2026-10-17

App-only follow-up on `app/prototype-polish`, based on the existing uncommitted
prototype. Firmware/protocol/partitions are unchanged by this follow-up.

## Findings and corrections

- The reported Glyph Museum Cat/Eye GIFs already render a spaced LED grid.
  Point sampling the rendered gaps while resizing produced an alias/checker
  pattern. A centered 25 x 25 logical-grid mode now extracts dot centers, with
  conservative recognition, an explicit source override and an inset control.
  Area filtering handles ordinary pictures. White-on-black previews separate
  the artwork from optional editing guides; solid previews avoid paint seams.
- The old import dialog was too tall and encoded the complete animation multiple
  times per animated build. Import is now a full-screen workspace with a fixed
  action, separate adjustment sections, bounded decoded frame sizes and serialized
  isolate conversion. Stale conversions cannot enable Use design.
- A text-speed slider expanded its dialog to nearly the entire viewport. It now
  has bounded content and a visible interval. Text/emoji input scrolls within its
  secondary dialog when the keyboard or larger text needs space.
- Studio action/media/brush panels now align their buttons; the camera icon for
  vertical flip is replaced. Motion loses its nested card and gains Done, with
  manual rotation chips fitting a normal phone row. The redundant Recordings
  shortcut is removed; playback has a direct play/pause control where needed.
- Dialog/menu spacing and shapes are consistent. Repeated QR-limit copy is
  reduced while retaining absolute web-link limits and expandable detail.

## Visual coverage

107 app-owned screenshots captured with real Roboto/Material fonts. Main pages,
menus and dialogs were inspected in light/dark themes. Source captures remain
in ignored `app/build/ui`; [capture inventory](capture-inventory.json) records
names. [Selected review sheet](visual-review.png) contains synthetic fixtures only.
Personal media is not redistributed.

| Area | Captured/inspected |
| --- | --- |
| Badge | Offline/connected workspace, display modes, brightness, Motion auto/manual |
| Links | Current/preset/custom rows, library, add/edit, QR limits, NFC details |
| Text | Composer/preview, layout/alignment/size menus, speed, library, save/rename/load |
| Studio | Canvas/timeline, brush size, Brush/Actions/Media panels, animation/playback, library |
| Import | Source/Tone/Place, image/glyph modes, threshold, placement, JSON, text/emoji, video |
| Audio | Offline/ready/recording/playback, empty/populated recordings, options/delete, help |
| General | Badge options, pairing help, About, licenses, narrow/large-text and keyboard cases |

Fixed main-page tests cover 320 x 640, 390 x 700 at 2x text and tablet layouts.
Import controls also cover 320 x 640 at 2x, including scrolling to each section's
lower controls. These secondary controls may scroll; main workspaces stay fixed.
Android file-picker/share UI is system-owned, outside this app-owned visual pass.

## Verification

- Flutter suite: **127 passed**, including two optional local-media cases;
  ordinary CI runs 125 without those private fixtures. Analysis: no issues.
- Synthetic regression: extraction preserves every logical dot's intensity;
  flat/native images are not falsely recognized; area sampling averages source
  stripes instead of selecting alternating gaps. Alpha/timing/codec/controller/
  persistence/layout regressions remain in the complete suite.
- Four cached GIFs: ordinary 25/97-frame media stays in Image mode; Eye (93 frames)
  and Cat (157 source frames, first 128 retained) detect 5% inset grids. Converted
  clips encode/decode identically and fit the existing artwork budget: Eye
  33000 bytes, Cat 125122 bytes. This verifies data/preview handling, not optical
  fidelity or maximum physical BLE transfer acceptance.
- Final pixel-background adjustment: all 8 visual tests passed again; the narrow
  section-bottom capture check passed. Dart formatting, repository C/C++/YAML/
  hygiene and `git diff --check` pass. Existing CRLF normalization warnings remain.
- APK debug build: Pixel Badge **0.9.0/build 25**. SHA-256:
  `1e2e41dda326feabe8113f27fc1c92fccad7696543bf9df9010b8b9d551a9f41`.
  Installed in place on the connected Nothing phone. Package version confirmed;
  saved draft/library hash matches before/after installation. No app data clear,
  badge flash, reset, tag provisioning or recording was initiated.
- Phone is locked at deployment verification. Interactive on-device walkthrough,
  bench verification of these revised imports, and actual sending of the revised
  Cat clip remain unverified. The widget screenshots include actual local media;
  they are not a substitute for the pending physical display check.

Reproduce the normal suite with `flutter test` from `app/`; capture with
`--dart-define=CAPTURE_UI=true` and `--dart-define=UI_FONTS=<Flutter material_fonts>`.
Optional local cached GIF checks use `--dart-define=GLYPH_REVIEW=<local directory>`
containing `source-0.gif` through `source-3.gif`. They are deliberately not test
assets. No new design decision; existing firmware optical/load and Rev A
port gates remain open. No commit or push.


## Grid follow-up - 0.9.1/build 26

I requested the grid always visible as before, retaining black/white coloring.
All previews (including imports and thumbnails) again show neutral dark-gray
unlit cells and grayscale/white lit cells. Import's grid toggle is removed.
Glyph extraction and pixel data are unchanged. [Updated preview](grid-restored.png)
uses synthetic fixtures.127 Flutter tests pass; analyzer, formatting and APK
build pass. The phone is absent from ADB and wireless discovery, so this APK is
built but not installed; the last confirmed installed version remains0.9.0+25.
APK SHA-256: `eac039c925746e8dd62f697196210d72dcaac88fe4c1b29c96dbba334aed3aac`.


## Selection-menu follow-up - 0.9.2/build 27

I requested better-looking selection menus. Replaced the small mixed dropdowns
with one controlled choice-sheet component for display mode, text layout,
alignment/size, brush size, playback and import source. Labeled sheets have
consistent spacing, icons, a subtle current-value highlight and explicit Close.
Long/large-text lists scroll without the selected row's ink escaping its viewport.
Overflow actions have matching icons, fine borders and no heavy shadow; Studio
tool panels have lighter fills and outlines. Segmented controls use the same
corner treatment. [Selected menu captures](selection-menus.png) use synthetic data.

129 local Flutter tests pass (127 without the two optional private-media cases),
including selecting a false-valued choice, cancel preservation, final-item
selection on a narrow2x-text sheet and the existing loaded-draft/BLE/layout tests.
Both themes and the revised menu captures were visually inspected. Analyzer,
Dart format, APK build, repository hygiene and diff checks pass. APK SHA-256:
`2cc8a948a9ae8627d363f760eec34d6b04a3433168b4195f1292bfdacd276609`.

0.9.2/build27 installed in place; package version and unchanged draft/library
hash verified against a fresh pre-install hash. Process is running with no fatal
or unhandled app log lines. Phone remains at its lock/ambient screen, so a live
interactive walkthrough remains pending. No firmware flash, badge reset,
provisioning, recording, commit or push. [Install summary](menus-install-summary.json).
