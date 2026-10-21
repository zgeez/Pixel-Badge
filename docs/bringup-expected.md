# Rev A bring-up expected values

Design values to compare with bring-up measurements, each with its source.
None of them is a measurement. Where the design has no calculated value, the
table says so: record the measurement as the board's baseline instead of
judging it against a guess. Use beside the [checklist](bringup.md) and the
[log reference](bringup-logs.md).

## Rails

| Quantity | Expected | Source |
|---|---|---|
| 3V3 logic rail (TP203/TP204) | 3.30 V set point: 0.5 V × (1 + 112 kΩ / 20 kΩ). Tolerance follows the TPS63802 feedback accuracy and the 1 % resistors | [power.md](power.md) logic rail |
| VLED (TP206), LED rail off | About 0 V: LED_EN 100 kΩ pull-down, TPS61023 load disconnect | [power.md](power.md) LED rail |
| VLED, LED rail on | 4.65-4.89 V (4.77 V typical) from 33 kΩ / 4.7 kΩ and VREF 580-610 mV; when SYS is higher than the setting, about SYS minus the pass-through drop | [power.md](power.md) LED rail (SLVSF14B §7.3.5) |
| SYS (TP201), USB present | At or above the 3.52 V minimum system voltage | [power.md](power.md) charger (SLUSEG2D) |
| SYS, pack only, or charging done or disabled | About VBAT + 50 mV | [power.md](power.md) LED rail (SLUSEG2D §8.3.4.1) |

## Currents

| Quantity | Expected | Source |
|---|---|---|
| Idle current, first power (no pack, LED rail off) | No calculated value in the design. Record it as the baseline; stop if anything gets hot or the supply reaches its current limit | [power.md](power.md) (quiescent current: calculate and measure) |
| Column current per lit LED | 18.0 mA nominal (R_IREF 2.94 kΩ), up to about 19.4 mA with driver and resistor tolerance at 25 °C | [power.md](power.md), [hardware.md](hardware.md) |
| Peak VLED current, one full row lit | 32 × 18.0-19.4 mA = 576-621 mA while BLANK is low | Column current × 32 columns |
| Charge current, first watched charge | ICHG 480 mA set by `diagnostics-charge.conf` (power-on default 1040 mA, 0.98-1.10 A, before firmware configures it) | [power.md](power.md) charger |

### Average LED current at the matrix bench steps

Upper bounds on the average VLED-side current from the firmware's timing
(`hub75_timing.h`): TIMER at 16 MHz; on-time per bit plane
`4 × 2^plane × brightness / 255` ticks (at least 1); slot period
`max(on + 64, 256)` ticks. Real values are lower, because SPI and thread time
between slots lengthen every slot. Supply-side current also depends on the
boost efficiency, which is not stated here.

| Bench step | Lit LEDs | Duty | Average VLED current (18.0 / 19.4 mA) |
|---|---|---|---|
| 2, 3: single LED, brightness 1 | 1 | 0.0044 | 0.08 / 0.09 mA |
| 9: all on, brightness 8 | 1024 | 0.0166 | 9.6 / 10.3 mA |
| 10: all on, brightness 32 | 1024 | 0.0625 | 36.0 / 38.8 mA |
| 11: all on, brightness 128 | 1024 | 0.2418 | 139 / 150 mA |
| (product, brightness 255) | 1024 | 0.4194 | 242 / 260 mA |

Duty is the sum of on-times over the sum of slot periods for the eight bit
planes. Each slot drives one row, so with all LEDs on the average current is
32 columns × column current × duty. The minimum frame period (32 rows × 8
planes) is 4.10 ms up to brightness 32, 4.23 ms at 128 and 4.86 ms at 255.

## Timing

| Quantity | Expected | Source |
|---|---|---|
| Scan-stall one-shot width (bench step 12) | 0.76-1.41 ms by calculation | [hardware.md](hardware.md) matrix |
| Row slot, LAT to next LAT | At least 16 µs (256 ticks) | `hub75_timing.h` |

## Inputs read by firmware

| Quantity | Expected | Source |
|---|---|---|
| CC, default USB current advertised | At the CC pin ≤ 0.612 V (≤ 0.066 V at the SAADC pin); logged as `default` | [power.md](power.md) CC sensing (USB Type-C R2.5 Table 4-36) |
| CC, 1.5 A or 3.0 A advertised | At the CC pin ≥ 0.746 V (≥ 0.080 V at the SAADC pin); logged as `1.5A/3.0A` | Same |
| VBAT through the switched divider | Within the divider's ±1 % ratio plus SAADC error of a meter at the pack; record the difference for calibration | [power.md](power.md) battery measurement |
| IMU at rest | Acceleration magnitude 8800-10800 mm/s² (gravity 9807); 80-120 data-ready interrupts per second at 100 Hz | `bringup.cpp` pass bands |
| Microphone | Mean within ±4096 and not stuck; AC RMS rises with sound | `bringup.cpp` (threshold set on the DK with a different microphone) |
