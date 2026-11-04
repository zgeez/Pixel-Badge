# Rev A bring-up, boards 1 and 2

Dates: 2026-10-30 to 2026-11-03. Hardware: two JLCPCB-assembled Rev A boards
(E73-2G4M08S1C nRF52840 module), PKCELL LP785060 2500 mAh pack. Source:
unmerged `fw/imu-config-readback`, based on main `b9a5fc1`. Probe: nRF5340 DK
debug out (J-Link). Supply: bench DC supply on the battery connector for first
power, then USB and the pack. Readings come from RTT logs, SWD register reads
and a handheld meter; outcomes the operator reported by eye are marked
*observed*. Raw logs are kept locally and not published.

## Summary

| Area | Result | Scope |
|---|---|---|
| First power | No rail shorts; 3V3 3.3 V; 2 mA idle at 3.7 V before firmware | Both boards; meter resolution 1 mA |
| Diagnostics image | "Bring-up checks done: 0 failed" on board 2: LFXO, charger ID, TLC chain readback, IMU identity and 100 Hz data-ready, microphone data | Board 1 failed LFXO and matrix checks until reworked (below) |
| Assembly defect | Board 1: E73 pads 11, 13, 15, 17 open; reflowed by hand | Found by firmware checks and SWD, confirmed by continuity |
| Matrix | Hardware-sequenced scan at about 215 frames/s (was 33); no flicker *observed* | Board 1 |
| Stall blanking | One-shot blanked the LEDs 969-974 µs after the last latch (calculated 760-1410 µs) | Board 2, firmware-hang case |
| Halt and reset | Debugger halt and reset leave the matrix dark; ARM low gives -EIO within 5 ms | Board 1 |
| USB and charging | Enumeration; input limit follows USB configuration; 496 mA charge at ICHG 480 mA, TDIE 28.5 °C | Charger ADC readings; no external current meter |
| Discharge | 22.4 h to load shed at QR, 50 % brightness, about 140 mA | One run; capacity by charger ADC is unverified |
| Load shedding | LED rail turned off at loaded VBAT < 3.40 V; badge stayed up | Real pack |
| BLE | LE Secure Connections passkey pairing, security level 4, app control | Board 1 |
| NFC | Tag set up over I2C; phone read and app URL change *observed*; QR and NFC agree at boot | Range not measured |
| Recording | Encrypted audio saved and played back in the app *observed* | Bench build; product default off |

Not yet tested: System OFF on the real pack, NFC range, BLE range, thermal
probe during charging, optical brightness, host suspend (HIZ). Board 2 is
down with a 3V3 regulator fault that has not been localised.

## Board 1: slow clock and dead safety path

The IMU reported about 365 data-ready interrupts per firmware second instead of
100. Register read-back matched the configuration. Counting RTC1 over SWD for
10 s gave 91,401 ticks: the 32.768 kHz crystal ran at about 9.1 kHz while the
clock driver still reported "running, crystal". With the RC oscillator selected
the RTC counted 327,694 and the IMU gave exactly 100 per second. Board 2's
crystal was correct, so the fault was board-specific.

The matrix failed its first-write check on the same board: after ARM and a
latch pulse, `MTX_SAFE_SENSE` must go low and stayed high. With the core halted,
ARM was driven high and LAT toggled from TIMER4 through PPI/GPIOTE. The chip's
own IN register showed both pins switching, but the meter read 0 V at the LAT
test point and the ARM resistor. Both faults sit on the same edge of the module
as the crystal pins. Continuity showed pads 11, 13, 15 and 17 unsoldered.
After a hand reflow the RTC counted 327,744 ticks in 10 s on the crystal and
`SAFE_SENSE` followed the latch.

## Matrix scan

The original driver ran one thread wake-up per row slot. At its first priority
it never blocked and starved every lower thread (the power-manager heartbeat
froze while the RTC advanced; PC sampling over SWD showed only scan code). At
a lower priority it ran about 33 frames/s, with visible flicker.

The replacement pre-encodes the 256 slots of a frame and lets the hardware run
them: SPIM2 EasyDMA ArrayList, TIMER3 compares through PPI/GPIOTE for SPI start,
latch and blanking, and a TIMER4 counter that stops TIMER3 after each 32-row
plane. The CPU takes one interrupt per plane. SPIM3 is avoided because of
nRF52840 anomaly 198. Flash erase/write stalls stop the scan after its plane,
as designed; the plane interrupt now checks `SAFE_SENSE` only when it runs on
time, so a stall no longer reports a false display fault.

## Power

Discharge from full, QR at 50 % brightness (bench battery log, 2-minute
records in flash): 4156 to 3452 mV loaded over 21.2 h, mean battery current
rising from 128 to 149 mA. The brightness ceiling dropped to half at 3582 mV.
A second session reached the shed threshold 1.2 h later. Battery-level bands
were recalibrated from this curve: 3485 / 3560 / 3690 / 3920 mV for about
12 / 35 / 60 / 85 % of the drawn charge. These are for this load; brighter
content sags further.

While shed, the badge drew about 12 mA. About 6-8 mA of that is the two
TLC59283 column drivers, which are on the always-on 3.3 V rail and have no
sleep mode (ICC 3 mA typical each, SBVS199C).

Charging stopped above about 26 °C at first. The BQ25620 TS register defaults
place `V_TS_PREWARM` at 48.4 % of REGN; on this board's 10 k thermistor network
without RT2 that is about 26 °C, and firmware blocked every non-normal zone.
Firmware now allows normal, pre-cool and pre-warm, and keeps its own 38 °C stop.

## Fixes made during bring-up

- Hardware-sequenced matrix scan; scan thread priority below the application.
- Latch pulse and `SAFE_SENSE` read under an IRQ lock.
- VLED drain wait after a quick reset instead of latching a fault.
- USB: control-transfer buffer pool sized for the host's requests; USB
  power-ready event replayed when VBUS is present at boot.
- Input-limit filter for the attach transient.
- Charger TS zones as above.
- Charging flag held through the 0.5 s pack-presence probe.
- Rev A IMU axes fixed (+X right, -Y down).
- Artwork staging continues through a brief display recovery; Apply still
  waits for a healthy display.
