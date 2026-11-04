# Firmware

Zephyr on nRF Connect SDK 3.4.1. Services are C++17 and drivers are C. The
DK prototype and Rev A share the application and have separate board and
build roots.

| Image | Runs on | What it's for |
|---|---|---|
| Prototype | nRF5340 DK with HUB75 panel and breakouts | The bench version, with BLE |
| Diagnostics | Rev A | Peripheral checks one at a time, no BLE |
| Main | Rev A | The badge firmware |

[Commands for each image](../firmware/README.md).

## Building

Install NCS 3.4.1 with its matching toolchain. The PowerShell wrapper looks in
`C:\ncs` by default; pass `-NcsRoot` or set `NCS_ROOT` for another location.

```powershell
.\scripts\dev.ps1 build -Image Main -Pristine
.\scripts\dev.ps1 build -Image Diagnostics -Pristine
.\scripts\dev.ps1 build -Image Prototype -Pristine
```

On Linux or WSL, load the SDK environment and set `ZEPHYR_BASE`:

```sh
make build    # DK prototype without BLE
APP_DIR="$PWD/firmware/rev_a_nrf52840" BOARD=pixel_badge_reva/nrf52840 \
  BUILD_DIR=build/rev_a_nrf52840 make build
```

Main keeps the LED output, charging and recording off. Turn them on with the
[bench add-ons](../firmware/rev_a_nrf52840/README.md#bench-add-ons).

### Your own QR links

Copy [.env.example](../.env.example) to `.env` to set contact link presets.
`-QrLink Representative` ignores them. Settings saved on the badge win over
the build value. A QR Version 1 code holds up to 25 uppercase alphanumeric
characters or 17 bytes of other text, so keep links short and uppercase
where you can. Path case is kept as written.

## Buttons

| Press | Does |
|---|---|
| Mode | Next mode |
| Action | Next brightness |
| Action, held | Opens the 60-second pairing window |
| Mode, held | Asks to record, when recording is built in and the app is ready |
| Either, while recording | Stops |
| Action held through boot | Factory reset: settings and bond erased |

Settings and the bond live in NVS. Artwork sits in two flash banks, so the
old content stays until a new upload checks out. If settings can't be read,
the badge boots in safe mode. Display faults retry, and repeated failures end
in a watchdog restart.

## Code layout

Under `firmware/common/src/`: `app/` coordinates, `services/` holds portable
logic (host-testable), `drivers/` owns device I/O, and `platform/` connects
Zephyr, storage, crypto and USB. Bluetooth callbacks and interrupts only queue
work; flash and bus access happen in worker threads. No exceptions or RTTI.

## Recording

Recording needs an authenticated phone that's subscribed, and the badge must
be showing its recording view. Audio and keys stay in RAM on the badge and
never reach flash or logs. The phone keeps encrypted files and exports WAV
only when asked. [Details](audio-prototype.md).

## Flash layout

Both targets use MCUboot with two 320 KiB slots, two artwork banks and 32 KiB
of settings. Images are signed with the SDK's development key. There's no OTA
transport. [Rev A map](rev-a-firmware.md#flash-and-ram).

## Tests

`.\scripts\dev.ps1 test` runs the Ztest/Twister host tests in Docker, and
`.\scripts\dev.ps1 check` runs the static checks. On Linux, `make test` and
`make check`. The tests cover services, the protocol, interrupted storage
writes, display health, power policy, audio, motion and an emulated IMU.
[Test records](validation.md).

## Bench guides

[DK wiring](prototype-wiring.md), [BLE](ble-bench.md), [NFC](nfc-prototype.md),
[audio](audio-prototype.md), [motion](motion-prototype.md),
[DK scan timing](testing/matrix-timing.md).

## Licenses

[MIT](../LICENSE), except the Apache-2.0 nRF5340 board files derived from
Zephyr's `raytac_mdbt53_db_40`, and the [QR](../firmware/common/lib/qrcodegen/README.md)
and [font](../firmware/common/lib/font5x8/README.md) notices.
[Apache-2.0 text](../LICENSES/Apache-2.0.txt).
