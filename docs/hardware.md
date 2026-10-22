# Rev A hardware

The board is a 118 × 76 mm, four-layer design in KiCad 10.0.6. It is in
production; electrical, optical, RF, and wearable testing awaits the assembled
boards. Open the project and run checks using the
[hardware source guide](../hardware/README.md).

## Development vs final hardware

| | Prototype fixture | Rev A badge |
|---|---|---|
| MCU | nRF5340 DK | Ebyte E73-2G4M08S1C / nRF52840 |
| Display | External HUB75 RGB panel, 32 × 32 window | 1024 white SMD LEDs |
| Sensors/tag | External modules | ICM-42688-P, PDM mic, NTAG on PCB |
| Power | USB and bench supply | Protected 1S LiPo, USB-C power path |
| Debug | DK debugger and RTT | 10-pin SWD header and RTT |

The fixture supports firmware development and scoped tests. Its display,
antenna, power supply, and acoustic path differ from the finished board.

## Selected Rev A components

| Function | Part / implementation |
|---|---|
| MCU | Ebyte E73-2G4M08S1C with integrated ceramic antenna |
| Column current | 2 × TLC59283RGER; 2.94 kΩ IREF, adjustable downward |
| Row selection | 4 × SN74HC595, SN74LVC8T245, 32 × AO3401A |
| Scan-stall blanking | SN74LVC1G123, inverter, and OR gate |
| Matrix | 32 × 32 KT-0603W LEDs, 2 mm pitch, 1:32 scan |
| NFC | NT3H2111W0FHKH with a 30 × 25 mm PCB coil and tuning footprint |
| IMU | ICM-42688-P over I²C |
| Microphone | SPH0641LM4H-1, bottom port through a 0.6 mm PCB hole |
| Charger | BQ25620 switching charger with NVDC power path |
| Regulators | TPS63802 for 3.3 V logic; TPS61023 for switchable VLED |
| Battery design reference | Adafruit 328, protected 2500 mAh 1S LiPo |
| Interface | USB-C, two buttons, slide switch, status LED, SWD header |

Pin numbering, footprint sources, and simplified 3D-model limitations are in
the [library reference](../hardware/libraries/README.md). Component source
documents are listed in [references](references.md).

## Display and safety

Column current is nominally 18.0 mA, calculated up to about 19.4 mA with driver
and resistor tolerances at 25 °C. These are design values, not measurements.
SPI shifts row/column data; timed blanking covers row transitions. A hardware
one-shot forces blanking if latch pulses stop. Its calculated timeout is
0.76–1.41 ms and must be measured.

LED headroom, current, refresh, ghosting, QR reliability, and grayscale output
require tests on Rev A. The full QR quiet zone fits only Version 1 at one pixel
per module. [Display test procedure](testing/display.md).

## Power and debug

USB-C is a 5 V sink with separate 5.1 kΩ Rd resistors on CC1 and CC2; no USB PD.
VBUS feeds both the charger and the module's USB supply. The signal-level power
switch controls operation; USB can keep logic alive for charging-only mode.
Firmware senses protected-pack VBAT and sheds the LED rail before shutdown.
See [power design](power.md) for circuits, limits, and proposed thresholds.

The battery reference has no thermistor; the board NTC measures board
temperature. Confirm the actual pack identity, polarity, protection limits,
and charge-temperature rating before charging. Eight-hour runtime is a target,
not a measured result.

All 29 module GPIO are used. There are no UART/spare pads or IMU SPI fallback.
Program and log over SWD/RTT. The
[bring-up checklist](bringup.md#debug-and-programming) covers DK debug-out
voltage matching and first programming.

## Layout and carrier

All assembled parts are on the front; only the battery, connector body, and
carrier occupy the rear. The pack area is free of exposed pads and assembled
parts, with insulation required between PCB and pouch. Antenna and NFC keep-outs
span the copper layers.

The printed carrier supports magnetic and lanyard mounting in portrait and
landscape. Current 3MF files are in `hardware/mechanical/`. Accepted dimensions
do not establish thread durability, retention, battery clearance, or RF/thermal
performance. These remain part of [system testing](testing/system.md).

See the [architecture decisions](decisions/README.md) for the module change,
single-sided assembly, and board outline history.
