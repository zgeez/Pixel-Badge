# Prototype foundation bench record

- Date: 2026-10-11.
- Hardware: nRF5340 DK and existing HUB75 bench setup, 32 x 32 window.
  Exact panel/phone identifiers were not re-recorded in this test session.
- Firmware: uncommitted initial P0 on `fw/prototype-foundation`, based on
  `b57a97ce1fa0503e5e400c4936eb958369a646f8`; default application plus the
  watchdog-loop fragment for the forced-reset test. This predates the later
  display retry/schema recovery corrections. A committed tested-image revision
  and image hash were not retained for this session.
- Equipment: my panel/power/phone setup for functional checks;
  J-Link RTT Logger V9.82, application-core device `nRF5340_xxAA_APP`, SWD
  4000 kHz, RTT channel 0 for the independent capture.
- Method: six steps in [the firmware guide](../../../docs/firmware.md#historical-foundation-checks).
- Threshold: functional expected behavior from that procedure; no numerical
  optical acceptance or GPIO timing threshold was measured here.

## Outcomes and evidence limits

My manual notes record all six steps passing, with RTT checked: normal
QR/name and brightness controls, reserved long presses, brightness surviving
reset/power cycle, boot-button factory reset, watchdog-loop safe mode and
restoration of the normal image. These outcomes were not independently
repeated during the direct capture.

The direct capture contains NCS/Zephyr boot banners, reset-cause read success,
eight 4096-byte NVS sectors, expected missing-setting default warnings,
`badge ready: QR mode`, and `healthy boot; reset-loop counter cleared` at
uptime 30.111 s. It contains no error messages and none of the earlier
individual test sequences. Attaching the logger did not flash/reset the DK
or inject input; the logger was subsequently closed cleanly.

## Raw data

| File | Bytes | SHA-256 |
|---|---:|---|
| [normal-boot-rtt.log](normal-boot-rtt.log) | 989 | `66fb0fbdc43bcbf98fa357e1456a9469472d42ec0eca27577c2b0dcbfd9afc31` |

This is the original capture, including terminal color escapes. Measured
button/GPIO timing, interrupted physical flash writes, concurrent optional
checks, broad optical acceptance and the corrected firmware remain outside
this functional pass. No Rev A hardware result is claimed.
