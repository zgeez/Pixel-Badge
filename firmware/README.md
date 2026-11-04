# Firmware

Three images, each built and flashed by name:

| Image | Hardware | Source |
|---|---|---|
| Main | Rev A (nRF52840) | [rev_a_nrf52840](rev_a_nrf52840/README.md) |
| Diagnostics | Rev A, peripheral checks without BLE | [rev_a_nrf52840](rev_a_nrf52840/README.md) |
| Prototype | nRF5340 DK with HUB75 panel and breakouts | [nrf5340_prototype](nrf5340_prototype/README.md) |

```powershell
./scripts/dev.ps1 build -Image Main -Pristine
./scripts/dev.ps1 flash -Image Main
```

LED output, charging and recording always need a deliberate bench add-on
(`-ExtraConfFile`, own `-BuildDir`). `flash` checks the probe sees the right
chip and that the build matches the image.

`common/` holds the shared app, services, drivers and libraries, and
`tests/` the native tests. Each target has its own CMake, Kconfig, sysbuild
and flash layout. There's nothing to build in `firmware/` itself.

There's also a [Pro Micro bench](bench/promicro_nrf52840/README.md) that runs
the shared BLE and settings code on a bare nRF52840 board. The old
`smart_led_badge/nrf5340` board under the prototype is kept only so it keeps
compiling; Rev A is `pixel_badge_reva/nrf52840`.

[Rev A implementation](../docs/rev-a-firmware.md).
