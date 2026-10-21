# Firmware targets

There are three programs, each built and flashed by name:

| Program | Hardware | Purpose | Build, then flash |
|---|---|---|---|
| Prototype | nRF5340 DK and external HUB75/NTAG/IMU/microphone | The working prototype with BLE ([nrf5340_prototype](nrf5340_prototype/README.md)) | `./scripts/dev.ps1 build -Image Prototype -Pristine`, `./scripts/dev.ps1 flash -Image Prototype` |
| Diagnostics | Rev A E73-2G4M08S1C / nRF52840 | Bring-up checks, one peripheral at a time; no BLE ([rev_a_nrf52840](rev_a_nrf52840/README.md)) | `./scripts/dev.ps1 build -Image Diagnostics -Pristine`, `./scripts/dev.ps1 flash -Image Diagnostics` |
| Main | Rev A E73-2G4M08S1C / nRF52840 | The badge firmware with every service ([rev_a_nrf52840](rev_a_nrf52840/README.md)) | `./scripts/dev.ps1 build -Image Main -Pristine`, `./scripts/dev.ps1 flash -Image Main` |

Diagnostics and Main are two configurations of the same Rev A application.
Bench add-ons stay separate opt-in builds, added with `-ExtraConfFile` and their
own `-BuildDir`: the matrix stepper and the watched first charge on Diagnostics,
the matrix and recording bench on Main, and recording on Prototype. LED output
and charging therefore always need a deliberate build. `flash` checks that the
probe sees the program's chip (the DK's nRF5340 for Prototype, an nRF52840 for
Rev A) and that the build matches the program.

An optional [Pro Micro headless bench](bench/promicro_nrf52840/README.md) runs
shared BLE/settings/rendering on a bare nRF52840 board with a USB pairing display.
It has its own factory-bootloader layout and is not a Rev A hardware image.

Each target has its own CMake/Kconfig/sysbuild root, board definitions and flash
layout. `common/` contains shared application/services, headers, libraries and
project drivers. `tests/` contains the shared native tests. There is no buildable
application at the `firmware/` directory itself. Pixel Badge in `app/` uses the
same BLE protocol for both targets.

The earlier `smart_led_badge/nrf5340/*` custom board is retained under the
prototype target for regression builds. It describes superseded hardware.
Use `pixel_badge_reva/nrf52840` for the actual Rev A board.

See [Rev A implementation and remaining work](../docs/rev-a-firmware.md).
