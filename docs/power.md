# Power and USB

The BQ25620 supplies SYS from USB or a protected 1S LiPo. A TPS63802 supplies
3.3 V logic; a TPS61023 supplies the switchable LED rail. Circuit values below
are design calculations; board measurements are pending.

## USB supply connections

USB-C is a 5 V sink without PD. CC1 and CC2 each have a separate 5.1 kΩ, 1% Rd
to ground, present without battery/MCU power. VBUS feeds the charger and E73
pin 27; D+/D− go to pins 31/29. The module branch uses R401 2.2 Ω and C402
4.7 µF, inherited from the earlier module circuit and still requiring E73
USBREG/inrush checks.

| Source | Charger input limit |
|---|---|
| Unknown/default source or host before configuration | 100 mA |
| Configured USB host | 500 mA |
| CC 1.5/3 A advertisement or BC1.2 DCP | 1.5 A |
| Configured host suspended | HIZ; pack supplies the badge |

CDP is treated as SDP because enumeration within its 2.5 s allowance is not
guaranteed. The inert CDC ACM interface enables host configuration; the D+
pull-up connects after BC1.2 detection. CC is sampled every 20 ms; lower
advertisements reduce the limit within the Type-C 60 ms adjustment window.
HIZ requires VBAT of at least 3.40 V.

CE is pulled inactive. Firmware configures/readbacks the charger before driving
CHG_EN high; reset or lost logic power disables charging. The input limit is
separate from battery charge current. Watchdog expiry halves ICHG but retains
the input and temperature settings.

Each CC sense path uses 100 kΩ, an AO3400A pass gate tied to 3V3, then 12 kΩ
and 100 nF to ground. Ratio is 0.107; powered loading gives about 4.88 kΩ
effective Rd. SAADC uses gain 2, 0.6 V reference; settle at least 8 ms.
Default/1.5 A CC boundaries map to about 66/80 mV. Check ADC error against
that margin and check off-state voltage through power-up/down.

C303/C304/C402 total 15.7 µF nominal on VBUS. Effective capacitance and inrush
need measurement against the 10 µF ∥ 44 Ω reference. Do not assume DC bias
alone brings it below the attach limit. Changes to R401/C402 need an E73/
nRF52840 supply review; the charger branch must not pass through R401.

## Power states

The switch is a control input: `EN_3V3 = switch ON or charger PG`.

| USB | Switch | Behavior |
|---|---|---|
| Absent | OFF | Logic off; passive NFC remains available |
| Absent | ON | Normal battery operation |
| Present | OFF | Charging-only: manager/USB on, matrix/audio/IMU/BLE/NFC I²C off |
| Present | ON | Normal operation with the input-current limit enforced |

LED_EN has a pull-down and stays off at boot. USB removal with the switch off
turns logic off in hardware. PG keeps logic powered during USB HIZ; verify this
and total suspend current on the board.

## Independent battery protection

The design reference is Adafruit 328, protected 2500 mAh, with no thermistor.
Confirm the actual pack label/protection IC and its specification before use:
the available PKCELL and Sounddon sheets differ. Apply the stricter 0–40 °C
charge envelope until identity is confirmed. The design applies Adafruit's
1.2 A charge limit; the PKCELL reference lists 1.5 A continuous discharge and
a 3.0 V cell cutoff;
its protector trips lower, so firmware must shed load earlier.

The BQ25620 and firmware do not replace independent pack protection. Header
pin 1 is BAT−, pin 2 BAT+; meter-check each pack before connecting it. Use
controlled fixtures for protection tests, not an uncontrolled LiPo short.

## Regulators and charge current

| Rail/function | Design setting |
|---|---|
| BQ25620 | 4.200 V regulation; SYS minimum 3.52 V on USB |
| Watched first charge | 480 mA, explicit diagnostics configuration |
| TPS63802 | 3.30 V, 112 kΩ/20 kΩ feedback, 0.47 µH inductor |
| TPS61023 | 4.77 V typical; calculated 4.65–4.89 V, 33 kΩ/4.7 kΩ feedback |
| LED/charger inductors | XGL4020-102MEC, 1 µH |
| Column setting | 18.0 mA nominal, 2.94 kΩ IREF; about 19.4 mA with tolerance at 25 °C |

The boost cannot regulate below SYS: `VLED ≈ max(setting, SYS − drop)`.
Current adjustment is series resistance only. The design estimate is
`I_BAT = 32 × I_column,max × VLED,max / (efficiency × VBAT) + I_logic`.
The original budget uses 0.8 efficiency, 3.0 V VBAT, about 60 mA logic, and
the tolerance-adjusted column current. Verify actual current, headroom,
inductor ripple/peaks, and startup/overload behavior before accepting the limit.

The 10 kΩ NTC (B=3380 K) and 10 kΩ REGN pull-up measure board temperature,
not cell temperature. Calculated reset COLD/HOT limits span −1.5–1.1 °C and
41.7–44.5 °C. Firmware sets `TS_TH4_TH5_TH6=011` and adds a stricter hot gate:
CHG_EN stays low below TS 38.4% of REGN (about 38 °C), released at 40.0%
(about 36.1 °C). These are circuit estimates, not cell-temperature accuracy.
Cold-cell/warm-board, transitions, and hot-cell/cool-board tests are required.
Charging stays off by default.

## Battery-state measurement

VBAT is measured before the charger power path through an AO3401A high-side
switch and 47 kΩ/47 kΩ divider, with 100 nF at AIN6. The gate defaults off.
SAADC uses a 0.6 V reference, gain 1/4, 12 bits, 10 µs acquisition. Wait at
least 25 ms after enabling (RC time constant 2.35 ms), then disable the path.
Calibrate each board at two voltages and measure residual error/leakage.
This measures voltage for shedding, not state of charge or remaining runtime.

Proposed firmware defaults, pending pack characterization:

| Condition | Action |
|---|---|
| Loaded VBAT < 3.55 V | Cap brightness at 127 |
| < 3.40 V for three 100 ms samples | Shed the LED rail |
| Unloaded ≥ 3.70 V for 10 s | Battery-qualified recovery |
| USB limit ≥ 500 mA and unloaded ≥ 3.50 V, outside HIZ | Input-qualified recovery |
| Unloaded < 3.30 V for three readings | System OFF |
| Missing/stale/invalid sample | Keep the matrix off |

Recovery starts at 13, then 64/127/255 in 5 s steps. Input-qualified brightness
uses a worst-case total-system current estimate with a proposed 20% margin.
A shed within 60 s counts as failure; retry delay doubles from 60 s to 15 min.
After three failures, require battery-qualified recovery. USB presence alone
is insufficient: at 100 mA, the pack still supplies the display deficit.

## Runtime and validation

Eight hours is a target. Measure it with a stated brightness/mode/audio/BLE
profile and normal-stop condition. Before wearable use, verify the pack,
charging temperatures, current limits, inrush, ADC accuracy, shedding margins,
and wake/recovery paths.
[Bring-up](bringup.md) · [Power implementation](rev-a-firmware.md#power-manager) ·
[Test plan](testing/electrical.md) · [Primary sources](references.md)
