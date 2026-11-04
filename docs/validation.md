# Validation

What has actually been measured, and where the records are. A passing build
or a DK test says nothing about the custom board; each record states its own
scope.

| Area | Record | Scope |
|---|---|---|
| Rev A bring-up | [Boards 1 and 2](../validation/results/2026-11-03-rev-a-bringup/README.md) | Diagnostics, an assembly fault, the hardware scan, charging, one discharge run, BLE, NFC and recording working |
| Scan timing | [DK logic-analyzer capture](../validation/results/2026-10-09-dk-matrix-timing/README.md) | 195.2 Hz on a standalone bench, no matrix |
| QR | [HUB75 screening](../validation/results/2026-10-11-hub75-qr-screening/README.md) | One phone, three tries per setting |
| Settings and pairing | [BLE checks](../validation/results/2026-10-13-dk-p2-settings/README.md) | DK, with security rejection logs |
| NFC | [Provisioning proof](../validation/results/2026-10-12-dk-positive-nfc-proof/README.md), [local sync](../validation/results/2026-10-15-dk-local-nfc-sync/README.md) | DK and an external tag |
| Audio, motion | [Audio](../validation/results/2026-10-14-p3-audio-start/README.md), [motion](../validation/results/2026-10-15-dk-p4-motion/README.md) | Software and DK |
| App | [UI review](../validation/results/2026-10-17-app-visual-cleanup/README.md), [artwork transfer](../validation/results/2026-10-17-editor-transfer/README.md) | Phone and DK |
| nRF52840 port | [Pro Micro](../validation/results/2026-10-18-promicro-bench/README.md), [Rev A port](../validation/results/2026-10-18-rev-a-port/README.md) | Before the boards arrived |

Everything else is in [validation/results/](../validation/results/). Older
DK summaries are collected in [prototype results](testing/prototype-results.md).

On Rev A, bare LEDs gave a QR code phones couldn't read in any polarity or
brightness. With the printed diffuser it scans easily. That's a hands-on
result so far, not a recorded scan test.

## Still to test on Rev A

| Area | Procedure |
|---|---|
| QR scan rate with the diffuser | [Display tests](testing/display.md#qr-scan-reliability) |
| Scan timing on a logic analyzer, ghosting, LED headroom | [Display tests](testing/display.md#matrix-timing-and-ghosting) |
| Inrush, TS behavior with a cell probe, pack protection | [Electrical tests](testing/electrical.md) |
| Current per mode | [Electrical tests](testing/electrical.md#current) |
| BLE range | [Wireless tests](testing/wireless.md#ble) |
| NFC range and RF-write rejection | [Wireless tests](testing/wireless.md#nfc) |
| IMU timing under load, microphone response | [Sensor tests](testing/sensors-audio.md) |
| Thermals while charging | [Electrical tests](testing/electrical.md#thermal) |
| Runtime against the eight-hour target | [System tests](testing/system.md#battery-runtime) |
| Carrier fit and retention | [System tests](testing/system.md#wearability) |
| System OFF and brownout recovery | [System tests](testing/system.md#brownout-and-recovery) |

## Recording results

For each result, note the date, board, firmware commit, equipment, settings,
method, where the raw data is, the result and the pass limit. Put raw data in
`validation/results/<date>-<name>/` with file hashes in its README. Captures
over about 5 MB go in a release asset, with the hash recorded. Mark anything
observed by eye separately from captured data. Limits nobody has agreed on
stay marked TBD or PROPOSED.
