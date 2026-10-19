# Editor controls and BLE readback - 2026-10-17

Pixel Badge0.10.0/build28 and matching nRF5340 DK firmware. This extends the
existing uncommitted prototype-polish work; no commit or push was performed.

## Changes

- All five navigation labels remain visible. Text size has a compact unfilled
  trigger and separation from the text field. Focused text offers precise caret
  arrows; native selection remains available. [Synthetic capture](editor-caret.png).
- Bounce lines preserves explicit lines, independently scrolling wide lines in
  both directions with endpoint pauses. Short lines retain alignment; extra rows
  page. Dart preview and firmware use the same algorithm. Text speed is50..500ms.
- New design confirms replacing all frames with one blank frame while retaining
  the saved library. Clear frame still affects only the selected frame.
- The low global on-time preset is13/255, labeled5%, replacing16/255. Old stored
  values normalize in RAM without an eager write. Legacy connected firmware keeps
  the6% preset. Per-pixel intensity remains0..255; optical output is not measured.
- Capability bit7 gates editor features and sequential artwork readback. A single
  cursor setup replaces a cursor write before every page. Android requests high
  connection priority during artwork transfer and restores balanced priority.
  CRC, stable metadata, bounds and exact post-Apply readback remain required.

## Software verification

- Flutter134 tests pass, including two optional locally cached-media cases
  (132 without those cases); analyzer and Dart formatting pass.
- New regressions cover sequential pages/invalid offsets, capability dependency,
  new and legacy brightness, bouncing line reversal/blank lines, caret placement
  and clearing all frames without deleting library items.
- Native Twister51 cases across five executed configurations pass with no
  warnings: BLE protocol including alternate errno, settings, modes and content.
  Content ran on native_sim/native; the other four configurations used native/64.
- Normal, recording and NFC-setup sysbuilds pass with the repository's NCS3.4.1
  toolchain. Recording application uses206808 flash bytes and104505 RAM bytes;
  normal uses207120/104505; NFC setup uses86412/47235. These are DK allocations,
  not nRF52840 resource acceptance.
- Real-font light/dark, focused-caret, menu and narrow/large-text captures were
  inspected. Repository C/C++ formatting, YAML, hygiene and whitespace checks pass.

Build/test logs remain in the ignored build directory: editor-all-tests.log,
editor-final-targeted.log, editor-analyze.log, editor-visual.log,
editor-native-verified/, editor-content-native/, editor-*-build.log and
editor-hygiene.log. The native checks used the pinned disposable SDK container;
no global tools were installed or upgraded.

## Deployment and physical observations

The app was installed in place and package version checked. A fresh app
draft/library hash was unchanged across installation. Firmware was flashed with
no erase after an encrypted320KiB persistent-region backup. Immediately after
flash, the0xb0000..0x100000 region had zero changed or newly programmed bytes
versus that backup. Bond loaded, NFC ready/result0, healthy boot and running CPU
were observed. [Sanitized deployment summary](deployment.json).

The owner unlocked the Nothing phone for a read-only bonded reconnect. It
reconnected without a new pairing code and returned to enabled settings controls.
No text, artwork or setting was applied, and no recording was started. The actual
Text page showed all navigation labels, compact size control and cursor arrows.

Debug timings on that fresh connection:

```text
14:47:16.528 Pixel Badge artwork MTU: 247
14:47:17.809 Pixel Badge artwork read: 3551 bytes / 786 ms
```

That is approximately4518 content bytes/s (4.5 decimal kB/s) for the page loop;
it excludes scanning, authentication, initial/final metadata and the rest of
settings loading. It is a single small-asset sample. The earlier approximately
15.5s settings reconnect for36297 bytes is not an equal-size controlled baseline.
Do not claim a measured2x speedup from these two observations.

At MTU247, each acknowledged upload packet carries up to235 content bytes and
each artwork read carries236. A maximum131718-byte read needs559 pages; sequential
mode removes558 of the old per-page cursor writes, while retaining one initial
selector. Upload staging still uses acknowledged writes, followed by Apply and
complete verified readback. A new staging-upload rate and maximum-size end-to-end
timing were not measured here; no owner design was overwritten for a benchmark.

## Artifact hashes (SHA-256)

| Artifact | Hash |
|---|---|
| APK0.10.0+28 | `d9a7f9faaded52ad7890f1ea18333cc10894f76fbb80273d885e4c3f2a6e5ccc` |
| Recording merged.hex, installed | `9e1dfe37d0d2493c570d0931649f7d2da08e4321f41c42d6666ba724de64f652` |
| Recording merged_CPUNET.hex, installed | `66ed99eeebcf3ab93449c8ecf356aad6f13eaadaab4f7f87ddc11c158e871eec` |
| Normal merged.hex, built | `bf5571e0e9ec492128c57e82d27efe8bffb05136c6e0a0ccbb625601b7d9343c` |
| NFC-setup merged.hex, built | `d0748b7141d13a4cb7de4ca1881521a37681a132651ff680a645918f907c01af` |

Remaining physical checks include optical grayscale/refresh, per-line bounce on
the panel, maximum-size transfers and combined audio/motion/display load. The
nRF52840 Rev A port, resources and hardware acceptance remain separate. No new
owner design decision was introduced.
