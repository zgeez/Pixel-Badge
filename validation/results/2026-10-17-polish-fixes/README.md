# Prototype polish corrections - 2026-10-17

Revision: app/prototype-polish working tree based on main39593e8; the prepared
commit plan fingerprints the final files. Target: nRF5340 DK/HUB75 prototype,
NCS3.4.1; Android Pixel Badge0.10.3/build31. No nRF52840 fit/bring-up claim.

## Corrections

- Keep saved Bounce visible but disabled on legacy firmware; supported layouts
  can replace it without a selector exception or silent draft conversion.
- GIF89a streaming sampled decoding uses a4096-entry dictionary and <=256x256
  composition buffers. Retain at most128 frames/32MiB RGBA; file reads use64KiB
  chunks. No native source-sized GIF canvas. Work is off-thread and limited to
  256 million expanded pixels. Nearest-center sampling preserves bounded state,
  including transparent partial frames/disposal/interlace. Non-GIF animations
  above4 million source pixels ask for GIF conversion before frame decoding.
  Still formats remain subject to platform codec resources.
- Use one eight-plane HUB75 scan for binary and grayscale input. Unchanged white
  pixels therefore share a duty schedule regardless of other pixel values.
  Binary-only nominal duty is reduced to the existing gray budget; this is a
  calculated correction, not a measured optical or power result.
- Both Scroll and Bounce qualify for20ms render cadence outside display recovery;
 100ms recovery throttling remains.
- Build and pointer updates never encode the clip for budget display. Check only
  the changed frame's presence during a stroke; compress a settled snapshot on
  a worker isolate and discard stale results. Debounce autosave until stroke end;
  one undo checkpoint per stroke remains. Exact encoding/readback is still used
  for Send and persistence.

## Verification

-142 Flutter tests passed, including legacy selected Bounce, a continuous stroke
  over128 frames without per-move encoding, a65535-square logical GIF canvas,
  full5000x100 GIF frame, file streaming,129-source-frame truncation and corrupt
  input. Independent native RGBA/timing comparisons cover disposal, interlace,
  palette and dictionary/code-size transitions with synthetic fixtures.
-147 native firmware cases/21 configurations passed on native_sim/native in the
  pinned SDK container. Includes moving-text scheduling and exhaustive plane/
  brightness pulse bounds, plus storage/CRC/interruption/migration suites.
-21 Android app JVM cases/seven suites passed. Analyzer, Dart formatting,
  C/C++ formatting, YAML/hygiene and whitespace checks pass.
- Normal, recording-enabled and NFC setup DK builds pass. Recording application
  allocation206568 flash/104497 RAM bytes; these are link allocations on the DK.
-8 QR config,3 app defines and17 NFC host tests pass (ResourceWarning is an error).
- Debug APK built with existing local contact-link defines, values omitted from
  logs. Kotlin incremental compilation fell back successfully on this Windows
  checkout; Android tests explicitly use -Pkotlin.incremental=false.

The four-group signed commit script passes its preview and refuses a wrong
resume point or altered fingerprint without changing the index.

Raw logs/reports remain ignored under build/polish-fixes-* and Android/Flutter
build output. Synthetic fixtures and regression sources are public. No phone
recording, firmware flash, APK install, provisioning, bond/settings erase, commit
or push was performed. Owner routing/root tracked hardware/index are untouched.

## Remaining physical checks

Scope/optical refresh, binary QR brightness after the common duty schedule,
perceived grayscale, combined BLE/audio/motion load, maximum transfer and changed
recording-view visibility remain bench gates. Existing Rev A power/port/security
acceptance remains separate. No new owner design decision was needed.
