# Rev A firmware

Rev A runs the app and the Bluetooth controller on one nRF52840. Board and
config files are in `firmware/rev_a_nrf52840`, and shared code is in
`firmware/common`. [Build and flash commands](../firmware/rev_a_nrf52840/README.md).

## Pins and peripherals

| Resource | Allocation |
|---|---|
| Matrix SPI | SPIM2 for the scan at 8 MHz, SPIM3 for the diagnostics readback; SCK P0.12, MOSI P0.04, MISO P0.08 |
| Matrix control | LAT P1.09, BLANK P0.05, ARM P0.06, SAFE P0.26, LED_EN P0.07 |
| Scan timing | TIMER3 slots, TIMER4 plane counter, GPIOTE for LAT and BLANK, PPI |
| IMU and charger | TWIM0 at 100 kHz, P0.15/P0.17; IMU interrupt P0.09 |
| NFC tag | TWIM1 at 100 kHz, P0.22/P0.24; VCC P1.00, field detect P1.02 |
| Microphone | PDM CLK P0.20, DIN P0.13 |
| ADC | AIN6 VBAT, AIN0 CC1, AIN1 CC2, AIN5 VLED |

All 29 GPIO are used. The 32.768 kHz crystal has external load capacitors.
The firmware writes UICR on first boot to keep P0.18 as reset and the NFC pins
as GPIO. REG1 stays in LDO mode.

`pinmap.json` lists the 43 module pads. `scripts/ci/check_reva_contract.py`
checks the compiled devicetree and flash layout against it, and can compare
U401 with a KiCad netlist.

## Display scan

Each frame is pre-encoded into 256 row slots: 32 rows times 8 brightness
planes, 8 bytes each. SPIM2 walks the slots with an EasyDMA list. TIMER3
starts each transfer and drives LAT and BLANK through PPI and GPIOTE.
TIMER4 counts rows and stops TIMER3 after each plane. The CPU takes one
interrupt per plane to set the next on-time. That's about 215 frames/s by
the firmware's count; it hasn't been checked on a logic analyzer yet.

If the CPU stalls or halts, LAT stops after at most one plane and the
external one-shot blanks the matrix. A flash erase stalls the scan the same
way, so the plane interrupt only checks `SAFE_SENSE` when it ran on time.
SPIM3 isn't used for the scan because of nRF52840 anomaly 198.

ARM stays low until VLED settles and a full row is latched. Driver errors
blank and disarm the matrix, and recovery has to show scan progress again.
Persistent errors end in a watchdog restart. In the default Main image the
matrix is deliberately disarmed. The service loop keeps running, but nothing
that needs a visible display can succeed: pairing, artwork Apply, recording.

## Flash and RAM

| Region | Start | Size |
|---|---|---|
| MCUboot | `0x00000` | 64 KiB |
| Primary slot | `0x10000` | 320 KiB |
| Secondary slot | `0x60000` | 320 KiB |
| Artwork banks | `0xb0000` | 2 × 132 KiB |
| Reserve (bench battery log) | `0xf2000` | 24 KiB |
| Settings and bond | `0xf8000` | 32 KiB |

Main uses about 90 % of the primary slot. Offsets match the prototype, but
bonds don't carry over between chips.

## Power manager

`app/power_manager.cpp` sets up the charger, follows the USB current limit,
checks the pack, samples the battery and sheds load. The policy itself is in
`services/power_policy.cpp` and has host tests. Thresholds are in
[power](power.md).

Charging needs VBUS, a healthy TS zone and fault state, register readback
that matches, charger and ADC readings that agree, and a recent
pack-presence check. The probe pauses charging, lets the charger pull about
30 mA for 60 ms, and requires VBAT ≥ 3.0 V. Charging is off unless the image is
built with `CONFIG_APP_CHARGING`.

USB enumerates after BC1.2 detection, using the Zephyr test VID/PID (fine for
a personal badge, not for distributed boards). The charger goes to HIZ while
the host is suspended. Main adds the charging-only state and low-battery
System OFF.

Diagnostics logs rails, charger state, clock and reset cause, buttons and the
matrix chain. [Bring-up checklist](bringup.md), [expected values](bringup-expected.md),
[log lines](bringup-logs.md).

## nRF5340 to nRF52840

The module changed because of availability ([ADR 0015](decisions/0015-nrf52840-e73-module.md)).
The radio and app now share one 64 MHz core and 256 KiB of RAM. The PDM
captures natively at 16 kHz instead of 32 kHz plus decimation; audio packets
are unchanged. Bounded buffers and flash-backed artwork keep the same content
limits.

The [Pro Micro bench](../firmware/bench/promicro_nrf52840/README.md) runs the
shared BLE, settings and rendering on a bare nRF52840 board with a USB pairing
display. It keeps the board's UF2 bootloader and isn't a Rev A image.
