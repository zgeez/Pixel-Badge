# Firmware

NCS 3.4.1 / Zephyr, C++17 services, and C device drivers. Prototype and Rev A
share application logic but have separate board/build roots.

## Platform

| Program | Hardware |
|---|---|
| Prototype | nRF5340 DK, HUB75, external sensors/tag |
| Diagnostics | Rev A nRF52840, peripheral checks without BLE |
| Main | Rev A nRF52840, application and BLE controller |

[Target commands](../firmware/README.md). The older nRF5340 custom board is a
compile-regression fixture, not the current Rev A board.

## Building

Install NCS **3.4.1** with its matching toolchain. The Windows wrapper defaults
to `C:\ncs`; use `-NcsRoot` or `NCS_ROOT` for another location.

```powershell
.\scripts\dev.ps1 build -Image Prototype -Pristine
.\scripts\dev.ps1 build -Image Diagnostics -Pristine
.\scripts\dev.ps1 build -Image Main -Pristine
```

On Linux/WSL, load the SDK environment and set `ZEPHYR_BASE`:

```sh
make build
APP_DIR="$PWD/firmware/rev_a_nrf52840" BOARD=pixel_badge_reva/nrf52840 \
  BUILD_DIR=build/rev_a_nrf52840 make build
```

`make build` defaults to the non-BLE prototype configuration. Named PowerShell
images select their presets. Main keeps matrix/recording/charging disabled;
use the explicit [bench fragments](../firmware/rev_a_nrf52840/README.md).

### Local QR links

Copy [.env.example](../.env.example) to `.env` for optional contact presets.
Use `-QrLink Representative` to ignore local values. Saved settings take
precedence; preserve URL path case. Version 1 fits up to 25 QR alphanumeric
characters or 17 ASCII bytes with a full quiet zone.

## Settings and controls

| Control | Action |
|---|---|
| Mode short | Next mode |
| Action short | Next brightness |
| Action long | Pairing window |
| Mode long | Recording request, when enabled and the app is ready |
| Either button during recording | Stop |
| Action held through boot confirmation | Factory reset; settings/bond erased |

NVS holds validated settings/bond; two flash artwork banks preserve the active
content until upload verification completes. Read failures select safe mode.
Display faults retry, then reach the watchdog restart policy. RTT supplies logs.

## Layering

Under `firmware/common/src/`, `app/` coordinates services, `services/` holds
portable logic, `drivers/` owns device I/O, and `platform/` connects Zephyr,
storage, crypto, and USB. Buffers/queues are bounded. SDK Bluetooth/crypto use
bounded heaps; application code disables exceptions/RTTI and uses Zephyr locks.
BLE callbacks queue work; ISRs signal workers rather than doing bus/flash work.

## Audio privacy requirement

Recording needs an authenticated subscribed app and confirmed visible frame.
Audio/keys remain volatile on the badge and out of logs/dumps. The phone keeps
encrypted private files; export is explicit. Display/rail/app/link faults stop
capture. [Audio details](audio-prototype.md).

### MCUboot and static flash layout

Use the sysbuild output, not an unsigned inner image. Both targets reserve
two 320 KiB slots, artwork banks, and 32 KiB NVS; the DK also has a network-core
image. Signing uses the SDK development key; OTA transport is not implemented.
[Partition map](rev-a-firmware.md#flash-and-ram).

## Tests

Windows: `./scripts/dev.ps1 test` (Docker) and `./scripts/dev.ps1 check`.
Linux: `make test` and `make check`. Ztest/Twister covers services, protocol,
storage interruptions, display health, power policy, audio, motion, and the
emulated IMU. Physical results are indexed in [validation](validation.md).

## Bench tests

[DK wiring](prototype-wiring.md) · [BLE](ble-bench.md) · [NFC](nfc-prototype.md) ·
[Audio](audio-prototype.md) · [Motion](motion-prototype.md)

### Historical foundation checks

[Initial P0 procedure and limits](testing/foundation.md).

### Matrix scan timing

[Standalone DK timing bench](testing/matrix-timing.md). Its 195.2 Hz result
predates the current grayscale driver.

## Licenses

[MIT](../LICENSE), except the Apache-2.0 nRF5340 board files derived from
Zephyr's `raytac_mdbt53_db_40` and the retained
[QR](../firmware/common/lib/qrcodegen/README.md)/[font](../firmware/common/lib/font5x8/README.md)
notices. [Apache-2.0 text](../LICENSES/Apache-2.0.txt).
