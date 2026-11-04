# Bring-up log lines

What to flash for each step and what the RTT log should say. Strings in
`code` are printed exactly; Zephyr adds a timestamp and module prefix.
Use with the [checklist](bringup.md).

## Images

| Step | Build |
|---|---|
| Electrical checks, sensors, matrix outputs off | Diagnostics |
| Staged matrix bench | Diagnostics + `matrix-stepper.conf` |
| Pack probe and first charge | Diagnostics + `diagnostics-charge.conf` |
| BLE, NFC, power states (matrix off) | Main |
| Display, pairing, load shedding | Main + `matrix-bench.conf` |

Add-ons go in `-ExtraConfFile` with their own `-BuildDir`
([commands](../firmware/rev_a_nrf52840/README.md)). Log every run with
`dev.ps1 rtt -Target RevA`; files land in `build/rtt/`.

## Diagnostics, first boot

The checks run once, two seconds after `smart-led-badge starting on pixel_badge_reva/nrf52840`.

| Line | Pass | If not |
|---|---|---|
| `Reset cause ... (pin ., por ., watchdog ., ...)` | `por 1` after power-up | `watchdog 1` on a first boot: save the log and stop |
| `LFCLK running, source 32.768 kHz crystal` | Present | Then also count RTC1 over SWD; board 1's crystal ran at 9 kHz while this line looked fine |
| `Charger: BQ25620 rev N found` | Present | `FAIL, no BQ25620`: I²C, supply or address |
| `Power: VBAT N mV via divider` | About 0 with no pack, else close to a meter | Divider or ADC setup |
| `Power: VLED N mV (LED rail is off at start: expect ~0)` | About 0 | `FAIL, VLED ... while LED_EN is low` after 60 s: stop |
| `Power manager started: N failed checks` | `0 failed checks` | Find the matching FAIL line |
| `Matrix: MTX_SAFE_SENSE 1` | `1` | `0`: the safe path isn't safe. Stop |
| `Matrix: TLC chain readback OK (offset N bits)` | Offset 31–33 on all five lines | `MISO stuck`: chain or MISO |
| `IMU: identity OK` | Present | `FAIL, not ready`: bus, power or identity |
| `IMU: N data-ready interrupts in 1 s (ODR 100 Hz)` | 80–120 | Far above 120: check the 32 kHz clock |
| `NFC tag: guarded read result 0` | `0` | Tag supply, I²C or address. Read only |
| `Mic: data present; confirm by sound` | Level rises when you clap | `FAIL, no microphone data` |
| `Bring-up checks done: N failed` | `0 failed` | |

The HC595 row chain can't be read back with VLED off. Check SER_V, SRCLK_V
and RCLK_V on a logic analyzer.

Every 5 s Diagnostics then logs rails and CC, charger telemetry, the TS zone
and D+/D- result (`SDP 500mA` on a computer, `DCP 1.5A` on a wall charger),
button and switch edges, IMU, field detect and mic readings.

## USB

| Source | Log |
|---|---|
| Computer | `SDP 500mA`, `USB configuration 1`, `Charger: input limit 500 mA` |
| 1.5 A or 3 A USB-C charger | CC `1.5A/3.0A`, `input limit 1500 mA`, no enumeration |
| BC1.2 wall charger | `DCP 1.5A`, `input limit 1500 mA`, no enumeration |
| Host asleep, pack fitted | `Charger: entering HIZ`, then `leaving HIZ` on wake |

## Charging

| Setup | Pass | Stop if |
|---|---|---|
| USB, no pack | `... NO PACK` and `Charger: charging off (...)` | `CHARGING ENABLED` |
| Metered pack | `pack present`, then `Charger: CHARGING ENABLED at ICHG 480 mA` | A reason line that doesn't fit |
| Pack pulled while charging | `NO PACK` within 30 s | Charging stays on |

Each reason charging is held off is logged once as `Charger: charging off (<reason>)`:
for example `no VBUS`, `TS zone blocks charging`, `board above 38 C (cell charge limit 40 C)`,
`VBAT readings disagree (SAADC vs charger)` or `pack presence probe`.

## Matrix stepper

MODE steps through the bench and ACTION stops or resumes it. Steps: 0
blanked, 1 rows with no current, 2–3 single LEDs, 4–5 row and column walks,
6 orientation marker, 7–8 checkerboards, 9–11 all on at brightness 8, 32
and 128, 12 stall test.

| Step 12 line | Meaning |
|---|---|
| `Matrix stall: one-shot blanked the LED N us after the last LAT pulse` | Pass if 760–1410 µs |
| `Matrix stall: CHECK, outside the calculated 760-1410 us` | Blanked, but measure it on a scope |
| `Matrix stall: FAIL, MTX_SAFE_SENSE still low after 3000 us` | The one-shot didn't blank. Don't scan the matrix |

## Main

| Line | Meaning |
|---|---|
| `matrix intentionally disarmed; service loop remains active` | Default Main: not a fault |
| `Power: VBAT N mV: matrix ceiling N (<state>, ...)` | Shedding changed state |
| `Power: battery low; System OFF until USB is attached or the switch is cycled` | Going to System OFF |
