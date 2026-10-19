# smart-led-badge

A wearable smart identity/networking badge with a 32×32 monochrome white LED
matrix, built on Nordic nRF5 SoCs with Zephyr RTOS. It shows a scannable QR
link and name/identity, runs a motion-driven fluid simulation and an audio
visualization, is configured over BLE from a companion phone app, and carries a
passive NFC tag that shares the same link even when the badge is switched off.

**Status:** the nRF5340 DK prototype implements QR/text, BLE configuration,
synchronized passive NFC, 128-frame grayscale artwork, microphone spectrum/level
visualization, encrypted phone recording and IMU fluid/orientation. Pixel Badge
is the Android-first Flutter companion app. Recording is enabled only by an
explicit development fragment pending physical acceptance.

The firmware has separate [prototype and Rev A applications](firmware/README.md)
with shared services. The E73/nRF52840 Rev A port builds the combined application/
controller and matrix backend; charger/power policy and physical bring-up remain.
Final prototype physical checks are deferred to board bring-up at the owner's
request. Software tests and successful builds do not establish electrical,
optical, RF, battery-safety or complete product acceptance.

See [firmware](docs/firmware.md), [Rev A implementation](docs/rev-a-firmware.md),
[app setup](app/README.md) and [validation](docs/validation.md) for scope/evidence.

## Planned features

Rev A scope:

- QR link display (Version 1 fits the 32×32 display with a full quiet zone;
  additional encoder versions only where the output geometry permits)
- Name / identity display
- Motion-driven Flip/fluid-style simulation (IMU)
- Audio visualization and voice recording (PDM microphone). Recordings stream
  to the phone app for playback and are never stored on the badge; the matrix
  shows a recording indicator for the whole recording
- BLE configuration through a companion phone app; usable without a phone
  after configuration
- Passive NFC contact link that works with the badge switched off
- Battery-powered custom PCB with USB-C charging, two buttons, power switch,
  and debug access

## Stack

| | |
|---|---|
| MCU | Nordic nRF52840 on the badge (Ebyte E73-2G4M08S1C module, [ADR 0015](docs/decisions/0015-nrf52840-e73-module.md)); separate nRF5340 DK prototype and [Rev A port](docs/rev-a-firmware.md) |
| Firmware | nRF Connect SDK v3.4.1 / Zephyr RTOS, C++17 |
| Tests | Ztest, Twister (`native_sim`) |
| Hardware | KiCad 10.0.6, 4-layer PCB (≤ 118 × 76 mm), JLCPCB assembly (provisional) |
| CI | GitHub Actions with the nRF Connect SDK toolchain container |

## Repository layout

```
firmware/    Zephyr application (src/, include/, boards/, tests/)
app/         Android-first Flutter BLE configuration app
hardware/    KiCad project, libraries, mechanical files
docs/        Architecture, firmware, hardware, validation, bring-up, ADRs
scripts/     Developer (dev.ps1) and CI helper scripts
.github/     CI workflows, issue and pull request templates
```

## Building

See [docs/firmware.md](docs/firmware.md) for prerequisites.

| Task | Windows (PowerShell) | Linux / WSL |
|---|---|---|
| Build firmware | `.\scripts\dev.ps1 build` | `make build` |
| Run tests | `.\scripts\dev.ps1 test` (Docker) | `make test` |
| Format sources | `.\scripts\dev.ps1 format` | `make format` |
| Run checks | `.\scripts\dev.ps1 check` | `make check` |
| Schematic ERC, pin/pad check, PDF/BOM (KiCad 10.0.6) | `.\scripts\dev.ps1 hardware` | `make hardware` |

## Documentation

Start with [docs/architecture.md](docs/architecture.md).

## License

[MIT](LICENSE), except the Rev A board definition in
`firmware/nrf5340_prototype/boards/smart_led_badge/`, which is modified from Zephyr's
`raytac_mdbt53_db_40` board and stays under
[Apache-2.0](LICENSES/Apache-2.0.txt) with its upstream copyright notices.
