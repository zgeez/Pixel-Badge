# Bring-up expected values

Design values to compare measurements with. None of these are measurements.
Where the design has no number, record what you measure as the board's
baseline. Use with the [checklist](bringup.md).

## Rails

| Rail | Expected | Source |
|---|---|---|
| 3V3 (TP203/TP204) | 3.30 V: 0.5 V × (1 + 112 kΩ / 20 kΩ) | TPS63802 feedback |
| VLED (TP206), off | About 0 V (100 kΩ pull-down, load disconnect) | TPS61023 |
| VLED, on | 4.65–4.89 V, or about SYS minus the drop when SYS is higher | SLVSF14B §7.3.5 |
| SYS (TP201), USB in | At least 3.52 V | SLUSEG2D |
| SYS, battery only | About VBAT + 50 mV | SLUSEG2D §8.3.4.1 |

## Currents

| Quantity | Expected |
|---|---|
| Idle at first power | No design value; board 1 drew 2 mA at 3.7 V before firmware |
| Per lit LED | 18.0 mA nominal, up to about 19.4 mA |
| One full row lit | 576–621 mA peak while BLANK is low |
| First watched charge | 480 mA set by `diagnostics-charge.conf` (power-on default about 1.04 A) |

### Average LED current, all 1024 lit

From the scan timing: on-time per plane is `4 × 2^plane × brightness / 255`
ticks at 16 MHz (at least 1), and each slot is 256 ticks plus the on-time.
Average current is 32 × column current × duty. Supply current depends on
boost efficiency on top of this.

| Brightness | Duty | Frame time | Average VLED current (18.0 / 19.4 mA) |
|---|---|---|---|
| 8 | 0.016 | 4.16 ms | 9.4 / 10.1 mA |
| 32 | 0.059 | 4.35 ms | 34 / 37 mA |
| 128 | 0.200 | 5.12 ms | 115 / 124 mA |
| 255 | 0.333 | 6.14 ms | 192 / 206 mA |

A single LED gets 1/32 of the column current times the duty.

## Timing

| Quantity | Expected |
|---|---|
| Stall one-shot (stepper step 12) | 0.76–1.41 ms calculated; board 2 measured 969–974 µs |
| Row slot | At least 16 µs (256 ticks) |
| 32.768 kHz clock | About 327,680 RTC ticks in 10 s |

## Inputs

| Quantity | Expected |
|---|---|
| CC, default USB current | ≤ 0.612 V at the CC pin (≤ 66 mV at the ADC), logged `default` |
| CC, 1.5 A or 3 A | ≥ 0.746 V at the pin (≥ 80 mV at the ADC), logged `1.5A/3.0A` |
| VBAT via divider | Within ±1 % plus ADC error of a meter at the pack |
| IMU at rest | Magnitude 8800–10800 mm/s²; 80–120 interrupts/s at 100 Hz |
| Microphone | Mean within ±4096, not stuck; AC level rises with sound |
