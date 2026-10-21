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
- [ ] Rails in order, with no battery and the LED rail off: SYS (TP201), then
      3V3_REG and +3V3 (TP203/TP204); VLED (TP206) stays at 0 V
- [ ] Each rail voltage
- [ ] Idle current
- [ ] Flash the diagnostics image (`firmware/rev_a_nrf52840/README.md`). Its
      RTT log reports reset cause, the 32.768 kHz clock source, the SAADC VBAT,
      VLED and CC readings, the charger's identity, faults, TS zone, D+/D-
      result and ADC readings, and button/power-switch edges. The status LED
      blinks while it runs. Charging stays off in this image
- [ ] Matrix dark in reset and boot: LED_EN low and BLANK high, VLED about
      0 V in the diagnostics log, no LED lit
- [ ] Charger behavior: no charging until firmware drives CHG_EN high, in MCU
      reset, or with the logic rail off; reset TS
      thresholds stop charging below about 0 °C and above 45 °C
- [ ] Pack-presence probe, before the first charge: with the charge image
      and USB-C attached but no pack, the log must report `NO PACK` and keep
      CHG_EN low; with a meter-checked pack it must report `pack present` at
      about the metered voltage. Unplug the pack while charging: CHG_EN must
      drop within the next 30 s probe
- [ ] First charge, watched: diagnostics image plus `diagnostics-charge.conf`
      (ICHG 480 mA), a meter-checked pack, and a USB-C source that advertises
      1.5 A or a BC1.2 dedicated charger (a computer port gives 500 mA once it
      configures the badge, otherwise 100 mA). The log
      must show every gate passing before CHG_EN goes high. Confirm the charge
      current on an inline USB-C power meter and the charger IBAT reading; feel
      or IR-check the charger, inductor and pack for the first 15 minutes; stop
      if anything gets hot. Scope REGN, BTST-SW and SW before any charge
      current above about 500 mA
- [ ] USB-C plug seating with the cables you will use (the receptacle face sits
      0.55 mm inside the board edge)
- [ ] CC1/CC2 each have separate 5.1 kΩ Rd; C-to-C VBUS/charging attachment in
      both orientations, including switch-off and battery-absent states
- [ ] USB-C VBUS reaches charger input and module VBUS pin 27 through the
      specified conditioning/decoupling; hot-plug and switch-off states checked
- [ ] USB enumeration: on a computer the badge appears as a CDC ACM device,
      the log shows `USB configuration 1` and the input limit rises to
      500 mA; on a wall charger it never enumerates and the D+ pull-up comes
      on only after the charger's detection result is logged
- [ ] USB suspend: sleep the host with a pack fitted; the log shows the
      charger entering HIZ, an inline meter shows VBUS current at or below
      2.5 mA, the badge keeps running (switch ON or OFF), and waking the host
      leaves HIZ
- [ ] Battery-powered operation
- [ ] Switched VBAT sensing agrees with meter; disabled/reset/off leakage and
      ADC back-powering checked; loaded low-battery shedding/recovery and invalid
      sample behavior verified (validation.md §12); final thresholds recorded
- [ ] Product power states: with USB attached and the switch OFF the log
      reports the charging-only state, BLE does not advertise and the matrix
      stays dark; switching ON restarts into normal operation, and switching
      OFF again restarts into charging-only
- [ ] Load shedding on a current-limited supply at the battery connector
      (matrix image): lowering the supply caps brightness below about
      3.55 V and turns the LED rail off below about 3.40 V; raising it to
      3.70 V or more re-enables from the lowest level in 5 s steps. Record the
      loaded and unloaded VBAT readings against a meter
- [ ] Low-battery off: on that supply with no USB, about 3.25 V unloaded
      enters System OFF (log, then no RTT); record the off current; attaching
      USB must wake the badge. A debugger keeps the nRF52840 in emulated
      System OFF, so measure with it detached
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
- [ ] Probe check before any write: with the badge unpowered,
      `dev.ps1 flash -Target RevA` must refuse because it sees the DK's
      nRF5340; with the badge powered it must see an nRF52840
- [ ] New module: `dev.ps1 recover -Target RevA -EraseAll` once (AP-Protect
      ships enabled on newer nRF52840 build codes and blocks SWD until
      ERASEALL); record the module's build code from `nrfutil device device-info`
- [ ] Flash the diagnostics image (`dev.ps1 flash -Target RevA -BuildDir
      build/reva-diagnostics`) and run it; the first boot writes P0.18 pin
      reset and NFC pins as GPIO to UICR, then reset through J501 pin 10 and
      confirm SWD still connects after a power cycle (debug port stays open)
- [ ] RTT log output (`dev.ps1 rtt -Target RevA`; the only log path: Rev A
      has no UART pads)

## MCU and clocks

- [ ] High-frequency crystal (inside the module)
- [ ] 32.768 kHz crystal with the 12 pF C405/C406 loads: starts reliably,
      frequency error measured (the load value is calculated, not measured)

## LED matrix

- [ ] All-off state
- [ ] LED-driver chain, outputs off (diagnostics image): MTX_SAFE_SENSE reads
      1 and the TLC chain readback passes at one offset; check the HC595 chain
      (SER_V/SRCLK_V/RCLK_V) on a logic analyzer
- [ ] Staged matrix bench (`matrix-stepper.conf`, current-limited supply):
      rows scanning with no LED lit, then one LED at minimum brightness, then
      walks, patterns and all-on at rising brightness; record supply current
      and VLED at each all-on step
- [ ] Stalled-scan safety: halt the MCU in the debugger mid-scan; the
      matrix must go dark within about 1-1.5 ms (monostable)
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
