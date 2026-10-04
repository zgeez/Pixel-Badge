# Rev A bring-up checklist

Copy this checklist into a bring-up issue per board. Record measured values,
equipment, and firmware commit next to each item. Stop at the first failure in
the power section.

Board: ____  Serial/batch: ____  Firmware commit: ____  Date: ____

## Before power

- [ ] Visual inspection (solder bridges, missing/rotated parts, polarity marks)
- [ ] Resistance from each supply rail to ground
- [ ] Battery connector polarity
- [ ] Assembly matches BOM and placement

## Power

- [ ] First power from current-limited bench supply
- [ ] Each rail voltage
- [ ] Idle current
- [ ] Charger behavior
- [ ] Battery-powered operation

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
- [ ] NFC tag read (badge on and switched off)

## Mechanical

- [ ] Fit in enclosure/mount
- [ ] Magnetic mount retention

## Issues found

| # | Description | Severity | Workaround | Issue link |
|---|---|---|---|---|
| | | | | |
