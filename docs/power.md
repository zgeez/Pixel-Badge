# Power and USB

The BQ25620 runs the system rail (SYS) from USB or a protected 1S LiPo. A
TPS63802 makes 3.3 V for logic, and a TPS61023 boost makes the switchable LED
rail. Values here are design calculations unless they say measured.

## USB

USB-C is a 5 V sink with no PD. CC1 and CC2 each have their own 5.1 kΩ
pull-down. VBUS feeds the charger and the E73's VBUS pin through R401 (2.2 Ω)
and C402 (4.7 µF).

| Source | Input limit |
|---|---|
| Unknown, or a host before configuration | 100 mA |
| Configured USB host | 500 mA |
| CC advertising 1.5 A or 3 A, or a BC1.2 wall charger | 1.5 A |
| Suspended host | HIZ, the battery runs the badge |

The D+ pull-up connects after BC1.2 detection. CDP is treated as a normal
host port. CC is sampled every 20 ms, so a lower advertisement takes effect
within the Type-C 60 ms window. A higher limit only applies after it has been
requested for 200 ms, because the readings bounce for about 100 ms at attach.
HIZ needs VBAT of at least 3.40 V.

CE is tied inactive, and firmware sets and reads back the charger before it
drives CHG_EN high. A reset or lost logic power stops charging.

## Power states

The slide switch is a logic input: `EN_3V3 = switch ON or charger PG`.

| USB | Switch | Behavior |
|---|---|---|
| Absent | Off | Logic off; NFC still readable |
| Absent | On | Normal, on battery |
| Present | Off | Charging only: no display, BLE, audio, IMU or NFC writes |
| Present | On | Normal, within the USB input limit |

LED_EN has a pull-down, so the LED rail is off at boot.

## Battery

Board 1 uses a PKCELL LP785060, a protected 2500 mAh cell. The charger
doesn't replace the pack's own protection. Header pin 1 is BAT−, pin 2 is
BAT+; check every new pack with a meter before plugging it in.

The design keeps to the stricter of the published specs: charge only at
0–40 °C, at no more than 1.2 A. The firmware stops charging when the board NTC
reads above about 38 °C, and starts again below about 36 °C. That NTC sits on
the board, not the cell.

With this board's 10 kΩ NTC network, the BQ25620 places its pre-warm zone at
about 26 °C. Pre-warm and pre-cool keep the normal charge settings, so the
firmware lets charging run in them and blocks only cold, cool, warm, hot and
bias faults.

## Regulators

| Rail | Setting |
|---|---|
| BQ25620 | 4.200 V charge, 3.52 V SYS minimum on USB |
| TPS63802 | 3.30 V |
| TPS61023 | 4.77 V typical (4.65–4.89 V) |
| Column current | 18.0 mA nominal, about 19.4 mA worst case |

The boost can't go below SYS, so `VLED ≈ max(setting, SYS − drop)`.

## Battery measurement

VBAT is read before the power path through a switched 47 kΩ/47 kΩ divider on
AIN6. Firmware turns the switch on, waits 25 ms, samples, and turns it off.
This tells you voltage, not state of charge.

| Condition | Action |
|---|---|
| Loaded VBAT < 3.55 V | Cap brightness at 127 |
| < 3.40 V, three samples in a row | Turn the LED rail off |
| Unloaded ≥ 3.70 V for 10 s | Bring the display back |
| USB at ≥ 500 mA and unloaded ≥ 3.50 V | Bring it back, limited to what USB can supply |
| Unloaded < 3.30 V, three readings | System OFF |
| No recent valid sample | Keep the matrix off |

The display comes back at brightness 13, then steps up every 5 s. If it sheds
again within 60 s, the retry wait doubles, from 60 s up to 15 minutes.

The app shows a rough battery level. The band edges, 3.485, 3.56, 3.69 and
3.92 V, come from one discharge of board 1 at about 140 mA (QR code at half
brightness). That run reached the brightness cap after about 13.4 hours and
shed the LEDs after about 22 hours. Brighter content pulls the voltage
lower, so the level reads low.

While shed, the badge still draws about 12 mA. The two TLC59283 drivers sit
on the always-on 3.3 V rail and account for 6–8 mA of that.

## Still to measure

Runtime against the eight-hour target with a stated usage profile, charging
temperatures with a probe on the cell, inrush on USB attach, and System OFF
current on the pack. [Electrical tests](testing/electrical.md).
