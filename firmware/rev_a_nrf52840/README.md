# Rev A nRF52840 firmware

First port increment for the Ebyte E73-2G4M08S1C on Rev A, board target
`pixel_badge_reva/nrf52840`. Application, BLE host and Bluetooth controller run
on one Cortex-M4F. Shared badge logic lives in `../common`.

Two of the three programs ([firmware targets](../README.md)) come from this
application: Diagnostics and Main.

```powershell
# Main: full services, matrix output disarmed, recording disabled.
./scripts/dev.ps1 build -Image Main -Pristine

# Diagnostics: separate peripheral checks; no BLE/settings loop or recording.
# Charging off.
./scripts/dev.ps1 build -Image Diagnostics -Pristine

# Diagnostics add-on, staged matrix bench (current-limited supply):
# MODE = next step, ACTION = stop.
./scripts/dev.ps1 build -Image Diagnostics -BuildDir build/reva-stepper -ExtraConfFile firmware/rev_a_nrf52840/configs/matrix-stepper.conf -Pristine

# Diagnostics add-on for the watched first charge (480 mA, every gate must pass).
./scripts/dev.ps1 build -Image Diagnostics -BuildDir build/reva-diag-charge -ExtraConfFile firmware/rev_a_nrf52840/configs/diagnostics-charge.conf -Pristine

# Main add-on: the current-limited matrix + recording development variant.
./scripts/dev.ps1 build -Image Main -BuildDir build/reva-matrix -ExtraConfFile @('firmware/rev_a_nrf52840/configs/matrix-bench.conf','firmware/rev_a_nrf52840/configs/recording-bench.conf') -Pristine
```

These commands build only. No Rev A hardware has been flashed or tested.
Use the matrix variant only during the staged electrical bring-up. The default
image keeps LED_EN/ARM low and BLANK high; display availability, new passkey
pairing and recording visibility intentionally cannot succeed in that state.
The intentional disarm does not trigger display recovery or watchdog resets:
the service loop, BLE advertising/readback and local NFC startup remain active.
After 30 healthy seconds, this state also clears the retained watchdog-reset
counter without marking the physical display ready.
Display-dependent writes, new pairing and capture remain unavailable; a blank
matrix is never acknowledged as a physically visible frame. Actual driver
initialization errors still follow bounded recovery and restart policy.
The power manager runs in every Rev A image: USB-C/BC1.2 input-current policy,
battery sensing and matrix load shedding, and in the product image the
charging-only state (switch OFF with USB) and low-battery System OFF. A USB
device enumerates for the 500 mA host-port limit and uses HIZ while the host
is suspended. It keeps the Zephyr test VID/PID for this personal badge;
distributed firmware or boards need an assigned ID.
Charging stays disabled until the image is built with `CONFIG_APP_CHARGING`;
a complete self-test is not implemented. Thresholds are PROPOSED until
bring-up measurements
([rev-a-firmware.md](../../docs/rev-a-firmware.md#power-manager)).

NFC VCC starts low and turns on on first transport preparation, with a 10 ms
bring-up guard. It remains on while the service owns FD/session state; dynamic
power cycling and related erratum recovery are follow-up work. No automatic tag
personalization occurs. Rev A capture is 16 kHz at 1.28 MHz PDM clock; voice packets
remain compatible with Pixel Badge. Microphone response/gain require measurement.

## Flashing and logs

Rev A is programmed over SWD through the nRF5340 DK's debug out to J501 (setup
in [bringup.md](../../docs/bringup.md#debug-and-programming)). Rev A has no UART
pads; logs come over RTT.

```powershell
# Once per new module: newer nRF52840 build codes ship with AP-Protect on, so
# SWD is blocked until an ERASEALL. This erases everything on the chip.
./scripts/dev.ps1 recover -Target RevA -EraseAll

# Program an image. Only the ranges in merged.hex are erased, so UICR (pin
# reset, NFC pins as GPIO, open debug port), settings, the bond and artwork stay.
./scripts/dev.ps1 flash -Image Diagnostics
./scripts/dev.ps1 flash -Image Main

# An add-on build: name its program and its build directory.
./scripts/dev.ps1 flash -Image Diagnostics -BuildDir build/reva-stepper

# Log RTT channel 0 to build/rtt/<time>.log (Ctrl+C to stop).
./scripts/dev.ps1 rtt -Target RevA
```

Each command refuses to act unless the J-Link identifies an nRF52840. With
nothing powered on the debug-out cable the DK debugs its own nRF5340, which
holds the prototype firmware and its data. `recover` also accepts a target that
AP-Protect keeps from being identified; `recover` and `flash` pass `--family
nrf52` to nrfutil, so the erase or write itself fails if the target is not an
nRF52 (for example, badge power lost after the check). `flash` also refuses an
image that is not a Rev A build, that does not match `-Image` (Diagnostics has
no BLE, Main has), or that carries UICR data, since erasing UICR would lock the
debug port again. The firmware writes the pin-reset and NFC-pin UICR settings
itself on its first boot (`gpio-as-nreset`, `nfct-pins-as-gpios`), and keeps the
debug port open while UICR allows it (`CONFIG_NRF_APPROTECT_USE_UICR`, the SDK
default). `flash` and `recover` also refuse an nRF52840 whose UICR records a
bootloader (`UICR.NRFFW[0]`, for example the UF2 bootloader of the Pro Micro
bench): `flash` would overwrite its MBR and `recover` would erase the
bootloader. If that chip really is the badge, `recover -EraseAll
-EraseBootloader` clears UICR. Add `-SerialNumber` when more than one J-Link is
connected.

[Bring-up logs](../../docs/bringup-logs.md) lists the expected log lines for each
bring-up step.

[Implementation, memory, chip trade-offs and bring-up gates](../../docs/rev-a-firmware.md)
