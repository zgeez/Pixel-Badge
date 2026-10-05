# Hardware

Status: **schematic capture in progress**. Rev A architecture is defined; the
KiCad project lives in [`hardware/`](../hardware/README.md). The regulators/
power-tree and USB/charger/battery sheets are drawn; the other sheets are not,
and no layout exists yet.
Items marked provisional may change. Electrical values come from primary datasheets and are recorded in
[references.md](references.md) as parts are confirmed.

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
- **NFC:** external passive tag (NXP NT3H2111W0FHKH, XQFN-8, NTAG I²C plus) so the
  link works with the badge off or the battery empty; on its own I²C bus with a
  firmware-switched supply. PCB coil about 30 × 25 mm, 6 turns, designed per NXP
  AN11276 to resonate slightly above 14.5 MHz, with capacitor footprints to tune
  it down once the battery, magnets and carrier are installed (not yet
  measured). Public RF reads and protected RF writes must coexist with MCU/I²C
  URL updates; provision per [nfc.md](nfc.md), not factory defaults. Test
  access on the tag's VCC, SDA, SCL and FD.
- **USB-C:** charging and USB 2.0 data (5 V sink, no USB Power Delivery), ESD
  protected, on an edge. VBUS feeds both the charger input and MDBT53-P1M
  VBUS pin 44, with the module USB supply circuit described in
  [power.md](power.md); D+ → pin 46, D− → pin 48.
  CC1 and CC2 each have their own 5.1 kΩ, 1% Rd to ground, present with the
  switch off/battery absent; no joined CC pins or USB PD controller.
- **Power:** protected 1-cell LiPo pack (Adafruit 328, 2500 mAh, genuine
  JST-PH 2-pin, integrated protection, no thermistor) with a BQ25620
  switching charger (≈ 1 A charge, BC1.2 detection, NVDC power path); a
  board NTC next to the pack qualifies charging temperature. TPS63802
  buck-boost logic rail (3.3 V) enabled by the power switch or, for a
  charging-only state, by USB presence; TPS61023 boost LED rail with load
  disconnect, enabled by firmware. See [power.md](power.md).
  Battery state comes from a default-off high-side-switched VBAT divider to
  nRF5340 SAADC, not the regulated logic rail; final sensing values and firmware
  thresholds await design calculations and pack characterization.
- **Runtime:** about eight hours under representative career-fair use is the
  design target, not a measured capability. Capacity alone does not establish
  runtime; validate the agreed usage profile in [validation.md](validation.md).
- **Sensors:** TDK ICM-42688-P 6-axis IMU on I²C (**conditional**: project I²C
  driver passes build and emulator tests; real-part I²C, interrupt and
  wake-on-motion checks pending), with SPI-fallback links; Knowles
  SPH0641LM4H-1 PDM microphone (lifecycle not yet confirmed).
- **User interface:** two edge-accessible buttons, edge-accessible power switch,
  one status LED.
- **RF:** module-integrated antenna at a board edge/corner with the keep-out the
  module vendor specifies; NFC coil in a separate region; battery and magnets
  kept clear of both.
- **Mechanical:** rear 3D-printed carrier for the battery and attachment magnets,
  mounted to the PCB; target total thickness ≈ 12 mm.

## Selected Rev A components

Selection is not validation: nothing below has been measured on Rev A.

| Function | Part | Status |
|---|---|---|
| MCU module | Raytac MDBT53-P1M | Selected |
| Charger / power path | TI BQ25620 (switching, I²C, BC1.2) | Selected; drawn; ≈ 1 A charge |
| Logic rail (3.3 V) | TI TPS63802, Murata DFE201612E-R47M (0.47 µH) | Selected; drawn |
| LED rail | TI TPS61023, Coilcraft XEL4030-102ME (1.0 µH) | Selected; VLED setting provisional |
| Column drivers | 2 × TI TLC59283RGER (QFN-24) | Selected; IREF/peak current prototype-dependent |
| Row control | 4 × SN74HC595, SN74LVC8T245 translator, 32 × AO3401A, SN74LVC1G123 + SN74LVC1G32 safety blanking | Selected |
| NFC tag | NXP NT3H2111W0FHKH (XQFN-8) | Selected; coil tuning prototype-dependent |
| IMU | TDK ICM-42688-P on I²C | Conditional (real-part verification pending) |
| Microphone | Knowles SPH0641LM4H-1 | Conditional (lifecycle check) |
| 32.768 kHz crystal | Micro Crystal CM7V-T1A, 9 pF | Selected |
| USB ESD | TI TPD4E05U06 (D+, D−, CC1, CC2) | Selected |
| Battery | Adafruit 328, protected 2500 mAh LiPo; JST B2B-PH-SM4-TB header | Selected; header polarity to verify |
| USB-C receptacle, buttons, power switch, matrix LED | — | Mechanical / prototype gates |

## Rev A requirements

- Independent battery protection covering overdischarge, overcurrent, and
  short circuit, with charge protection appropriate to the selected cell. The
  selected pack's integrated protection is intended to meet this (overcharge,
  over-discharge at 2.3 V per its protection IC, short circuit); its trip limits
  are not documented. The BQ25620 does not satisfy this requirement by itself;
  firmware low-battery cutoff is additional control.
- SWD access, reset access, and power test points (VBAT, VBUS, logic rail, LED
  rail, charger/system rail).
- UART test pads for fallback logging.
- Matrix timing test points (clock, latch, blank, row select).
- Removable links for current measurement where useful.
- Battery-voltage measurement and sense-node access, with an error budget and
  load-shed margin above regulator/protection cutoff; see [power.md](power.md).

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
- Check separate CC1/CC2 Rd connections, nominal/tolerance and total loading
  against USB-IF Release 2.5 §4.5.1.3.2 and Table 4-28. Review both orientations,
  dead-battery attachment and a source-compliant charger current policy.
- Review VBAT divider/switch, default-off behavior, AIN limits across supply
  states, source impedance/acquisition/settling, calibration and measurement
  access. Record proposed shed/recovery policy and worst-case margin before
  freeze; final measured thresholds may remain provisional. See power.md.
- The NTAG memory/protection map, bus, and provisioning sequence are defined in
  [nfc.md](nfc.md). Route field detect for NXP's RF/I²C errata handling.
  Irreversible locks are applied only on the verified event build; rejected
  RF-write tests are required.
- The project footprint `Raytac_MDBT53-P1M` is KiCad's conversion of Raytac's
  own library in the **241021** package (`MDBT53_230719.lbr`): 65 pads checked
  against the source, the vendor antenna keep-out added as a rule area on every
  copper layer, and pin names matching the Ver.E §2.5 table. Before layout,
  compare it visually with the Ver.E §§2.1–2.3 drawings. Keep the battery,
  magnets, and carrier clear of the keep-out.
- External 32.768 kHz crystal: Micro Crystal CM7V-T1A, CL 9 pF, ±20 ppm,
  RS ≤ 70 kΩ, which meets the package's recommended specification. The Rev A
  devicetree must set the nRF5340 internal load capacitance to 9 pF (the DK
  uses 7 pF); external load-capacitor footprints are fitted but not populated.
- Pack (Adafruit 328; PKCELL LIPO785060 specification): 1.2 A maximum charge
  (vendor), 1.5 A maximum continuous discharge, charging 0–45 °C, 3.0 V cell
  cut-off, protection IC over-discharge at 2.3 V. 50.5 × 62.5 × 8.1 mm max and
  50 g: keep rear components out from under it, and keep the NFC coil, module
  antenna and magnets clear. The pack drawing labels its plug "reverse":
  verify the header polarity against the pack before layout.
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
