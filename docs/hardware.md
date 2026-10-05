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
| Power | USB / bench supply | Independently protected 1-cell LiPo with USB-C charging |

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
  validation), 1:32 multiplexing (provisional). Two TI TLC59283 constant-current
  column drivers and high-side row switches (provisional); refresh timed by nRF5340
  peripherals; drive current adjustable on Rev A.
- **NFC:** external passive tag (NTAG I²C plus family) so the link works with the
  badge off or the battery empty; PCB coil with adjustable tuning.
- **USB-C:** charging and USB 2.0 data (5 V sink, no USB Power Delivery), ESD
  protected, on an edge. VBUS feeds both the charger input and MDBT53-P1M
  VBUS pin 44, with the module USB supply circuit described in
  [power.md](power.md); D+ → pin 46, D− → pin 48.
- **Power:** commercially protected 1-cell LiPo pack preferred (≈ 1200 mAh
  starting target), with a power-path charger; charging works with the power
  switch off; regulated logic rail; firmware-switchable LED
  rail. An unprotected cell requires dedicated hardware protection. Specific
  pack, charger, regulators, and protection ratings remain provisional/open.
- **Runtime:** about eight hours under representative career-fair use is the
  design target, not a measured capability. Capacity alone does not establish
  runtime; validate the agreed usage profile in [validation.md](validation.md).
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

- Independent battery protection covering overdischarge, overcurrent, and
  short circuit, with charge protection appropriate to the selected cell. The
  BQ24074 charger/power-path direction is provisional and does not satisfy this
  requirement by itself. Firmware low-battery cutoff is additional control.
- SWD access, reset access, and power test points (VBAT, VBUS, logic rail, LED
  rail, charger/system rail).
- UART test pads for fallback logging.
- Matrix timing test points (clock, latch, blank, row select).
- Removable links for current measurement where useful.

## Schematic preparation and review

- Verify the module symbol pin → footprint pad → datasheet pin mapping against
  Raytac Ver.E §2.5, including VBUS (44), D+ (46), D− (48), VDD (38), and
  VDDH (41). For the proposed regulated normal-voltage supply, follow §8.1
  (VDD and VDDH connected together) and the selected §8.1.2 circuit.
- Transcribe the module-level USB supply conditioning and decoupling from
  Raytac §8.1.2; reconcile it with Nordic USBREG and reference circuitry.
  Check hot-plug, USB absent, switch off with USB attached, back-powering,
  source resistance, capacitor ratings, and the complete connector inrush
  budget. A 0 Ω measurement link does not provide electrical isolation when
  fitted or replace transient conditioning. See [power.md](power.md).
- Use the MDBT53-P drawings in the **241021** footprint/design-guide package
  and Ver.E §§2.1–2.3 for land pattern, orientation, and antenna keep-out on
  every layer. Keep the battery, magnets, and carrier clear of that region.
- Select the external 32.768 kHz crystal and its load configuration using
  Ver.E §8, the guide's crystal documents, and Nordic clock specifications;
  check the firmware clock setting against the actual selected crystal.
- Before schematic freeze, identify the protected pack and its manufacturer
  protection specifications, or complete the dedicated protection circuit for
  an unprotected cell. Check charge/discharge and peak-current ratings,
  protection trip/recovery behavior, temperature limits, and connector polarity
  against the chosen pack/cell documents. Do not infer these from the charger.
- LED part, pitch, peak current, current-setting resistor, brightness cap, NFC
  tuning, and exact outline remain dependent on prototype evidence. A build or
  clean ERC result does not validate any of them.

Versioned source links and document locations: [references.md](references.md).

## Design tools and rules

- KiCad 10.0.6. Project files and custom libraries live in
  [`hardware/`](../hardware/README.md).
- Fabrication outputs (Gerbers, drill, BOM, placement, STEP) are generated as
  CI or release artifacts, not committed.
- ERC/DRC violations are not suppressed without a documented rationale.
- Footprint pin numbering, polarity, and power connectivity are verified
  manually against datasheets before fabrication.
- Fabrication sign-off is a human decision.
