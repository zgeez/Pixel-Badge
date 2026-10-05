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
    NFC["NTAG I²C plus<br/>passive, EEPROM NDEF"] <-->|I²C| APP
    COIL["NFC coil"] --- NFC
    MIC["PDM microphone"] -->|PDM| APP
    UI["2 buttons, status LED"] --- APP

    USB["USB-C<br/>charge + USB 2.0"] -->|VBUS| CHG["Power-path charger"]
    USB -->|CC1| RD1["5.1 kΩ Rd to GND"]
    USB -->|CC2| RD2["5.1 kΩ Rd to GND"]
    USB -->|VBUS| USBPWR["Module USB supply<br/>conditioning + decoupling"]
    USBPWR --> VBUS["Dedicated VBUS input<br/>module pin 44"]
    VBUS --> MOD
    USB -->|D+/D−| APP
    BATT["Protected 1-cell LiPo pack<br/>or dedicated hardware protection"] --- CHG
    BATT --> SENSE["Default-off switched<br/>VBAT divider"]
    SENSE -->|SAADC| APP
    APP -->|Measurement enable| SENSE
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
| Display | 32×32 white SMD LEDs, binary pixels, discrete global brightness levels | Pitch 2.0 mm and 1:32 multiplexing provisional pending prototype testing |
| Matrix drive | Constant-current column sinks, high-side row switches, refresh timed by hardware peripherals; matrix forced off if the scan stalls | Parts provisional |
| BLE | Module antenna at a board edge/corner; configuration via companion app | Selected |
| NFC | External passive NFC tag (NTAG I²C plus family) with PCB coil; public RF reads, protected RF writes, MCU I²C updates | Architecture selected; exact part/provisioning map provisional |
| Sensors | 6-axis IMU (I²C, interrupts); PDM microphone | Parts provisional |
| Power | Independently protected 1-cell LiPo; USB-C charging with power path and separate CC1/CC2 Rd; dedicated module USB VBUS supply; regulated logic rail; switched VBAT-to-SAADC sensing; firmware-switchable LED rail with low-battery load shedding | Charging works with switch off; protection/sensing required; parts and thresholds provisional |
| User interface | Two buttons (Mode/Next, Action/Select), physical power switch, one status LED | Selected |
| Debug | SWD, SEGGER RTT logging, UART test pads, rail and matrix-timing test points | Required on Rev A |
| Mechanics | 4-layer PCB ≤ 112 × 74 mm (108 × 72 mm nominal), black soldermask, LEDs on the front, rear 3D-printed carrier with battery and magnetic attachment | Outline provisional within the maximum |

## Engineering priorities

In order: QR scan reliability, PCB electrical correctness, matrix timing and
brightness, firmware reliability, battery/power behavior, BLE/NFC usability,
mechanical wearability, visual polish.

## USB and battery requirements

Terminate CC1 and CC2 separately to ground with two 5.1 kΩ, 1% Rd resistors,
present even when switched off or without a battery. Do not join the CC pins.
This provides sink attachment in either orientation without USB PD.

USB-C VBUS supplies **both** the charger input and the module's dedicated VBUS
input (pin 44). D+ and D− connect to module pins 46 and 48. USB also requires
the normal device supply; VBUS does not replace the regulated logic rail.
Connection, decoupling, conditioning, and off-state review are specified in
[power.md](power.md), using the Nordic USBREG specification and Raytac Ver.E
§§2.5 and 8.1.2.

Use a commercially protected 1-cell LiPo pack for Rev A. An unprotected cell
requires a dedicated hardware protection circuit instead. Charger/power-path
functions and firmware load shedding do not replace independent battery
overdischarge, overcurrent, and short-circuit protection. Pack selection and
protection ratings remain open; no safety or runtime measurements exist.

Measure the protected pack's VBAT output through a default-off switched divider
to an nRF5340 SAADC input. The regulated logic voltage is not battery state.
Rev A uses voltage-based load shedding; divider values, acquisition timing,
error margin, shed/recovery thresholds and discharge characterization remain
provisional. See [power.md](power.md#battery-state-measurement).

## NFC content protection

The contact URL is public, including with all badge power removed. Provision
the tag to reject unauthenticated RF writes while retaining MCU/I²C EEPROM
updates. Factory-default NTAG protection is insufficient. The intended policy,
part-specific provisioning gate and recovery requirements are in
[nfc.md](nfc.md); tests cover powered and passive reads, rejected RF writes,
and subsequent authorized MCU updates.

## Open decisions

- LED matrix pitch, LED part, and drive current (after QR testing).
- Exact board outline within the size limit.
- Battery cell.
- Exact NFC tag, IMU, and microphone parts.

The owner-selected Rev A architecture is recorded in
[decisions/](decisions/README.md) as Accepted ADRs. Design selection does not
establish hardware validation.

See also: [firmware.md](firmware.md), [hardware.md](hardware.md),
[power.md](power.md).
