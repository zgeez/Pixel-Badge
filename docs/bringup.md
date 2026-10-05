# Rev A bring-up checklist

Copy this checklist into a bring-up issue per board. Record measured values,
equipment, and firmware commit next to each item. Stop at the first failure in
the power section.

Board: ____  Serial/batch: ____  Firmware commit: ____  Date: ____

## Before power

- [ ] Visual inspection (solder bridges, missing/rotated parts, polarity marks)
- [ ] Resistance from each supply rail to ground
- [ ] Battery connector polarity
- [ ] Protected pack identity/specifications verified, or dedicated cell
      protection circuit reviewed; charger alone is insufficient
- [ ] Assembly matches BOM and placement

## Power

- [ ] First power from current-limited bench supply
- [ ] Each rail voltage
- [ ] Idle current
- [ ] Charger behavior
- [ ] USB-C VBUS reaches charger input and module VBUS pin 44 through the
      specified conditioning/decoupling; hot-plug and switch-off states checked
- [ ] USB diagnostic enumeration/data with MCU logic supply enabled
- [ ] Battery-powered operation
- [ ] Independent battery protection and recovery checked against selected
      pack/cell specifications using controlled fixtures; record limits/results

## Debug and programming

- [ ] SWD connection
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

## Mechanical

- [ ] Fit in enclosure/mount
- [ ] Magnetic mount retention

## Issues found

| # | Description | Severity | Workaround | Issue link |
|---|---|---|---|---|
| | | | | |
