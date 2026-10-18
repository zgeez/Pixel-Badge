# Rev A nRF52840 firmware

First port increment for the Ebyte E73-2G4M08S1C on Rev A, board target
`pixel_badge_reva/nrf52840`. Application, BLE host and Bluetooth controller run
on one Cortex-M4F. Shared badge logic lives in `../common`.

```powershell
# Full services, matrix output disarmed, recording disabled.
./scripts/dev.ps1 build -Target RevA -Pristine

# Separate peripheral checks; no BLE/settings loop or recording.
./scripts/dev.ps1 build -Target RevA -BuildDir build/reva-diagnostics -ExtraConfFile firmware/rev_a_nrf52840/configs/diagnostics.conf -Pristine

# Compile the current-limited matrix + recording development variant.
./scripts/dev.ps1 build -Target RevA -BuildDir build/reva-matrix -ExtraConfFile @('firmware/rev_a_nrf52840/configs/matrix-bench.conf','firmware/rev_a_nrf52840/configs/recording-bench.conf') -Pristine
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
Charging remains disabled. Battery sensing, charger/USB policy, load shedding,
complete self-test and low-power transitions still need implementation.

NFC VCC starts low and turns on on first transport preparation, with a 10 ms
bring-up guard. It remains on while the service owns FD/session state; dynamic
power cycling and related erratum recovery are follow-up work. No automatic tag
personalization occurs. Rev A capture is 16 kHz at 1.28 MHz PDM clock; voice packets
remain compatible with Pixel Badge. Microphone response/gain require measurement.

[Implementation, memory, chip trade-offs and bring-up gates](../../docs/rev-a-firmware.md)
