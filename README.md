# smart-led-badge

A wearable smart badge with a 32×32 monochrome LED matrix, built on the Nordic
nRF5340 with Zephyr RTOS. The badge is intended to show a scannable QR link,
name/identity, and interactive visualizations driven by motion and sound, with
BLE and NFC connectivity and a custom battery-powered PCB.

**Status:** early development. Firmware is a minimal bootstrap; no hardware
design exists yet. No performance or battery figures have been measured.

**Build status:** _CI status badge to be added once the CI workflow has run on
`main`._

## Planned features

- QR link display
- Name / identity display
- IMU-driven interactive visualization
- Audio spectrum visualization
- BLE and NFC configuration/interaction
- Custom battery-powered PCB with debug access

## Stack

| | |
|---|---|
| MCU | Nordic nRF5340 (nRF5340 DK for development; module on final PCB, TBD) |
| Firmware | nRF Connect SDK v3.4.1 / Zephyr RTOS, C++17 |
| Tests | Ztest, Twister (`native_sim`) |
| Hardware | KiCad 10.0.6, 4-layer PCB (provisional), JLCPCB (provisional) |
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

## Documentation

Start with [docs/architecture.md](docs/architecture.md).

## License

[MIT](LICENSE)
