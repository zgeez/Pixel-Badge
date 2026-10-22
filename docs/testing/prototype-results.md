# Prototype result records

Original setup, firmware identity, methods, and limitations for the early DK
measurements. These records apply to their dated images, not current Rev A.
See the [validation index](../validation.md) for later records.

## Result: prototype foundation on the DK

On 2026-10-11 I reported all six steps in the
[foundation bench procedure](../firmware.md#historical-foundation-checks) passed and
confirmed checking RTT: normal QR/name operation, reserved long presses,
brightness persistence across reset/power cycle, boot-button factory reset,
forced watchdog-loop safe mode and restoration of the normal image.

The separate [direct RTT capture and setup record](../../validation/results/2026-10-11-dk-p0-foundation/README.md)
confirms normal QR readiness and healthy-counter clearing at uptime 30.111 s,
with expected default-setting warnings and no error messages. The capture
does not contain the earlier individual button, persistence, factory-reset
or forced-watchdog sequences; those outcomes remain manually reported.

The firmware was the uncommitted initial P0 working tree, before the later
display retry/schema recovery corrections. This is functional bench evidence
for that image, not Rev A validation or evidence that the corrected image was
flashed. Measured GPIO timing, physical power interruption during writes,
concurrent peripheral checks and broad optical acceptance remain unverified.

## Result: QR screening on the HUB75 bench panel

Screening only (one phone, three tries, distance varied), not the acceptance
test.

| Field | Value |
|---|---|
| Date | 2026-10-11 |
| Hardware | nRF5340 DK v2.0.3; 64 × 64 P2 HUB75 panel (2.0 mm pitch, the badge's pitch) through two SN74AHCT245N at 5.00 V; 32 × 32 window, panel turned so the image reads upright |
| Firmware | QR screening build (`firmware/nrf5340_prototype/configs/qr-screening.conf`) of the working tree before commit `b340b0db37b6`, which holds the QR mode (squash-merged to `main` as `414fbf5`; for the QR mode, the merge differs from it only in `hub75.c`, the later brightness fix). Differences in that commit: HUB75 blanking fixes (blanking is not used by the QR mode) and a run-time settings count. The local-link codes came from a local code file (`CONFIG_APP_QR_CODES_FILE`), not in the repository |
| Equipment | Nothing 4a Pro phone (recorded device label) with Google Lens; bench supply |
| Settings | Run 1: Version 1 codes, levels L, M, Q, representative payloads (19 and 16 characters), full quiet zone (5 and 6 modules). Run 2: my links, one Version 1 code at level L (uppercase, 24 characters, full quiet zone) and four Version 3 codes at levels H, M, M, L (24 to 44 characters) with a 1 to 2 module quiet zone. Each code in normal and inverted polarity at brightness 255, 64 and 16; 195 Hz refresh |
| Method | Procedure in the result folders' README. Deviations: 3 tries per setting instead of 10, Google Lens instead of the native camera, distance adjusted per setting, decode times not recorded |
| Raw data | [`validation/results/2026-10-11-hub75-qr-screening/`](../../validation/results/2026-10-11-hub75-qr-screening/) and [`validation/results/2026-10-11-hub75-qr-v3-reduced-quiet-zone/`](../../validation/results/2026-10-11-hub75-qr-v3-reduced-quiet-zone/): per-setting counts (no photos or video) |
| Threshold | None accepted. The proposed acceptance rule (9 of 10 within 1 s, 10 of 10 within 3 s) does not apply to a screening |
| Pass/fail | Screening pass: every setting decoded in all 3 tries (48 settings) |

Observation: the working distance depended on brightness and room light; in a
lit room the phone had to be further away for a dimmer code. Version 3 codes
with the reduced quiet zone also scanned, but they remain a nonstandard
experiment.

## Result: scan timing on the nRF5340 DK (bench, no matrix)

This result predates the move to the nRF52840
([ADR 0015](../decisions/0015-nrf52840-e73-module.md)); the scan timing, including
during BLE activity, is repeated on Rev A.

| Field | Value |
|---|---|
| Date | 2026-10-09 |
| Hardware | nRF5340 DK v2.0.3 (nRF5340 QKAA). No matrix or driver ICs attached; Rev A pins mapped onto the DK by the bench overlay. |
| Firmware | `firmware/nrf5340_prototype/bench/matrix_timing` at commit `d541e00880b3` (squash-merged to `main` as `f4c577f`, with identical firmware and scripts); nRF Connect SDK v3.4.1, board `nrf5340dk/nrf5340/cpuapp`. `zephyr.bin` SHA-256 `4c89e24ee02b8efc24603a943fdee9fcaf797bb62044e5425d7ec8292dc91b53`: the DK's flash read back after the captures and a pristine build of this commit both match it. |
| Equipment | FX2-based 8-channel logic analyzer (sigrok `fx2lafw` driver) with PulseView (libsigrok 0.6.0-git-883c2ac, from the session file); the DK's onboard J-Link (SEGGER J-Link software V9.82) for flashing and halting |
| Settings | 24 MS/s, 200 M samples (8.33 s). D0–D4 = SCK P0.08, MOSI P0.09, LAT P0.11, BLANK P0.12, frame marker P0.26. Edge resolution ±42 ns; the SCK duty cycle is not measurable at this rate. |
| Method | [Matrix scan timing](../firmware.md#matrix-scan-timing). Halt: J-Link command file `connect`, `h`, `sleep 2000`, `g` during the capture. Values measured from the capture by `analyze.py`. |
| Raw data | [`validation/results/2026-10-09-dk-matrix-timing/`](../../validation/results/2026-10-09-dk-matrix-timing/): sigrok session, screenshots, analysis script and output |
| Threshold | Refresh 150–200 Hz (project target). Qualitative: LAT only while BLANK is high, no row lit across the frame restart, BLANK high while the CPU is halted. The blank window length has no accepted threshold (the design estimate is at least 2–4 µs). |
| Pass/fail | **Pass.** Behavior under CPU and Bluetooth load not tested. |

Summary (6.3 s of scanning: 1234 frames, 39 485 rows):

- Row slot 160.0 µs; rows on for 155.0 µs every slot. BLANK high for 5.0 µs
  around each latch, from 2 µs before LAT rises to 3 µs after.
- LAT pulse 1.0 µs, always inside BLANK high. SPIM4 starts 0.33–0.62 µs after
  LAT falls; 64 clocks span 7.9 µs and end 151 µs or more before the next latch.
- All 39 485 shifts decode to the test pattern (row-select bit 0, then `A5`,
  row, `5A`, inverted row), rows in order, and each frame starts with row 1.
- Frame restart: the frame interrupt starts 2.0 µs after the last BLANK rise
  and runs for 2.0–2.2 µs; BLANK stays high for 7.8 µs. Frame period 5123 µs
  (195.2 Hz). In 5 of 1233 frames the interrupt started 12–15 µs after the
  last BLANK rise instead of 2 µs (cause not identified); BLANK stayed high for
  up to 21 µs and the frame took up to 5136 µs.
- CPU halted: the scan completed its frame (32 of 32 latches) and stopped with
  BLANK high, LAT and SCK low, for 2.01 s, then resumed when the CPU ran.
