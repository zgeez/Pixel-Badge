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
- [ ] Charger behavior: no charging until firmware drives CHG_EN high, in MCU
      reset, or with the logic rail off; reset TS
      thresholds stop charging below about 0 °C and above 45 °C
- [ ] CC1/CC2 each have separate 5.1 kΩ Rd; C-to-C VBUS/charging attachment in
      both orientations, including switch-off and battery-absent states
- [ ] USB-C VBUS reaches charger input and module VBUS pin 27 through the
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
      confirm they match before connecting the cable (a DK on USB alone is 3 V);
      the target is the badge's nRF52840
- [ ] Module supply (VCC = VDDH = 3V3, DCCH open): 3V3 ripple at the module
      and stable operation at idle and with the radio active. The E73's DC/DC
      inductors are inside the module, so there are no external regulator
      nodes to probe
- [ ] Flash and run bootstrap firmware; P0.18 pin reset and NFC pins as GPIO
      configured in UICR, then reset through J501 pin 10
- [ ] RTT log output (the only log path: Rev A has no UART pads)

## MCU and clocks

- [ ] High-frequency crystal (inside the module)
- [ ] 32.768 kHz crystal with the 12 pF C405/C406 loads: starts reliably,
      frequency error measured (the load value is calculated, not measured)

## LED matrix

- [ ] All-off state
- [ ] Single-pixel and row/column test patterns
- [ ] Brightness control
- [ ] Refresh timing (see validation.md), including during BLE activity: the
      BLE controller shares the single CPU with the matrix frame restart
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
- [ ] Magnetic mount retention, landscape and portrait
- [ ] Lanyard attachment, landscape and portrait (hangs level; carrier takes the load)
- [ ] Display rotation follows the badge orientation; QR mode stays locked

## Issues found

| # | Description | Severity | Workaround | Issue link |
|---|---|---|---|---|
| | | | | |
