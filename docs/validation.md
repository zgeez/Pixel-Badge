# Validation

The repository includes host tests, phone/DK checks, and measured DK scan timing.
**Rev A board measurements are pending.** A passing build or prototype test
does not establish electrical safety or performance on the custom PCB.

## Evidence

| Area | Recorded evidence | Scope |
|---|---|---|
| Scan timing | [DK logic-analyzer capture](../validation/results/2026-10-09-dk-matrix-timing/README.md) | 195.2 Hz standalone bench; no matrix or BLE load |
| QR | [HUB75 screening](../validation/results/2026-10-11-hub75-qr-screening/README.md) | One phone, three tries per setting; screening rather than acceptance |
| Settings and pairing | [BLE/settings checks](../validation/results/2026-10-13-dk-p2-settings/README.md) | Recorded functional checks and security rejection logs |
| NFC | [Positive provisioning proof](../validation/results/2026-10-12-dk-positive-nfc-proof/README.md), [local sync](../validation/results/2026-10-15-dk-local-nfc-sync/README.md) | DK/external tag; broader RF and Rev A tests pending |
| Audio and motion | [Audio software](../validation/results/2026-10-14-p3-audio-start/README.md), [motion software](../validation/results/2026-10-15-dk-p4-motion/README.md) | Software/deployment records; physical acceptance remains scoped in each report |
| App and artwork | [UI review](../validation/results/2026-10-17-app-visual-cleanup/README.md), [transfer checks](../validation/results/2026-10-17-editor-transfer/README.md) | Phone screenshots, tests, and a measured readback |
| nRF52840 | [Pro Micro bench](../validation/results/2026-10-18-promicro-bench/README.md), [Rev A port](../validation/results/2026-10-18-rev-a-port/README.md) | Bare-board BLE/settings checks and Rev A builds; no Rev A peripheral measurements |

All dated records are under [validation/results/](../validation/results/).
They describe the image and setup tested then, including failed and incomplete
checks. Later changes require relevant retesting.

## Recording results

For each result, record date, hardware revision, firmware commit, equipment,
settings, method, raw-data location, summary, threshold, and pass/fail.
Keep raw data under `validation/results/<date>-<name>/`, with file hashes in its
README. Put large captures (over about 5 MB per result) in a release asset and
record the hash. Label manually reported outcomes separately from direct captures.
Unaccepted numerical limits remain TBD or PROPOSED.

## Planned Rev A checks

Use the [bring-up checklist](bringup.md) for first power-on. Detailed procedures
are grouped below; none of these headings implies a completed test.

### 1. QR scan reliability

[Display tests](testing/display.md#1-qr-scan-reliability): full quiet zone,
payload, brightness, phone, distance, angle, and lighting.

### 2. Electrical correctness

[Electrical tests](testing/electrical.md#2-electrical-correctness): rail
sequencing, USB attachment, charging, isolation, and pack protection.

### 3. Matrix timing, flicker, and ghosting

[Display tests](testing/display.md#3-matrix-timing-flicker-and-ghosting): scan
timing, independent stall blanking, LED headroom, and combined BLE load.

### 4. Current and power

[Power measurements](testing/electrical.md#4-current-and-power) per operating
mode and brightness, including radio peaks.

### 5. BLE

[BLE tests](testing/wireless.md#5-ble): pairing, reconnect, range, configuration,
and unauthorized access.

### 6. NFC

[NFC tests](testing/wireless.md#6-nfc): passive reads, protected writes, MCU
updates, and range with battery/magnets/carrier installed.

### 7. IMU

[IMU tests](testing/sensors-audio.md#7-imu): orientation, interrupt delivery,
particle behavior, and timing under load.

### 8. Microphone and audio processing

[Audio tests](testing/sensors-audio.md#8-microphone-and-audio-processing):
capture, privacy, recording-view visibility, stop paths, and endurance.

### 9. Thermal

[Thermal tests](testing/electrical.md#9-thermal) during charging, display load,
and operation with the carrier fitted.

### 10. Battery runtime

[Runtime test](testing/system.md#10-battery-runtime) using a recorded usage
profile; the eight-hour goal remains a target.

### 11. Mechanical wearability

[Carrier tests](testing/system.md#11-mechanical-wearability): fit, retention,
garment/lanyard use, and battery protection.

### 12. Power cycling, brownout, and recovery

[Recovery tests](testing/system.md#12-power-cycling-brownout-and-recovery):
voltage accuracy, load shedding, rebound, stale samples, and restart behavior.

## Historical result summaries

### Result: prototype foundation on the DK

I reported the initial six-step P0 procedure passed on 2026-10-11.
The [direct RTT record](../validation/results/2026-10-11-dk-p0-foundation/README.md)
confirms normal QR boot and healthy-counter clearing, but does not capture all
button, persistence, factory-reset, or forced-watchdog sequences.
The [historical procedure](testing/foundation.md) documents those steps.
The [original result record](testing/prototype-results.md#result-prototype-foundation-on-the-dk)
retains the setup and evidence limits.

### Result: QR screening on the HUB75 bench panel

All 48 settings decoded in three tries each across the
[V1 screening](../validation/results/2026-10-11-hub75-qr-screening/README.md) and
[reduced-quiet-zone experiment](../validation/results/2026-10-11-hub75-qr-v3-reduced-quiet-zone/README.md).
Distance varied and decode times were not recorded. This does not qualify
nonstandard quiet zones or establish Rev A readability.
The [original screening record](testing/prototype-results.md#result-qr-screening-on-the-hub75-bench-panel)
retains the firmware identity and test settings.

### Result: scan timing on the nRF5340 DK (bench, no matrix)

The [2026-10-09 capture](../validation/results/2026-10-09-dk-matrix-timing/README.md)
measured 160 µs row slots, 155 µs light windows, and 195.2 Hz frame refresh.
All 39,485 shifts matched the pattern. Halting the CPU left BLANK high after
the frame completed. These results apply to the standalone timing image,
not the later grayscale driver or the nRF52840 board.
The [complete measurement record](testing/prototype-results.md#result-scan-timing-on-the-nrf5340-dk-bench-no-matrix)
retains the firmware hash, instruments, capture settings, and timing analysis.
