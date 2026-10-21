# Rev A bring-up logs

What each bring-up step flashes and what its RTT log must show. Use it beside
the [bring-up checklist](bringup.md), which holds the measurements and the stop
rules. Strings in `code` are printed by the firmware exactly as shown; Zephyr
adds a timestamp and module prefix such as `[00:00:02.004,000] <inf> bringup:`.
Values marked "expect" are design expectations to compare with a meter, not
measured results.

## Images and commands

| Stage | Program | Build, then flash |
|---|---|---|
| Electrical checks, sensors, outputs-off matrix test | Diagnostics | `dev.ps1 build -Image Diagnostics -Pristine`, `dev.ps1 flash -Image Diagnostics` |
| Staged matrix bench (current-limited supply) | Diagnostics + stepper | `dev.ps1 build -Image Diagnostics -BuildDir build/reva-stepper -ExtraConfFile firmware/rev_a_nrf52840/configs/matrix-stepper.conf -Pristine`, then `dev.ps1 flash -Image Diagnostics -BuildDir build/reva-stepper` |
| Pack probe and first watched charge | Diagnostics + charge | `dev.ps1 build -Image Diagnostics -BuildDir build/reva-diag-charge -ExtraConfFile firmware/rev_a_nrf52840/configs/diagnostics-charge.conf -Pristine`, then `dev.ps1 flash -Image Diagnostics -BuildDir build/reva-diag-charge` |
| BLE advertising and readback, NFC, power states; display disarmed | Main | `dev.ps1 build -Image Main -Pristine`, `dev.ps1 flash -Image Main` |
| Display, new passkey pairing, refresh timing, loaded load shedding (current-limited supply) | Main + matrix | `dev.ps1 build -Image Main -BuildDir build/reva-matrix -ExtraConfFile firmware/rev_a_nrf52840/configs/matrix-bench.conf -Pristine`, then `dev.ps1 flash -Image Main -BuildDir build/reva-matrix` |

The default Main image keeps the matrix disarmed (`CONFIG_REVA_MATRIX_OUTPUT=n`):
display writes and new passkey pairing cannot succeed, and the LED rail never
loads the battery. Use the Main + matrix build for those checks, only on a
current-limited supply until the battery and charging checks pass. Add
`firmware/rev_a_nrf52840/configs/recording-bench.conf` to the same
`-ExtraConfFile` list for recording checks.

Capture every run with `dev.ps1 rtt -Target RevA` (log in `build/rtt/`) and
attach the file to the board's bring-up issue. A new module may need
`dev.ps1 recover -Target RevA -EraseAll` once before the first flash
([Rev A README](../firmware/rev_a_nrf52840/README.md#flashing-and-logs)).

## Probe check (no image)

| Setup | Pass | Fail |
|---|---|---|
| Badge unpowered, cable on J501 | `flash` refuses: `sees an nRF5340 (the DK itself)` | Anything is programmed |
| Badge powered | `Probe <serial> sees NRF52840_...` | `could not identify an nRF52840`: check the cable, VTref and badge power; a new module may need `recover` |

## Diagnostics: first boot

The image starts with `smart-led-badge starting on pixel_badge_reva/nrf52840`.
Two seconds later the checks run once, in this order, and end with a count.

**Reset and clock**

| Line | Pass | Fail or follow-up |
|---|---|---|
| `Reset cause 0x... (pin ., por ., watchdog ., software ., debug .)` | Record it. After a power cycle expect `por 1`; after the J501 pin 10 reset expect `pin 1` | `watchdog 1` on a first boot: capture the log before it and stop |
| `LFCLK running, source 32.768 kHz crystal` | 32.768 kHz crystal running | `RC` or `synth`, or `stopped`: the crystal (C405/C406, 12 pF) is not the clock source; measure it before relying on BLE timing |

**Power manager** (the same code as the product image, with verbose logs)

| Line | Pass | Fail or follow-up |
|---|---|---|
| `Charger: BQ25620 rev N found` | Present | `Charger: FAIL, no BQ25620 at 0x.. (part 0x.., err); retrying`: I2C, charger supply or address. It retries every second |
| `Charger: FAIL, configuration write; retrying` | Absent | Present: register writes fail; check I2C before any charge test |
| `USB: FAIL, device start (err)` | Absent | Present: the USB stack did not start; with USB attached the manager reports unhealthy |
| `Power: VBAT N mV via divider (expect pack voltage, or ~0 with no pack)` | No pack: about 0. With a pack: compare with a meter at the pack and record the difference | A large difference: divider values or SAADC setup |
| `Power: VLED N mV (LED rail is off at start: expect ~0)` | About 0 | `Power: FAIL, VLED N mV while LED_EN is low; matrix held off`: the LED rail is on while disabled. Stop: power section failure |
| `Power manager started: N failed checks; charging disabled in this image` | `0 failed checks` | Any failure: find the matching FAIL line above |

**Matrix, outputs off**

| Line | Pass | Fail or follow-up |
|---|---|---|
| `Matrix: MTX_SAFE_SENSE 1 (expect 1: safe-off while ARM is low)` | `1` | `0`: the one-shot or SAFE path is not in its safe state with ARM low. Stop before any matrix work |
| `Matrix chain: sent ..., read ..., offset N` (five lines) | The same offset on all five, 31 to 33 | `Matrix: FAIL, readback does not match a 32-bit TLC chain`; `(MISO stuck at one level)` means no data returns: check MTX_MISO and the chain |
| `Matrix: TLC chain readback OK (offset N bits)` | Present | Absent: see the FAIL line above |

**Sensors and NFC**

| Line | Pass | Fail or follow-up |
|---|---|---|
| `IMU: identity OK (WHO_AM_I checked by the driver)` | Present | `IMU: FAIL, not ready (...)`: bus, power or identity; see the driver's log line before it |
| `IMU: accel x y z mm/s2 (\|a\| N), gyro ... mdps` | Badge still: \|a\| 8800 to 10800 (gravity 9807); gyro near 0 | `IMU: CHECK, \|a\| not near 9807 mm/s2`: keep it still and repeat; persistent means scale or axis problem |
| `IMU: N data-ready interrupts in 1 s (ODR 100 Hz)` | 80 to 120 | `IMU: FAIL, interrupt rate`: the INT line or its GPIO configuration |
| `NFC tag: guarded read result 0` | `0` | Any other value: NTAG supply (NFC VCC), I2C or address. The check reads only; it writes no EEPROM |
| `Mic: left mean ... AC RMS ...; right ...` then `Mic: data present; confirm by sound...` | Data present, and the AC RMS in the 5 s readings rises when you clap | `Mic: FAIL, no microphone data on DATA (stuck at one level)` or `Mic: FAIL, capture err`: PDM clock/data, mic supply or L/R strap |
| `Bring-up checks done: N failed` | `0 failed` | Count of the failures above |

The HC595 row chain cannot be read back with VLED off; check SER_V, SRCLK_V
and RCLK_V on a logic analyzer.

The mic threshold (mean within ±4096, not stuck) comes from the DK bench with a
different microphone; the clap check is the real test for the SPH0641LM4H-1.

## Diagnostics: every 5 seconds

| Line | Expect | Notes |
|---|---|---|
| `Rails: VBAT(SAADC) N mV, VLED N mV, CC1 N mV, CC2 N mV (pin), CC <advert>` | VLED about 0. CC `none` with no USB-C source; `default` from a source advertising default USB current (most computer ports and USB-A cables); `1.5A/3.0A` from a source that advertises it | `no reading` or `?` means a CC read problem |
| `Charger: VBUS N mV IBUS N mA \| VBAT ... \| VSYS ... \| TS N % \| TDIE N C` | Compare VBUS, VBAT and VSYS with a meter | `Charger: last register read failed; no current telemetry` is an I2C failure |
| `Charger: <charge state>, D+/D- <result>, TS <zone>, fault 0x00, ..., VREG 4200 mV, CHG_EN 0 - disabled in this image (CONFIG_APP_CHARGING=n)` | `fault 0x00`, TS `normal`, VREG 4200 mV, CHG_EN 0 | D+/D- result: `SDP 500mA` on a computer, `DCP 1.5A` on a BC1.2 wall charger, `none` without VBUS |
| `Charger event: flag0 0x.. flag1 0x.. fault_flag0 0x..` | Logged when the charger latches an event (plug-in, removal) | Non-zero fault flags: record them with the setup |
| `Button MODE pressed/released`, `Button ACTION ...`, `Power switch ON/OFF` | One line per press or switch change | Missing edges: button or switch wiring |
| `IMU: accel ..., gyro ..., N data-ready`, `NFC tag: field detect 0/1`, `Mic: ...` | Readings follow motion, a phone near the antenna (`1`), and sound | |

The status LED blinks while the image runs.

## USB-C

| Step | Pass |
|---|---|
| Computer port | D+/D- `SDP 500mA` (or `CDP 1.5A`), then `USB configuration 1` and `Charger: input limit 500 mA` |
| USB-C charger advertising 1.5 A or 3 A | CC `1.5A/3.0A` and `Charger: input limit 1500 mA`, no `USB configuration` line |
| BC1.2 wall charger | D+/D- `DCP 1.5A` and `Charger: input limit 1500 mA`, no enumeration |
| Host sleeps, pack fitted | `Charger: entering HIZ (USB host suspended)`; waking the host gives `Charger: leaving HIZ (USB host active)` |
| Host sleeps, no pack above 3.40 V | `USB host suspended without a pack above N mV: input stays on` |
| Failures | `Charger: HIZ not confirmed; matrix held`, `USB enable failed (err); retrying` |

## Pack probe and first watched charge (charge add-on)

| Setup | Pass | Stop |
|---|---|---|
| USB-C attached, no pack | `Charger: pack probe N mV under 30 mA discharge: NO PACK` and `Charger: charging off (VBAT out of range / no pack)` or another reason; never `CHARGING ENABLED` | `CHARGING ENABLED` without a pack |
| Meter-checked pack | `pack present` at about the metered voltage, then `Charger: CHARGING ENABLED at ICHG 480 mA; watch the cell and charger temperature` | Any reason line (below) that does not match the setup |
| Unplug the pack while charging | Within 30 s: `NO PACK` and `Charger: charging off (...)` | Charging stays on |

Every reason charging is held off is logged once as `Charger: charging off
(<reason>)`. Reasons include `no VBUS`, `charger fault`, `TS not normal`,
`TS thresholds not set`, `ICHG readback mismatch`, `VREG not 4200 mV`,
`input limit not at policy`, `VBAT out of range / no pack`,
`VBAT readings disagree (SAADC vs charger)`, `pack presence probe`,
`input source changed`, `USB host suspended (HIZ)` and read failures.
`Charger: I2C watchdog expired; re-applying ...` means the charger reset its
settings; charging resumes only after they read back again.

## Staged matrix bench (stepper add-on)

Start on a current-limited supply. The log opens with `Matrix bench: current-limited
supply only. MODE = next step, ACTION = stop/resume` and `Matrix bench step 0:
blanked, LED rail off`. Each MODE press advances one step and logs
`Matrix bench step N: <name>`:

| Step | Shows | Check |
|---|---|---|
| 0 | blanked, LED rail off | Dark |
| 1 | rows scanning, zero column current (brightness 0) | No LED may light |
| 2 | single LED top-left, brightness 1 | Only that LED |
| 3 | single LED bottom-right, brightness 1 | Only that LED |
| 4 | row walk top to bottom, brightness 8 | One row at a time, in order |
| 5 | column walk left to right, brightness 8 | One column at a time, in order |
| 6 | orientation marker (block top-left, long arm along the top) | Orientation |
| 7 | checkerboard, brightness 8 | No stuck or missing LEDs |
| 8 | inverse checkerboard, brightness 8 | The complement of step 7 |
| 9 | all on, brightness 8 | Record supply current and VLED |
| 10 | all on, brightness 32 | Record supply current and VLED |
| 11 | all on, brightness 128 | Record supply current and VLED |
| 12 | scan stall: one LED lit with LAT stopped | The one-shot blanks it; see below |

The press after step 12 returns to step 0 (blanked).

Step 12 runs the scan-stall test once on entry, and again on each ACTION
press. One LED (top-left) is lit after a single LAT pulse with no further
pulses, so only the external one-shot can blank it. The firmware times
MTX_SAFE_SENSE from the pulse; a timer re-blanks the LED through PPI after
3000 us if the one-shot does not.

| Line | Meaning |
|---|---|
| `Matrix stall: one-shot blanked the LED N us after the last LAT pulse` | Pass if N is inside the calculated 760 to 1410 us; confirm on a scope that the LED current stops |
| `Matrix stall: CHECK, outside the calculated 760-1410 us; measure the one-shot on a scope` | Blanked, but not at the calculated width: measure it before relying on it |
| `Matrix stall: FAIL, MTX_SAFE_SENSE still low after 3000 us; the timer re-blanked it...` | The one-shot did not blank. Stop: do not scan the matrix |
| `Matrix stall: FAIL, MTX_SAFE_SENSE did not drop at the LAT pulse (one-shot not triggered)` | The LAT pulse did not trigger the one-shot; the LED was not lit |

The debugger-halt, reset-during-scan and ARM-low cases of the stalled-scan
check are done from outside the firmware (bringup.md).

ACTION logs `Matrix bench: STOPPED (blanked)` and resumes with `Matrix bench:
resumed`. `Matrix bench: driver error N (matrix blanked); back to step 0` is a
driver fault: stop and capture the log. Load shedding is off in this add-on;
the supply's current limit protects the board.

## Main image

| Line | Meaning |
|---|---|
| `matrix intentionally disarmed; service loop remains active` | Default Main image: the matrix is off by design (not a fault) |
| `Power: VBAT N mV: matrix ceiling N (<state>, N failed re-enables)` | Load shedding changed state: `normal`, `shed` or `re-enabling`. With the matrix loading the rail, the reading is marked `loaded` (Main + matrix) |
| `..., LOW BATTERY` | Below the low-battery threshold (PROPOSED values, docs/power.md) |
| `Power: battery low; System OFF until USB is attached or the switch is cycled` | Entering System OFF (measure the off current with the debugger detached) |
| `Power: USB attached; staying on` | System OFF skipped because USB is present |
| `Power: charger not found yet; System OFF deferred` and other `staying on` lines | System OFF refused for a safety reason; the badge stays on |

Main has no verbose power logs. BLE, NFC and display checks follow the
checklist and [validation.md](validation.md).
