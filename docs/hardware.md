# Hardware

Status: **pre-schematic**. Rev A architecture is defined; no schematic or layout
exists yet. Items marked provisional may change. Electrical values come from
primary datasheets and are recorded in [references.md](references.md) as parts
are confirmed.

## Development vs final hardware

| | Development | Rev A badge |
|---|---|---|
| MCU | nRF5340 DK | Raytac MDBT53-P1M module (nRF5340) |
| Debug | On-board debugger (SWD, RTT) | SWD header, UART test pads, test points |
| Display | Off-the-shelf HUB75 panel (below) | 32×32 white SMD LED matrix on the badge PCB |
| NFC | NFC tag breakout (planned) | NFC tag (NTAG I²C plus family) with PCB coil |
| Power | USB / bench supply | 1-cell LiPo with USB-C charging |

Firmware written for the DK must not assume DK-specific pins or peripherals;
board differences belong in devicetree overlays or a custom board definition.

## Optical prototype

Before freezing matrix geometry, QR readability at 32×32 and 2.0 mm pitch is
evaluated on a prototype built from:

- nRF5340 DK
- 64×64 RGB HUB75 panel, 2.0 mm pitch, 1/32 scan (Waveshare)
- SN74AHCT245 level translators
- LSM6DSO IMU breakout (SparkFun)
- PDM microphone breakout (Adafruit)
- Bench power supply

The prototype panel is used only for optical and firmware evaluation; its drive
scheme is not the badge design.

## Rev A architecture

- **Size:** 108 × 72 mm nominal, **112 × 74 mm maximum**; rounded corners;
  4-layer PCB; black soldermask; LEDs on the front, most components on the rear.
- **Matrix:** 32×32 white SMD LEDs, 2.0 mm pitch (provisional, pending QR
  validation), 1:32 multiplexing. Two TI TLC59283 constant-current column
  drivers and high-side row switches (provisional); refresh timed by nRF5340
  peripherals; drive current adjustable on Rev A.
- **NFC:** external passive tag (NTAG I²C plus family) so the link works with the
  badge off or the battery empty; PCB coil with adjustable tuning.
- **USB-C:** charging and USB 2.0 data (5 V sink, no USB Power Delivery), ESD
  protected, on an edge.
- **Power:** 1-cell LiPo (≈ 1200 mAh target) with a power-path charger; charging
  works with the power switch off; regulated logic rail; firmware-switchable LED
  rail. Specific parts provisional.
- **Sensors:** 6-axis IMU on I²C (LSM6DSO family, provisional); PDM microphone
  (provisional).
- **User interface:** two edge-accessible buttons, edge-accessible power switch,
  one status LED.
- **RF:** module-integrated antenna at a board edge/corner with the keep-out the
  module vendor specifies; NFC coil in a separate region; battery and magnets
  kept clear of both.
- **Mechanical:** rear 3D-printed carrier for the battery and attachment magnets,
  mounted to the PCB; target total thickness ≈ 12 mm.

## Rev A requirements

- SWD access, reset access, and power test points (VBAT, VBUS, logic rail, LED
  rail, charger/system rail).
- UART test pads for fallback logging.
- Matrix timing test points (clock, latch, blank, row select).
- Removable links for current measurement where useful.

## Design tools and rules

- KiCad 10.0.6. Project files and custom libraries live in
  [`hardware/`](../hardware/README.md).
- Fabrication outputs (Gerbers, drill, BOM, placement, STEP) are generated as
  CI or release artifacts, not committed.
- ERC/DRC violations are not suppressed without a documented rationale.
- Footprint pin numbering, polarity, and power connectivity are verified
  manually against datasheets before fabrication.
- Fabrication sign-off is a human decision.
