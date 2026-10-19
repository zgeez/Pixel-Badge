# Automatic large-image import - 2026-10-17

Pixel Badge0.10.2/build30. App-only change; firmware, BLE protocol, artwork format,
frame count and badge storage budgets are unchanged.

The former4096-pixel still-image,1024-pixel animated-image and8MB encoded-source
rejections are removed. The decoder is requested to produce a proportional image
with longest side at most256, without upscaling pixel art. Every retained animated
frame is explicitly resized if its codec ignores that request. Up to128 frames
are retained with original timing; existing Fit/Crop/grayscale controls convert
the result to32x32 and enforce the connected badge's encoded/frame budgets.

Picked files use an engine file buffer instead of copying the entire source
through Dart. Android's image MIME filter accepts image sources generally.
Clipboard URI data streams to a unique cache file rather than a platform-channel
byte array; Dart deletes the temporary file after decoding, including failures.
Native stream-copy failures delete partial files. Unsupported/corrupt sources
still fail with a clear error. No universal format or unlimited decoder-memory
claim is made: native decoding still depends on the platform and its resources.

## Verification

- 136 full Flutter cases pass, including six import cases. Regression fixtures
  include6000x3000 and100x7000 PNGs, a5000-pixel source with more than8MiB of
  valid PNG metadata, missing/corrupt file failures and a two-frame5000x100 GIF.
  Checks cover bounded retained dimensions, aspect ratio, durations and successful
  encoding/decoding of a badge-ready design. Fixtures are generated in tests;
  no owner media is copied into the repository.
- Analyzer and Dart formatting pass. Android `:app:testDebugUnitTest` passes21
  cases in seven suites, compiling the modified native import handler. The
  correctly quoted `-Pkotlin.incremental=false` option avoids Windows cross-drive
  incremental caches; an earlier unscoped task hit an unrelated plugin test-root
  issue. Existing deprecation warnings remain.
- Debug APK builds with the existing whitelisted local contact presets.
- Repository formatting, YAML, hygiene and whitespace checks pass.

Ignored logs: large-image-tests.log, large-image-all-tests.log,
large-image-analyze.log, large-image-android-final.log, large-image-apk.log and
large-image-hygiene.log. Android XML is under
app/build/app/test-results/testDebugUnitTest/.

## Installation and limits

Installed in place on the connected Nothing phone. Package version0.10.2/code30
and a fresh unchanged draft/library hash were verified. No badge content, setting,
bond or firmware was changed. [Installation summary](installation.json).
APK SHA-256: `227ed795d4f00f091309debb9cb2d025b5ede74b57f67158cbcb328ad48ed172`.

A synthetic6000x3000 PNG of8422537 bytes was copied to the phone for a preview
check, but the phone was at its lock/ambient screen. The fixture was removed
without selecting it or replacing the draft. Actual Android picker/clipboard
large-source preview remains a physical follow-up; host decoder regressions and
native compilation are not that acceptance. Existing firmware optical/load and
nRF52840 port gates remain separate. No new design decision, commit or push.
