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
matching ODRs or, later, the FIFO. On the bench module, the first I²C
transaction after the driver turns I3C off stalled until the TWIM timeout and
its bus recovery, so the driver runs the bus recovery right after that write
(a bench observation, not a datasheet requirement). Moving the part to SPI only
changes the bus node and uses `compatible = "invensense,icm42688",
"invensense,icm4268x"`. Wake-on-motion
is not implemented yet.

The application reaches the display only through Zephyr's display API
(`zephyr,display` chosen node), as a 32 × 32 monochrome display
(`PIXEL_FORMAT_MONO01`, rows packed most significant bit first). On the DK the
backend is a HUB75 panel (below); the badge gets its own matrix driver behind
the same API. Frames and test patterns live in `firmware/src/services/`
(`badge/test_pattern.hpp`), and `firmware/src/app/display_demo.cpp` cycles the
patterns.

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

On the DK, `firmware/boards/nrf5340dk_nrf5340_cpuapp.overlay` puts the Rev A
pin roles on the DK's pins where it can: SPIM4 on P0.08–P0.10, matrix LAT and
BLANK on P0.11/P0.12, and I²C1 with SCL on P1.02 and SDA on P1.03. It also adds
a frame marker on P0.26 for the logic analyzer and removes Buttons 3 and 4
(their pins carry the matrix SPI). UART1 stays off: its pins are the interface
MCU's serial port 0 (VCOM0 on the board; RXD P1.00, TXD P1.01, CTS P0.10,
RTS P0.11), and P0.10/P0.11 are no longer forwarded to the network core, so
starting it (for Bluetooth, for example) cannot take matrix MISO and LAT away.
Adding or changing an overlay needs a pristine build (`-Pristine`, or
`--pristine` with west).

The overlay also makes a HUB75 RGB panel the DK's display, for bench work
only: R1, G1, B1, CLK and LAT on P1.11–P1.15, OE on P1.08 and row address A–E
on P0.04–P0.07 and P0.25, through 5 V buffers (SN74AHCT245). The driver
(`firmware/src/drivers/hub75`, compatible `smartledbadge,hub75`) shows the
32 × 32 window in the panel's top half in one colour. TIMER0 times each 160 µs
row slot (195 Hz refresh): its interrupt latches the row shifted during the
previous slot and shifts the next row while this one is lit, and a second
compare switches OE off through DPPI and GPIOTE, so brightness is set by
hardware. TIMER1 and TIMER2 stay free for the matrix timing test. If the image
is mirrored left to right, set `mirror-x` on the node.

The DK overlay also puts the IMU (INT1 on P1.04), the PDM microphone (CLK P1.07,
DATA P1.09) and the NFC tag's field-detect line (P1.06) on their Rev A pins. The
tag shares the IMU's I²C1 bus on the DK, because its Rev A bus pins are the
debugger's serial port. At start-up the application runs module checks
(`firmware/src/app/bringup.cpp`) while the display test patterns run in their
own thread, then logs an IMU sample, the NFC field-detect level with its edge
count, and the microphone level every 5 s:

- IMU: identity (checked by the driver), one sample with the gravity magnitude,
  and the data-ready interrupt rate (100 Hz expected).
- NFC tag: reads block 00h over I²C and checks that byte 0 reads 04h. The read
  follows the field-detect guarded procedure in [nfc.md](nfc.md): it does not
  start while an RF field is present, sends the block address and the read as
  separate frames (START and STOP), and before and after each frame waits the
  field-detect latency and checks for a field, so no frame starts until a field
  that arose during the previous one would have shown. Any RF field
  or activity fails the check, because the tag then needs full verification
  before use. The field-detect latency on the DK is not measured yet, so a
  fixed margin is used. It writes nothing to the tag.
- Microphone: captures both PDM channels at 32 kHz with the clock near 2 MHz
  (the MP34DT01-M's range is 1–3.25 MHz, 2.4 MHz typical; at 1.25 MHz the
  quiet level was over three times higher) and logs the mean, AC RMS and
  range of each. A
  mic's PDM stream averages near zero; a DATA line nothing drives (pulled down
  on the DK) decimates to a large constant offset, so the check fails unless a
  channel varies with a mean within ±4096. Both channels may carry nearly the
  same data, because the mic's idle half-cycle can hold the previous bit.
  Samples stay in the capture buffers and are discarded; only the levels are
  logged.

Before matrix work on the DK: the interface MCU leaves its UART pins tri-stated
until a terminal opens the port and asserts DTR, and then drives them. Close
every terminal on the DK's J-Link COM ports and log over RTT instead. If one was
opened, power-cycle the DK by unplugging its USB cable. For captures that do not
need the debugger, the nRF ONLY switch (SW6) disconnects the interface MCU
completely, including SWD, so flashing and RTT stop working until it is set back.

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

## Bench tests

Bench tests are separate applications under `firmware/bench/`, built for the
DK with the same overlay. They are not part of the badge image. CI builds them
so they keep compiling.

### Matrix scan timing

`firmware/bench/matrix_timing` drives the matrix scan signals with the planned
hardware timing, without a matrix attached, so they can be checked on a logic
analyzer. TIMER1 times each 160 µs row slot and drives SPIM4 and the LAT and
BLANK pins through DPPI and GPIOTE. TIMER2 counts 32 rows and then stops TIMER1.
The CPU only restarts each frame from the TIMER2 interrupt, so a late interrupt
leaves the matrix blanked instead of a row on.

| Time in slot | Event |
|---|---|
| 1 µs | LAT high: latches the row shifted during the previous slot |
| 2 µs | LAT low; SPIM4 starts shifting the next row (8 bytes at 8 MHz) |
| 4 µs | BLANK low: row on |
| 159 µs | BLANK high: row off |
| 160 µs | Next slot |

Each row is 32 row-select bits (the row's bit is 0, which turns its PFET on)
followed by 32 column bits (`A5`, row number, `5A`, inverted row number).
RTT logs the frame rate once a second (about 195 frames/s).

Measured results, with the setup and raw data, are in
[validation.md](validation.md#result-scan-timing-on-the-nrf5340-dk-bench-no-matrix).

```powershell
.\scripts\dev.ps1 build -App firmware/bench/matrix_timing -BuildDir build/matrix_timing
```

```sh
APP_DIR="$PWD/firmware/bench/matrix_timing" BUILD_DIR=build/matrix_timing make build
```

Analyzer channels: SCK P0.08, MOSI P0.09, LAT P0.11, BLANK P0.12, frame
marker P0.26 (high while the frame interrupt runs), and GND. A 24 MS/s
analyzer decodes the 8 MHz shift (three samples per clock) and times edges to
about ±42 ns, but it distorts the SCK duty cycle; use a faster one to check
SCK itself. Leave Buttons 3 and 4 unpressed and keep terminals off
the DK's COM ports (see above).

### QR screening

Building the application with `firmware/configs/qr-screening.conf` replaces the
display test patterns with QR test codes for the scan-reliability screening
(validation plan, QR section). Each code is shown in normal and inverted
polarity at brightness 255, 64 and 16; DK Button 1 steps to the next setting,
Button 2 back, and RTT logs the setting number.

```powershell
.\scripts\dev.ps1 build -BuildDir build/qr -ExtraConfFile firmware/configs/qr-screening.conf
```

The codes come from `scripts/qr/gen_qr_test_codes.py` (segno to encode,
ZXing-C++ to decode each frame back in both polarities). The committed set uses
representative Version 1 payloads with the full quiet zone. Codes for other
payloads, such as personal links, are written to a file outside the repository
with `--out-cpp` and selected with `CONFIG_APP_QR_CODES_FILE`; Version 2 and 3
codes fit only with a reduced quiet zone and are experiments.

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
`firmware/tests/test_pattern` checks the frame bit layout and the test
patterns.
