# System architecture

Status: **Rev A architecture defined; nothing validated on hardware yet.**
Items marked provisional or TBD may change; see [validation.md](validation.md)
for how they will be tested.

## Overview

The badge is a battery-powered wearable identity/networking badge built around a
Nordic nRF5340 radio module. It drives a 32×32 monochrome white LED matrix and
offers these Rev A modes and functions:

- QR link display and name/identity display
- Motion-driven Flip/fluid-style simulation (IMU input)
- Audio visualization (PDM microphone, processed locally)
- BLE configuration from a companion phone app; the badge works without a phone
  once configured
- A passive NFC tag that opens the same link as the QR code, including when the
  badge is switched off or the battery is empty

Development uses the nRF5340 DK; the badge uses a Raytac MDBT53-P1M module.

## Subsystems

```mermaid
flowchart LR
    subgraph MOD["MDBT53-P1M (nRF5340)"]
        APP["Application core<br/>firmware"]
        NET["Network core<br/>BLE controller"]
        APP <-->|IPC| NET
    end
    NET --- ANT["Module PCB antenna"]

    APP -->|"SPI, hardware-timed"| DRV["Column + row drivers"]
    DRV --> MATRIX["32×32 LED matrix<br/>(2.0 mm pitch, provisional)"]
    IMU["IMU"] -->|I²C| APP
    NFC["NFC tag<br/>(passive, I²C)"] -->|I²C| APP
    COIL["NFC coil"] --- NFC
    MIC["PDM microphone"] -->|PDM| APP
    UI["2 buttons, status LED"] --- APP

    USB["USB-C<br/>charge + USB 2.0"] --> CHG["Power-path charger"]
    USB -->|D+/D−| APP
    BATT["1-cell LiPo"] --- CHG
    CHG --> LOGIC["Regulated logic rail"]
    CHG --> LED["Switchable LED rail"]
    SW["Power switch"] --> LOGIC
    LOGIC --> MOD
    LED --> DRV

    DBG["SWD, RTT, UART pads"] --- APP
```

| Subsystem | Rev A design | Status |
|---|---|---|
| MCU | Raytac MDBT53-P1M (nRF5340), external 32.768 kHz crystal | Selected |
| Display | 32×32 white SMD LEDs, 1:32 multiplexing, binary pixels, discrete global brightness levels | Pitch 2.0 mm provisional pending QR testing |
| Matrix drive | Constant-current column sinks, high-side row switches, refresh timed by hardware peripherals; matrix forced off if the scan stalls | Parts provisional |
| BLE | Module antenna at a board edge/corner; configuration via companion app | Selected |
| NFC | External passive NFC tag (NTAG I²C plus family) with PCB coil; MCU updates contents over I²C when powered | Architecture selected; exact part provisional |
| Sensors | 6-axis IMU (I²C, interrupts); PDM microphone | Parts provisional |
| Power | USB-C charging with power path, charging works with the switch off; regulated logic rail; LED rail switchable by firmware; matrix load shed on low battery | Parts provisional |
| User interface | Two buttons (Mode/Next, Action/Select), physical power switch, one status LED | Selected |
| Debug | SWD, SEGGER RTT logging, UART test pads, rail and matrix-timing test points | Required on Rev A |
| Mechanics | 4-layer PCB ≤ 112 × 74 mm (108 × 72 mm nominal), black soldermask, LEDs on the front, rear 3D-printed carrier with battery and magnetic attachment | Outline provisional within the maximum |

## Engineering priorities

In order: QR scan reliability, PCB electrical correctness, matrix timing and
brightness, firmware reliability, battery/power behavior, BLE/NFC usability,
mechanical wearability, visual polish.

## Open decisions

- LED matrix pitch, LED part, and drive current (after QR testing).
- Exact board outline within the size limit.
- Battery cell.
- Exact NFC tag, IMU, and microphone parts.

Accepted decisions are recorded as ADRs in [decisions/](decisions/README.md).

See also: [firmware.md](firmware.md), [hardware.md](hardware.md).
