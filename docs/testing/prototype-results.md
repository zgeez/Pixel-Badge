# DK results

Short summaries of the early nRF5340 DK results. Each links to its full
record, which has the firmware identity, setup and limits. None of them
applies to Rev A.

## First checks, 2026-10-11

All six [first checks](foundation.md) were reported passing. The
[RTT capture](../../validation/results/2026-10-11-dk-p0-foundation/README.md)
confirms a normal QR boot and the healthy counter clearing, but the button,
persistence, factory-reset and watchdog steps were observed, not captured.
The image was an uncommitted working tree from before later recovery fixes.

## QR screening on the HUB75 panel, 2026-10-11

A 64 × 64 HUB75 panel at the badge's 2 mm pitch showed a 32 × 32 window. One
phone with Google Lens, three tries per setting: Version 1 codes at levels L,
M and Q, both polarities, brightness 255, 64 and 16. All 48 settings decoded
every time, including some experimental Version 3 codes with a reduced quiet
zone. Distance was adjusted per setting and decode times weren't recorded,
so this is screening, not acceptance. In a lit room the phone had to stand
further back from a dim code.
[V1 record](../../validation/results/2026-10-11-hub75-qr-screening/README.md),
[V3 record](../../validation/results/2026-10-11-hub75-qr-v3-reduced-quiet-zone/README.md).

The HUB75 panel's RGB LEDs behave nothing like Rev A's: on the bare Rev A
matrix, phones couldn't read the code at all until the diffuser went on.

## Scan timing, 2026-10-09

The [timing bench](matrix-timing.md) at commit `d541e00880b3`, captured at
24 MS/s for 8.33 s. Over 1234 frames: 160.0 µs row slots, rows lit for
155.0 µs, a 1.0 µs latch always inside the blanked window, and all 39,485
shifts decoding to the expected pattern. Frame rate was 195.2 Hz. In 5 of
1233 frames the frame interrupt started 12–15 µs late; BLANK stayed high,
as designed. With the CPU halted, the scan finished its frame and stopped
with BLANK high. Behavior under Bluetooth load wasn't tested.
[Full record](../../validation/results/2026-10-09-dk-matrix-timing/README.md).
