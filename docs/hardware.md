# Hardware

Status: **schematic frozen for layout** (review 2026-10-07). Rev A architecture
is defined; the KiCad project lives in [`hardware/`](../hardware/README.md).
Changes after the freeze go through a recorded decision. Layout is in
progress (placement and LED matrix routing); nothing has been built or
measured.
Items marked provisional may change. Electrical values come from primary datasheets and are recorded in
[references.md](references.md) as parts are confirmed.

## Development vs final hardware

| | Development | Rev A badge |
|---|---|---|
| MCU | nRF5340 DK (prototype firmware) | Ebyte E73-2G4M08S1C module (nRF52840), [ADR 0015](decisions/0015-nrf52840-e73-module.md) |
| Debug | On-board debugger (SWD, RTT) | SWD header (RTT), test points; no UART or spare GPIO pads |
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

- **Size:** 108 × 72 mm nominal, **112 × 74 mm maximum**; Rev A uses the
  112 × 74 mm maximum; rounded corners; 4-layer PCB; black soldermask.
  Single-sided assembly ([ADR 0010](decisions/0010-single-sided-assembly.md)):
  the LEDs and all other components are on the front; the rear holds only
  the battery, its connector body and the carrier.
- **Matrix:** 32×32 white SMD LEDs, 2.0 mm pitch (provisional, pending QR
  validation), 1:32 multiplexing (provisional). Two TI TLC59283 constant-current
  column drivers and high-side row switches (provisional); refresh timed by nRF52840
  peripherals; column current set by R_IREF to 18.0 mA nominal (up to about
  19.4 mA with driver and resistor tolerance at 25 °C, the figure the battery
  limit uses) and adjustable downward only. If the scan stops, a retriggerable
  one-shot blanks the columns and disables the rows without the CPU, after
  0.76–1.41 ms by calculation (to be measured).
- **NFC:** external passive tag (NXP NT3H2111W0FHKH, XQFN-8, NTAG I²C plus) so the
  link works with the badge off or the battery empty; on its own I²C bus with a
  firmware-switched supply. PCB coil about 30 × 25 mm, 6 turns, designed per NXP
  AN11276 to resonate slightly above 14.5 MHz, with capacitor footprints to tune
  it down once the battery, magnets and carrier are installed (not yet
  measured). Public RF reads and protected RF writes must coexist with MCU/I²C
  URL updates; provision per [nfc.md](nfc.md), not factory defaults. Test
  access on the tag's VCC, SDA, SCL and FD.
- **USB-C:** charging and USB 2.0 data (5 V sink, no USB Power Delivery), ESD
  protected, on an edge. VBUS feeds both the charger input and the E73
  VBUS pin 27, with the module USB supply circuit described in
  [power.md](power.md); D+ → pin 31, D− → pin 29.
  CC1 and CC2 each have their own 5.1 kΩ, 1% Rd to ground, present with the
  switch off/battery absent; no joined CC pins or USB PD controller.
- **Power:** protected 1-cell LiPo pack (Adafruit 328, 2500 mAh, genuine
  JST-PH 2-pin, integrated protection, no thermistor) with a BQ25620
  switching charger (≈ 1 A charge, BC1.2 detection, NVDC power path); a
  board NTC on the front, at the edge over the pack, qualifies charging
  temperature (it measures board temperature, not cell temperature). TPS63802
  buck-boost logic rail (3.3 V) enabled by the power switch or, for a
  charging-only state, by USB presence; TPS61023 boost LED rail with load
  disconnect, enabled by firmware. See [power.md](power.md).
  Battery state comes from a default-off high-side-switched VBAT divider to
  nRF52840 SAADC, not the regulated logic rail; final sensing values and firmware
  thresholds await design calculations and pack characterization.
- **Runtime:** about eight hours under representative career-fair use is the
  design target, not a measured capability. Capacity alone does not establish
  runtime; validate the agreed usage profile in [validation.md](validation.md).
- **Sensors:** TDK ICM-42688-P 6-axis IMU on I²C (**conditional**: project I²C
  driver passes build and emulator tests; real-part I²C, interrupt and
  wake-on-motion checks pending), with SPI-fallback links; Knowles
  SPH0641LM4H-1 PDM microphone (lifecycle not yet confirmed).
- **User interface:** worn landscape or portrait, with the displayed content
  rotated by firmware from the IMU's orientation
  ([ADR 0009](decisions/0009-dual-orientation-dual-mounting.md)); two
  top-actuated buttons on the front below the LED field (landscape layout)
  when they fit, otherwise on the edge below the field
  ([ADR 0008](decisions/0008-landscape-front-buttons.md)), marked with
  orientation-independent symbols; power switch on an edge; one status LED
  on the front.
- **RF:** module-integrated antenna at a board edge/corner with the keep-out the
  module vendor specifies; NFC coil in a separate region; battery and magnets
  kept clear of both.
- **Mechanical:** rear 3D-printed carrier, mounted to the PCB, holding the
  battery and the attachment magnets and taking the lanyard load through
  attachment points for landscape and portrait hanging
  ([ADR 0009](decisions/0009-dual-orientation-dual-mounting.md)), with a vent
  channel for the microphone's PCB acoustic hole. Nothing on the rear besides
  the battery and the connector body: no parts, test points, exposed pads or
  protruding leads under the pack (inner copper and tented vias only), and an
  insulating layer between board and pack. Thickness targets (calculated,
  not measured): ≈ 12 mm over the LED field (11.4–12.0 mm), ≈ 13.5 mm over the
  electronics region (12.1–13.5 mm), local edge exceptions ≈ 14.5 mm at USB-C
  and ≈ 16.5 mm at the SWD header
  ([ADR 0010](decisions/0010-single-sided-assembly.md)).

## Selected Rev A components

Selection is not validation: nothing below has been measured on Rev A.

| Function | Part | Status |
|---|---|---|
| MCU module | Ebyte E73-2G4M08S1C (nRF52840, ceramic antenna, 13 × 18 × 3.0 mm), normal voltage mode; DC/DC inductors, 32 MHz crystal and DECx/DECUSB capacitors inside the module (Ebyte §5.1) | Selected; drawn ([ADR 0015](decisions/0015-nrf52840-e73-module.md)) |
| Charger / power path | TI BQ25620 (switching, I²C, BC1.2) | Selected; drawn; ≈ 1 A charge |
| Logic rail (3.3 V) | TI TPS63802, Murata DFE201612E-R47M (0.47 µH) | Selected; drawn |
| LED rail | TI TPS61023, Coilcraft XGL4020-102MEC (1.0 µH, 2.1 mm) | Selected; VLED setting provisional |
| Column drivers | 2 × TI TLC59283RGER (QFN-24), R_IREF 2.94 kΩ + series 0 Ω option | Selected; drawn; 18.0 mA nominal setting (about 19.4 mA worst case at 25 °C); peak current prototype-dependent |
| Row control | 4 × SN74HC595 (OE pulled up to VLED), SN74LVC8T245 translator, 32 × AO3401A with 10 kΩ gate pull-ups, SN74LVC1G123 + SN74LVC1G04 + SN74LVC1G32 scan-stall blanking | Selected; drawn |
| NFC tag | NXP NT3H2111W0FHKH (XQFN-8) | Selected; drawn; footprint per NXP's reflow footprint; coil tuning prototype-dependent |
| IMU | TDK ICM-42688-P on I²C | Conditional (real-part verification pending); drawn |
| Microphone | Knowles SPH0641LM4H-1, 0.8 mm PCB acoustic hole | Conditional (lifecycle check); drawn |
| 32.768 kHz crystal | Micro Crystal CM7V-T1A, 9 pF, with 2 × 12 pF C0G external loads | Selected; drawn; load value calculated, not measured |
| SWD header | ARM Cortex 10-pin, 2 × 5, 1.27 mm SMD (hanxia HX PZ1.27-2x5P TP), for the nRF5340 DK debug out | Selected; drawn; footprint checked against the vendor land pattern |
| USB ESD | TI TPD4E05U06 (D+, D−, CC1, CC2) | Selected |
| Battery | Adafruit 328, protected 2500 mAh LiPo; JST S2B-PH-K-S(LF)(SN) through-hole side-entry header, inserted from the rear and hand-soldered from the front by default | Selected; header pin 1 = BAT−, pin 2 = BAT+ (Adafruit convention) |
| USB-C receptacle | HRO TYPE-C-31-M-12 (16-pin USB 2.0) | Selected; drawn |
| Buttons | 2 × XUNPU TS-1088-AR02016 (top-actuated, 2.0 mm) | Selected; drawn; placement per ADR 0008 |
| Power switch | C&K PCM12SMTR (right-angle SPDT slide) | Selected; drawn |
| Status LED | Red 0603 (KT-0603R) with 1 kΩ | Selected; drawn |
| Matrix LED | 1024 × KT-0603W (0603 white), 2.0 mm pitch | Selected on the manufacturer datasheet; drawn. Its absolute maximum ratings cover the 18–19.4 mA peak at 1/32 duty. Forward voltage is specified only at 5 mA (higher-current curves are typical), so LED headroom at the actual column current, minimum VLED and low temperature is a Rev A qualification gate ([validation](validation.md#3-matrix-timing-flicker-and-ghosting)). Brightness and bin uniformity are measured on Rev A; the 2.0 mm pitch is accepted without the planned QR screening and verified on Rev A |

## Rev A requirements

- Independent battery protection covering overdischarge, overcurrent, and
  short circuit, with charge protection appropriate to the selected cell. The
  selected pack's integrated protection is intended to meet this (overcharge,
  over-discharge at 2.3 V per its protection IC, short circuit); its trip limits
  are not documented. The BQ25620 does not satisfy this requirement by itself;
  firmware low-battery cutoff is additional control.
- SWD access, reset access, and power test points (VBAT, VBUS, logic rail, LED
  rail, charger/system rail).
- No UART or spare GPIO pads: all module GPIO are used; logging is over RTT.
- Matrix timing test points (clock, latch, blank, row select).
- Removable links for current measurement where useful.
- Battery-voltage measurement and sense-node access, with an error budget and
  load-shed margin above regulator/protection cutoff; see [power.md](power.md).

## Schematic preparation and review

- The module symbol pin → footprint pad mapping follows the Ebyte E73-2G4M08S1C
  pin table, including VCC (19), VDDH (23), DCCH (25, open), VBUS (27),
  D− (29) and D+ (31). Normal voltage mode: VCC and VDDH both on 3V3.
- The module-level USB supply conditioning (R401, C402) was taken from
  Raytac §8.1.2 for the MDBT53 and kept for the E73; reconcile it with the
  nRF52840 USBREG specification and Ebyte's §5.1 reference circuit.
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
- The project footprint `Ebyte_E73-2G4M08S1C` is drawn from the Ebyte §3
  drawing (bottom view mirrored to the top view): 25 castellated pads at
  1.27 mm and 15 inner 0.8 mm pads, every centre checked against the drawing's
  dimensions; castellated lands extend 0.3 mm beyond the body. Ebyte gives no
  antenna keep-out drawing; the project keep-out covers every copper layer at
  the antenna end, which faces the bottom board edge. Keep the battery,
  magnets, and carrier clear of it.
- External 32.768 kHz crystal: Micro Crystal CM7V-T1A, CL 9 pF, ±20 ppm,
  RS ≤ 70 kΩ. The nRF52840 has no internal LFXO load capacitance, so C405/C406
  are fitted with 12 pF C0G (2 × 9 pF less the pin and board stray, a
  calculation still to be checked against the Product Specification's pin
  capacitance and measured at bring-up).
- Module supply: VCC = VDDH = 3V3 with 4.7 µF (C401) and module VBUS through
  2.2 Ω with 4.7 µF (R401, C402). The DC/DC inductors are inside the E73, so
  the board has none. USB D+/D− reach the module through 0 Ω links, which can
  take series resistors if USB testing calls for it.
- Debugging uses the nRF5340 DK's debug out (P19) through a 10-pin 1.27 mm
  flat cable; the badge grounds pin 3, which selects the external target.
  Nordic requires the target voltage to match the DK, and a DK on USB alone
  runs at 3 V. For the 3.3 V badge, the DK's own nRF5340 is supplied with 3.3 V on
  P21 (SW10 VEXT->nRF on, SW9 at VDD; 1.7–3.6 V allowed), which also sets
  the DK's voltage follower, with USB still connected for the debugger. P19
  pin 1 only supplies power when DK solder bridge SB47 is shorted; it stays
  open, because the badge drives its own 3.3 V on that pin.
- Pack (Adafruit 328; PKCELL LIPO785060 specification): 1.2 A maximum charge
  (vendor), 1.5 A maximum continuous discharge, charging 0–45 °C, 3.0 V cell
  cut-off, protection IC over-discharge at 2.3 V. 50.5 × 62.5 × 8.1 mm max and
  50 g: keep the NFC coil, module antenna and magnets clear of it. The
  header (JST S2B-PH-K-S, through-hole side entry, 4.8 mm tall) has its body
  on the rear next to the pack's lead exit and its pins soldered from the
  front outside the LED field; it is hand-soldered after assembly unless the
  assembler confirms rear insertion. Pin 1 = BAT−, pin 2 = BAT+
  as on Adafruit's charger boards; mark + and − on the silkscreen and meter-check each pack
  before its first connection.
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
