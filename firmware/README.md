# Firmware targets

| Application | Hardware | Build |
|---|---|---|
| [nrf5340_prototype](nrf5340_prototype/README.md) | nRF5340 DK and external HUB75/NTAG/IMU/microphone | `./scripts/dev.ps1 build -Target Prototype -Pristine` |
| [rev_a_nrf52840](rev_a_nrf52840/README.md) | Rev A E73-2G4M08S1C / nRF52840 | `./scripts/dev.ps1 build -Target RevA -Pristine` |

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
