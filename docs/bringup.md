# Rev A bring-up checklist

Copy this checklist into a bring-up issue per board. Record measured values,
equipment, and firmware commit next to each item. Stop at the first failure in
the power section.

Board: ____  Serial/batch: ____  Firmware commit: ____  Date: ____

## Before power

- [ ] Visual inspection (solder bridges, missing/rotated parts, polarity marks)
- [ ] Resistance from each supply rail to ground
- [ ] Battery connector polarity: meter the pack plug before first connection
      (red wire must land on header pin 2, BAT+)
- [ ] Protected pack identity/specifications verified, or dedicated cell
      protection circuit reviewed; charger alone is insufficient
- [ ] Assembly matches BOM and placement

## Power

- [ ] First power from current-limited bench supply
- [ ] Each rail voltage
- [ ] Idle current
- [ ] Charger behavior: no charging until firmware drives CE low; reset TS
      thresholds stop charging below about 0 °C and above 45 °C
- [ ] CC1/CC2 each have separate 5.1 kΩ Rd; C-to-C VBUS/charging attachment in
      both orientations, including switch-off and battery-absent states
- [ ] USB-C VBUS reaches charger input and module VBUS pin 44 through the
      specified conditioning/decoupling; hot-plug and switch-off states checked
- [ ] USB diagnostic enumeration/data with MCU logic supply enabled
- [ ] Battery-powered operation
- [ ] Switched VBAT sensing agrees with meter; disabled/reset/off leakage and
      ADC back-powering checked; loaded low-battery shedding/recovery and invalid
      sample behavior verified (validation.md §12); final thresholds recorded
- [ ] Independent battery protection and recovery checked against selected
      pack/cell specifications using controlled fixtures; record limits/results

## Debug and programming

- [ ] SWD through the nRF5340 DK debug out (P19, 10-pin 1.27 mm flat cable):
      DK SB47 open; DK on USB for the debugger; 3.3 V on DK P21 with SW10
      VEXT->nRF on and SW9 at VDD; meter the DK VDD and the badge 3V3 and
      confirm they match before connecting the cable (a DK on USB alone is 3 V)
- [ ] DC/DC regulators (DCC/DECR, DCCD/DECD): output ripple and stable operation
      at idle and with the radio active
- [ ] Flash and run bootstrap firmware
- [ ] RTT log output
- [ ] UART test pad output

## MCU and clocks

- [ ] High-frequency crystal
- [ ] Low-frequency crystal (if fitted)

## LED matrix

- [ ] All-off state
- [ ] Single-pixel and row/column test patterns
- [ ] Brightness control
- [ ] Refresh timing (see validation.md)
- [ ] QR code display

## Sensors

- [ ] IMU detected and producing plausible data
- [ ] PDM microphone producing plausible data

## Wireless

- [ ] BLE advertising and connection
- [ ] Configure a new URL through the intended MCU path, verify NTAG I²C EEPROM
      update, exact NFC URI while powered and passively after switch-off/power
      removal, and equality with the decoded QR (validation.md §6)
- [ ] NFC read-range/tap tests with battery, magnets, and carrier installed
- [ ] Provisioned RF write protection rejects unauthenticated URL/metadata/
      bypass writes in fresh RF sessions, powered and unpowered; public reads
      and subsequent MCU/I²C URL updates still work (validation.md §6)

## Mechanical

- [ ] Fit in enclosure/mount
- [ ] Magnetic mount retention

## Issues found

| # | Description | Severity | Workaround | Issue link |
|---|---|---|---|---|
| | | | | |
