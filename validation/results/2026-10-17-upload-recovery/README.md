# Large-animation upload and reconnect recovery

Owner report, 2026-10-17: BLE stopped during a reported 96-frame animation upload
and would not reconnect; some connection/text-size labels appeared garbled.
Worktree `app/prototype-polish`, uncommitted prototype-polish/grayscale/128-frame
work based on `39593e8`. No commit or push performed.

## Corrections

The DK was in watchdog-loop safe mode, which disables normal configuration.
The eight-plane driver started its shortest row timer before bit-banging the next
row. A compare can therefore expire inside the same ISR; repeated pending row
interrupts can starve the main/watchdog thread. Move TIMER START after shifting,
while the one-shot timer and OE remain off. Each subsequent timed slot now begins
after GPIO work. Pulse weights remain unchanged; total scan period includes
shifting and needs optical/timing acceptance. This fixes an enforceable scheduling
hazard; the original first watchdog event was not captured at its point of failure.

The app requested the larger MTU only for upload/audio, after initial settings
readback. Reconnect could therefore download artwork using 14-byte default-MTU
pages while displaying Reconnecting. Negotiate and cache the transfer MTU before
the initial artwork read, expose read progress and label this phase Loading
settings. Metadata/payload CRC and exact post-Apply readback remain required.

Affected status punctuation now uses plain ASCII; text size shows Auto/1x/2x/3x.
Source and prepared 0.8.0 APK did not reproduce literal mojibake sequences, so
this is a label normalization rather than a proven diagnosis of its origin.

## Verification and installation

- **115 Flutter tests passed**, including a regression that initial large-artwork
  read negotiates first and reports progress. Analyzer clean. Existing small-phone,
  keyboard/large-type layout and transfer/lifecycle tests pass.
- Normal, recording-enabled and NFC setup DK builds pass. Recording application
  allocates **206296 flash / 104497 RAM bytes**. Repository formatting/YAML/hygiene
  and `git diff --check` pass. No Kotlin or domain-storage behavior changed; their
  previous milestone suites were not rerun for this repair.
- Recording-enabled firmware installed through J-Link 1050067477 with --no-erase.
  HEX images exclude persistent partitions. A fresh encrypted DPAPI 320 KiB
  backup was verified first. No factory reset, bond deletion, tag provisioning or
  recording was initiated. Watchdog retention was cleared during recovery.
- Public state after firmware installation: writable, bond loaded, NFC ready /
  result0, CPU running and healthy boot. Persistent bytes changed between the
  backup and readback during concurrent app/use attempts (5133 previously written
  bytes differ and 36 new bytes), so byte-identical settings retention is not
  claimed. Recovery backup remains local/encrypted; no rollback was performed.
- Temporarily force eight-plane scan flags in display RAM, restore afterward:
  **35 seconds / 1488 samples**, scan counter advances, configuration stays
  writable and watchdog retention remains 0xa0. No saved content changed by this
  diagnostic. This does not measure optical shades or full combined load.
- Install **Pixel Badge 0.8.2/build 24** over the existing app on Nothing A069P.
  Package version verified. Retry the existing imported clip (93 frames, 36297
  encoded bytes / 35.4 KiB; owner called it 96 frames). Artwork Apply id 3/field 6 and
  subsequent animation-mode Apply id 4/field 4 both return phase 2/result 0. The app
  cannot proceed to the mode Apply until exact artwork readback succeeds.
- Runtime sample of the actual animation: mode 3 and writable throughout 35 samples
  over 34.2 s; scan counter 30248 -> 32620; watchdog retention 0xa0 throughout. See
  [numeric runtime summary](runtime-summary.json). Physical optical appearance is
  still owner acceptance; advancing software scan counters alone do not prove it.
- Restart the app process and reconnect using the existing bond. Current process
  logs **MTU 247** before settings reads. Before/after status snapshots match
  phase 0/id 0/flags 3; connected Animation dashboard and preview observed. Snapshot
  read bracket was approximately 15.5 s for this clip. Larger clips may still take
  longer; progress reflects reading rather than pretending pairing is ongoing.

## Artifacts

- `app/build/app/outputs/flutter-apk/app-debug.apk`: SHA256 `eeeee59830235cefe6ebff2b4dc55b0129dcd66a3c5484a6d5dbf20478276490`
- `build/p3-recording/merged.hex`: SHA256 `d7d196597f863d59fe8e086d03cebfd04b259e9ac2bdc9fabe21febfa7bea17f`
- `build/p3-recording/merged_CPUNET.hex`: SHA256 `66ed99eeebcf3ab93449c8ecf356aad6f13eaadaab4f7f87ddc11c158e871eec`

Ignored raw evidence: `build/upload-*`, `build/upload2-*` logs, encrypted recovery
backup, phone/UI captures and numeric samples. Do not publish backups, credentials
or the owner's imported media. Previous 128-frame/131718-byte software tests remain
capacity evidence; this physical clip exercises 93 frames only.

## Remaining gates

Optical grayscale/refresh/ghosting, maximum-size physical upload, measured combined
BLE/audio/motion load and Rev A/nRF52840 resources remain open. The scan-counter
rate under debug sampling is not the optical refresh acceptance measurement.
No new unresolved owner decision; existing OD22/OD60 remain unrelated/open.
