# smart-led-badge

A wearable smart identity/networking badge with a 32×32 monochrome white LED
matrix, built on the Nordic nRF5340 with Zephyr RTOS. It shows a scannable QR
link and name/identity, runs a motion-driven fluid simulation and an audio
visualization, is configured over BLE from a companion phone app, and carries a
passive NFC tag that shares the same link even when the badge is switched off.

**Status:** early development. Rev A scope is defined; firmware is a minimal
bootstrap and all schematic sheets are drawn (review and freeze pending; no
layout yet). No performance or battery figures have been measured.

**Build status:** _CI status badge to be added once the CI workflow has run on
`main`._

## Planned features

Rev A scope:

- QR link display (Version 1 fits the 32×32 display with a full quiet zone;
  additional encoder versions only where the output geometry permits)
- Name / identity display
- Motion-driven Flip/fluid-style simulation (IMU)
- Audio visualization (PDM microphone; processed on the badge, no audio stored)
- BLE configuration through a companion phone app; usable without a phone
  after configuration
- Passive NFC contact link that works with the badge switched off
- Battery-powered custom PCB with USB-C charging, two buttons, power switch,
  and debug access

## Stack

| | |
|---|---|
| MCU | Nordic nRF5340: nRF5340 DK for development; Raytac MDBT53-P1M module on the badge |
| Firmware | nRF Connect SDK v3.4.1 / Zephyr RTOS, C++17 |
| Tests | Ztest, Twister (`native_sim`) |
| Hardware | KiCad 10.0.6, 4-layer PCB (≤ 112 × 74 mm), JLCPCB assembly (provisional) |
| CI | GitHub Actions with the nRF Connect SDK toolchain container |

## Repository layout

```
firmware/    Zephyr application (src/, include/, boards/, tests/)
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
`firmware/boards/smart_led_badge/`, which is modified from Zephyr's
`raytac_mdbt53_db_40` board and stays under
[Apache-2.0](LICENSES/Apache-2.0.txt) with its upstream copyright notices.
