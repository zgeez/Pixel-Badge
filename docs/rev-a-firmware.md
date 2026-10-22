# Rev A nRF52840 firmware

Rev A runs the application and Bluetooth controller on one nRF52840.
The default image keeps matrix output, charging, and recording disabled.
Builds and host tests pass; board measurements are pending.

## Source and configuration

`firmware/rev_a_nrf52840` contains board/configuration files; `firmware/common`
contains shared services and drivers. The DK application remains separate.
[Build, flash, and diagnostics commands](../firmware/rev_a_nrf52840/README.md).

## Pins and peripherals

| Resource | Allocation |
|---|---|
| Matrix SPI | SPIM3, 8 MHz; P0.12 SCK, P0.04 MOSI, P0.08 MISO |
| Matrix control | P1.09 LAT, P0.05 BLANK, P0.06 ARM, P0.26 SAFE, P0.07 LED_EN |
| Timing | TIMER3, GPIOTE task, two PPI connections |
| IMU/charger | TWIM0, 100 kHz; P0.15/P0.17; IMU interrupt P0.09 |
| NTAG | TWIM1, 100 kHz; P0.22/P0.24; VCC P1.00, FD P1.02 |
| PDM | CLK P0.20, DIN P0.13 |
| SAADC | AIN6 VBAT, AIN0 CC1, AIN1 CC2, AIN5 VLED |

All 29 GPIO are allocated; there is no UART, spare pad, or IMU SPI fallback.
Use SWD/RTT. LFXO uses external capacitors; UICR enables pin reset and NFC pins
as GPIO. REG1 stays in LDO mode until module DC/DC support is verified.

`pinmap.json` records the 43 module pads. `scripts/ci/check_reva_contract.py`
checks compiled devicetree/flash layout and can compare U401 against a KiCad
netlist. Only load trusted local `edt.pickle` files. Run `--self-test` to check
that wrong GPIO and overlapping partitions are rejected.

## Display backend

A dedicated worker sends an eight-byte row/column command with SPI/DMA.
Two 1024-byte buffers swap at frame boundaries. Eight weighted bitplanes
serve both binary and grayscale content.

TIMER3/PPI/GPIOTE bracket BLANK independently of thread scheduling. The timer
stops before waking the worker; late work extends the dark interval. ARM stays
low until VLED settles and a complete row is latched. SPI faults/timeouts blank
and disarm the matrix. Recovery reinitializes it and requires scan progress;
persistent errors reach the shared watchdog restart policy.

Intentional output disarm keeps the service loop healthy but never acknowledges
a visible frame. Pairing codes, display-dependent writes, and recording remain
blocked. Thirty healthy seconds clear the retained watchdog-reset counter.
Refresh, optical shades, mapping, current, and independent stall blanking need
physical tests.

## Flash and RAM

| Region | Start | Size |
|---|---|---|
| MCUboot | `0x00000` | 64 KiB |
| Primary | `0x10000` | 320 KiB, including header pad |
| Secondary | `0x60000` | 320 KiB |
| Two artwork banks | `0xb0000` | 264 KiB |
| Reserve | `0xf2000` | 24 KiB |
| Settings/bond | `0xf8000` | 32 KiB |

The combined application/controller fits the primary slot. OTA transport is
not implemented; signing uses the SDK development key. Partition offsets
match the prototype, but cross-chip bond migration is not assumed. Linker
allocation does not measure stack high-water usage or timing margin.

## nRF5340 to nRF52840 trade-offs

The module change was driven by availability ([ADR 0015](decisions/0015-nrf52840-e73-module.md)).
Radio and application now share one 64 MHz core and 256 KiB RAM. The port uses
SPIM3/PPI rather than the DK's peripherals, and native 16 kHz/1.28 MHz PDM
rather than 32 kHz capture plus decimation. Audio packets remain 16 kHz.
Bounded buffers and flash-backed artwork preserve capacity; combined BLE,
display, audio, and motion deadlines still need measurement.

## Bare nRF52840 development-board bench

The [Pro Micro bench](../firmware/bench/promicro_nrf52840/README.md) tests shared
BLE/settings/rendering with a USB pairing display. It retains the factory UF2
layout and has no real NFC, microphone, or IMU. It is not a Rev A image.

## Power and diagnostics

### Power manager

`app/power_manager.cpp` owns charger setup, CC/USB limits, pack checks, battery
sampling, and load shedding. `services/power_policy.cpp` is host-testable.
[Power settings and thresholds](power.md) are documented once there.

Charging requires VBUS, healthy TS/fault state, matching register readback,
charger/ADC agreement, and a recent pack-presence check. The probe pauses
charging, applies the charger's approximately 30 mA discharge for 60 ms, then
requires VBAT ≥ 3.0 V and confirmed discharge-off. A failed clear keeps charging
off and is retried. Test both pack-present and pack-absent behavior first.

The product image adds charging-only and low-battery System OFF states.
Invalid/stale inputs hold the display off; sustained manager faults stop
watchdog feeding. The driver enforces the brightness ceiling per slot.
USB enumerates after BC1.2 detection and uses HIZ on host suspend.

Diagnostics logs rails, charger state/ADC, clock/reset cause, buttons, and
matrix-chain checks. The stepper enables staged patterns on a current-limited
supply; its load shedding is disabled deliberately. Keep the supply limit in
place. NFC dynamic power cycling and Rev A wired-setup packaging remain work.
[Bring-up checklist](bringup.md) · [Expected values](bringup-expected.md) ·
[RTT reference](bringup-logs.md)
