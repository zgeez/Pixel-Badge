# Firmware

Status: **bootstrap**. The application currently initializes logging and
prints a startup message. No product features are implemented.

## Platform

- nRF Connect SDK v3.4.1 (Zephyr RTOS)
- Initial target: `nrf5340dk/nrf5340/cpuapp`
- Language: C++17 for application and domain logic; C or C-compatible
  interfaces where Zephyr APIs require them

## Language and runtime policy

- No C++ exceptions.
- No RTTI unless explicitly justified later.
- No runtime dynamic allocation unless explicitly justified.
- Zephyr synchronization and threading primitives (`k_mutex`, `k_sem`,
  `k_msgq`, work queues), not `std::thread`/`std::mutex`.
- The build currently uses Zephyr's minimal C++ library. Whether to enable a
  full C++ standard library is TBD.
- Application-owned sources compile with `-Wextra -Werror`. These flags apply
  only to the application target, not to Zephyr or third-party modules.

## Proposed layering

Dependencies point downward only.

| Layer | Directory | Responsibility |
|---|---|---|
| Application | `firmware/src/app/` | Display modes, mode switching, top-level behavior |
| Services | `firmware/src/services/` | Hardware-independent logic and protocol handling (for example display content generation, BLE/NFC service logic, sensor data processing) |
| Drivers | `firmware/src/drivers/` | Project-specific device drivers (for example the LED matrix driver), using the Zephyr device model where it fits |
| Platform | `firmware/src/platform/` | Board and Zephyr integration glue |

Pure logic in the services layer should not depend on hardware so it can be
unit-tested on a host target. Public headers shared across layers go in
`firmware/include/`. Board overlays and custom board definitions go in
`firmware/boards/`.

`firmware/src/drivers/` is a Zephyr module (`zephyr/module.yml`) so the
application and the tests share its Kconfig, devicetree bindings and sources.
It holds the ICM-42688-P I²C driver (compatible `invensense,icm42688p-i2c`):
NCS v3.4.1's `icm4268x` driver supports SPI only. The project driver reuses
that driver's register map and devicetree property encoding, applies the
datasheet's I²C settings (DS-000347 §12.3), and supports data-ready interrupts.
It rejects accelerometer ODRs the selected power mode cannot run, sets ODR and
full scale before turning the sensors on, and reports a sensor that has no
sample since reset (data registers at -32768: sensor off or still starting) as
`-EAGAIN` from fetch and `-ENODATA` from `channel_get` instead of as data. It
does not detect repeated samples: with unequal ODRs the slower sensor's
registers hold its last sample, so consumers that need each sample once use
matching ODRs or, later, the FIFO. Moving the part to SPI only
changes the bus node and uses `compatible = "invensense,icm42688",
"invensense,icm4268x"`. Wake-on-motion
is not implemented yet. The other directories are empty; interfaces will be
defined as features are implemented.

## Logging

- Primary: Zephyr logging with the SEGGER RTT backend (enabled in `prj.conf`).
- Fallback on the custom board: UART test pads.

No project-specific logging abstraction is used.

## Audio privacy requirement

Planned audio visualization processes raw microphone samples locally in
volatile/transient buffers only. No raw samples may reach flash, NVS/settings,
external storage, logs, crash dumps, or other persistent storage. Review capture,
DSP, logging, persistence, and fault/dump data flows before release, including
debug configurations. Persistent crash dumps must exclude sample buffers or be
disabled. Verification requirements are in
[validation.md](validation.md#8-microphone-and-audio-processing); no audio
implementation or completed privacy audit exists yet.

## Building

Requires nRF Connect SDK v3.4.1 and its toolchain.

Windows (PowerShell), with the SDK installed under `C:\ncs` (override with
`-NcsRoot` or the `NCS_ROOT` environment variable). The script loads that SDK
and the toolchain it requires, and fails if the SDK is not v3.4.1 or the
toolchain is missing:

```powershell
.\scripts\dev.ps1 build
```

Linux/WSL, with the toolchain on `PATH` and `ZEPHYR_BASE` set:

```sh
make build
```

Output: `build/nrf5340dk/firmware/zephyr/zephyr.hex`.

### Rev A board

`firmware/boards/smart_led_badge/` defines the Rev A board (Raytac
MDBT53-P1M, nRF5340), based on Zephyr's `raytac_mdbt53_db_40` board. The
application-core devicetree follows the Rev A schematic:
- SPIM4 for the matrix chain, with GPIOs for LAT, BLANK, ARM and SAFE.
- TWIM1 for the IMU and charger at 400 kHz, and TWIM2 for the NTAG at 100 kHz.
- PDM microphone; SAADC channels for VBAT, CC1, CC2 and VLED with the gains
  in [power.md](power.md).
- Buttons, status LED, UART on the test pads, and USB.
- 32.768 kHz crystal with the internal 9 pF load; DC/DC on VREGMAIN and
  VREGRADIO.

Board signals without a Zephyr driver (charger enable CHG_EN, INT, STAT, power switch,
NFC power and field detect, LED-rail enable, VBAT measurement switch) are
`zephyr,user` GPIOs. The network core owns no pins. The build scripts pass
`BOARD_ROOT` so sysbuild finds the board, and CI builds both cores:

```powershell
.\scripts\dev.ps1 build -Board smart_led_badge/nrf5340/cpuapp -BuildDir build/badge
```

```sh
BOARD=smart_led_badge/nrf5340/cpuapp BUILD_DIR=build/badge make build
```

The application does not use most of these peripherals yet, so its own
configuration leaves their drivers out. `firmware/configs/rev-a-peripherals.conf`
enables the I²C, SPI, ADC, PDM, sensor (IMU), input, LED, USB and watchdog
drivers. CI builds the application core with it too, so driver and devicetree
integration breaks the build rather than bring-up. It is a compile check, not
a product configuration. Pass it with `-ExtraConfFile` (path relative to the
repository root) or `EXTRA_CONF_FILE`:

```powershell
.\scripts\dev.ps1 build -Board smart_led_badge/nrf5340/cpuapp -BuildDir build/badge-periph -ExtraConfFile firmware/configs/rev-a-peripherals.conf
```

```sh
BOARD=smart_led_badge/nrf5340/cpuapp BUILD_DIR=build/badge-periph \
  EXTRA_CONF_FILE="$PWD/firmware/configs/rev-a-peripherals.conf" make build
```

The board files are modified from Zephyr's `raytac_mdbt53_db_40` board and
keep its Apache-2.0 license and copyright notices
([LICENSES/Apache-2.0.txt](../LICENSES/Apache-2.0.txt)).

## Tests

Tests use Ztest and run with Twister on the `native_sim` host target. They live
under `firmware/tests/`. `native_sim` runs only on Linux, so on Windows
`dev.ps1 test` runs the tests in the same toolchain container image as CI
(requires Docker). The SDK sources are kept in the Docker volume
`smart-led-badge-ncs-v3.4.1`; if it ever fails the workspace version check,
remove it with `docker volume rm smart-led-badge-ncs-v3.4.1`.

The toolchain image has no host C++ compiler or `make`, so CI and `dev.ps1 test`
install them from a fixed Ubuntu archive snapshot with exact package versions
(`scripts/ci/install-host-deps.sh`).

```powershell
.\scripts\dev.ps1 test
```

```sh
make test
```

`firmware/tests/sanity` checks that C++17 tests build and run.
`firmware/tests/icm42688_i2c` runs the IMU driver against a register-level
emulator on the native_sim I²C bus: reset and identity checks, I²C-mode
configuration and write order, power-mode/ODR validation (including a
low-power start-up), invalid-sample handling, unit conversion, and the
data-ready interrupt path. It proves
the driver logic, not behavior on the real part.
