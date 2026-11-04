# Rev A firmware

Board target `pixel_badge_reva/nrf52840`, for the Ebyte E73 (nRF52840). The
app, BLE host and controller share one Cortex-M4F; shared code is in
`../common`. This application builds two images, Diagnostics and Main.

```powershell
./scripts/dev.ps1 build -Image Main -Pristine          # the badge, LEDs and charging off
./scripts/dev.ps1 build -Image Diagnostics -Pristine   # peripheral checks, no BLE
```

Main keeps LED_EN and ARM low and BLANK high. Everything else runs, BLE and
NFC included, but anything that needs a visible display is refused: new
pairing, artwork Apply, recording. That's on purpose, not a fault.

## Bench add-ons

Add with `-ExtraConfFile` and give each build its own `-BuildDir`. Several
go in one list, for example
`-ExtraConfFile @('firmware/rev_a_nrf52840/configs/matrix-bench.conf','firmware/rev_a_nrf52840/configs/charge-bench.conf')`.

| File in `configs/` | Add to | Turns on |
|---|---|---|
| `matrix-bench.conf` | Main | LED output (current-limited supply until power checks pass) |
| `charge-bench.conf` | Main | Attended charging at 480 mA, all gates still apply |
| `recording-bench.conf` | Main | The recording path; add `matrix-bench.conf` too |
| `battery-log.conf` | Main | Discharge logging to flash ([how](../../docs/testing/electrical.md#current)) |
| `qr-polarity-bench.conf` | Main | Action swaps QR polarity in QR mode |
| `nfc-provisioning.conf` | Main | Wired NFC setup mailbox ([NFC tools](../../scripts/nfc/README.md)) |
| `lfrc.conf` | Either | 32 kHz clock from the RC oscillator, for a bad crystal |
| `matrix-stepper.conf` | Diagnostics | Staged matrix bench: MODE steps, ACTION stops |
| `diagnostics-charge.conf` | Diagnostics | The watched first charge at 480 mA |

`CONFIG_APP_BLE_PAIRING_BENCH=y` lets a board without a working matrix pair,
with the passkey printed in the log. Anyone with the log can pair, so never
use it outside the bench.

The power manager runs in every image: USB current policy, battery sensing
and load shedding, plus charging-only and low-battery System OFF in Main.
Charging needs `CONFIG_APP_CHARGING`. [Power manager](../../docs/rev-a-firmware.md#power-manager).

## Flashing and logs

Rev A is programmed over SWD from the nRF5340 DK's debug out to J501
([setup](../../docs/bringup.md#debug-and-programming)). Logs come over RTT;
there's no UART.

```powershell
./scripts/dev.ps1 recover -Target RevA -EraseAll   # once per new module: clears AP-Protect, erases everything
./scripts/dev.ps1 flash -Image Main                # erases only what the image covers
./scripts/dev.ps1 flash -Image Main -BuildDir build/reva-matrix
./scripts/dev.ps1 rtt -Target RevA                 # log to build/rtt/<time>.log
```

`flash` keeps UICR, settings, the bond and artwork. It refuses to run unless
the probe sees an nRF52840. With the badge unpowered the DK debugs its own
nRF5340, which holds the prototype. It also refuses:

- an image that isn't a Rev A build, or doesn't match `-Image`;
- an image with UICR data (erasing UICR would lock the debug port again);
- a chip whose UICR records a bootloader, such as the Pro Micro's UF2. If
  that really is the badge, `recover -EraseAll -EraseBootloader` clears it.

The firmware writes the pin-reset and NFC-pin UICR settings on first boot.
Add `-SerialNumber` when more than one J-Link is plugged in.

[Bring-up log lines](../../docs/bringup-logs.md),
[implementation notes](../../docs/rev-a-firmware.md).
