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
| Matrix drive | 2 × TLC59283 constant-current column sinks; SN74HC595 row registers behind an SN74LVC8T245 translator driving AO3401A high-side row switches; refresh timed by hardware peripherals; monostable + OR gate force blanking if the scan stalls | Parts selected; peak current and IREF prototype-dependent |
| BLE | Module antenna at a board edge/corner; configuration via companion app | Selected |
| NFC | External passive NFC tag (NXP NT3H2111W0FHKH, XQFN-8) with PCB coil on its own switched I²C bus; public RF reads, protected RF writes, MCU I²C updates | Part selected; coil tuning and provisioning bytes provisional |
| Sensors | TDK ICM-42688-P 6-axis IMU on I²C with interrupts (project I²C driver; SPI fallback provided); Knowles SPH0641LM4H-1 PDM microphone | IMU conditional on real-part verification; microphone conditional on lifecycle check |
| Power | Protected 1-cell LiPo pack (Adafruit 328, 2500 mAh); BQ25620 switching USB-C charger with NVDC power path (≈ 1 A charge, BC1.2 detection) and separate CC1/CC2 Rd; dedicated module USB VBUS supply; TPS63802 3.3 V logic rail powered by the switch or by USB (charging-only mode); switched VBAT-to-SAADC sensing; TPS61023 firmware-switchable LED rail with low-battery load shedding | Parts selected; charging works with switch off; thresholds and VLED provisional |
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

Rev A uses a commercially protected 1-cell LiPo pack (Adafruit 328, 2500 mAh).
Charger/power-path functions and firmware load shedding do not replace its
independent overdischarge, overcurrent and short-circuit protection. The cell
allows up to 1.2 A charge (vendor) and 1.5 A continuous discharge; the pack
has no thermistor, so a board NTC next to it qualifies charging. No safety or
runtime measurements exist. See [power.md](power.md#independent-battery-protection).

The power switch is a signal-level control: with USB attached the logic rail
runs a charging-only firmware state even when the switch is OFF. See
[power.md](power.md#power-states).

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
- NFC coil tuning and provisioning bytes (verified on a development tag; see
  [nfc.md](nfc.md)).
- IMU: ICM-42688-P on I²C until real-part verification; SPI is the fallback.
- Microphone lifecycle confirmation.
- USB-C receptacle, buttons and power switch (mechanical).
- Charging temperature qualification with a pack that has no thermistor.

The owner-selected Rev A architecture is recorded in
[decisions/](decisions/README.md) as Accepted ADRs. Design selection does not
establish hardware validation.

See also: [firmware.md](firmware.md), [hardware.md](hardware.md),
[power.md](power.md).
