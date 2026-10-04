# Hardware

Status: **pre-design**. No schematic or layout exists yet. Every item below is
provisional unless stated otherwise. Electrical values will be taken from
primary datasheets and recorded in [references.md](references.md) when the
design begins.

## Development vs final hardware

| | Development | Final (Rev A) |
|---|---|---|
| MCU | nRF5340 DK | nRF5340 radio module on custom PCB (module TBD) |
| Debug | On-board debugger (SWD, RTT) | SWD connector/pads, UART test pads |
| Display | Off-the-shelf prototype panel (below) | 32×32 LED matrix on the badge PCB |
| Power | USB / bench supply | 1-cell LiPo (power architecture TBD) |

Firmware written for the DK must not assume DK-specific pins or peripherals
remain available on the final board; board differences belong in devicetree
overlays or a custom board definition.

## Optical prototype

Before freezing matrix geometry, QR readability at 32×32 and 2.0 mm pitch is
being evaluated on a prototype built from:

- nRF5340 DK
- 64×64 RGB HUB75 panel, 2.0 mm pitch, 1/32 scan (Waveshare)
- SN74AHCT245 level translators
- LSM6DSO IMU breakout (SparkFun)
- PDM microphone breakout (Adafruit)
- Bench power supply

The prototype panel is used only for optical and firmware evaluation; its drive
scheme is not the planned badge design.

## Provisional target architecture

- **Matrix:** 32×32 monochrome white LEDs, 2.0 mm pitch (provisional, pending
  QR validation). LED part, drive circuit, and scan scheme TBD.
- **Sensors:** IMU and PDM microphone. Final parts TBD.
- **Wireless:** BLE (2.4 GHz antenna per selected module), NFC (antenna design
  TBD).
- **Power:** 1-cell LiPo; charger, regulation, protection, and fuel gauging
  TBD.
- **PCB:** 4-layer provisional, exposed-PCB aesthetic, designed for JLCPCB
  fabrication and assembly where practical (provisional manufacturer).
- **Mechanical:** magnetic wearable mounting (TBD).

## Rev A requirements

- SWD access and power test points.
- UART test pads for fallback logging.
- Recovery paths preserved; a major second revision may not fit the schedule.

## Design tools and rules

- KiCad 10.0.6. Project files and custom libraries live in
  [`hardware/`](../hardware/README.md).
- Fabrication outputs (Gerbers, drill, BOM, placement, STEP) are generated as
  CI or release artifacts, not committed.
- ERC/DRC violations are not suppressed without a documented rationale.
- Footprint pin numbering, polarity, and power connectivity are verified
  manually against datasheets before fabrication.
- Fabrication sign-off is a human decision.
