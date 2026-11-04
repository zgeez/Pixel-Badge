# Bring-up checklist

For a new board. Copy it into an issue, and record the values, equipment and
firmware commit next to each item. Stop at the first power failure.
[Log lines for each step](bringup-logs.md), [expected values](bringup-expected.md).
Board 1 went through all of this; its results are in the
[bring-up record](../validation/results/2026-11-03-rev-a-bringup/README.md).

Board: ____  Firmware commit: ____  Date: ____

## Before power

- [ ] Look for bridges and missing, rotated or reversed parts.
- [ ] Measure each rail's resistance to ground.
- [ ] Meter the pack plug: red must land on header pin 2 (BAT+).
- [ ] Confirm the pack is protected and know its limits.

## Debug and programming

- [ ] Wire SWD from the nRF5340 DK debug out (P19, 10-pin 1.27 mm cable).
      Open DK SB47, power the DK from USB, feed 3.3 V into DK P21 with SW10
      on VEXT->nRF and SW9 at VDD. Meter the DK VDD and the badge 3V3 and
      make sure they match before connecting the cable.
- [ ] With the badge off, `dev.ps1 flash` must refuse (it sees the DK's own
      nRF5340). Powered, it must see an nRF52840.
- [ ] New module: run `dev.ps1 recover -Target RevA -EraseAll` once.
      AP-Protect ships on and blocks SWD until then.
- [ ] Flash Diagnostics. Its first boot writes UICR (pin reset, NFC pins as
      GPIO). Reset through J501 pin 10, power-cycle, and check SWD still connects.
- [ ] Capture RTT with `dev.ps1 rtt -Target RevA`. There's no UART.

## Power

- [ ] First power from a current-limited supply on the battery connector.
- [ ] SYS (TP201), then 3V3 (TP203/TP204), come up. VLED (TP206) stays at 0 V.
- [ ] Note the idle current.
- [ ] Diagnostics ends with `Bring-up checks done: 0 failed`. If the IMU
      reports far more than 100 interrupts a second, or the matrix chain
      fails, suspect the module joints first. That was board 1's fault.
- [ ] The matrix stays dark through reset and boot.
- [ ] Pack probe: with USB and no pack, the log says `NO PACK` and charging
      stays off. With a pack, it reads about the metered voltage.
- [ ] First charge, watched: Diagnostics plus `diagnostics-charge.conf`
      (480 mA), a 1.5 A USB-C source. Every gate must pass in the log before
      CHG_EN goes high. Check the current on a USB-C meter, and feel the
      charger, inductor and pack for 15 minutes. Scope REGN, BTST-SW and SW
      before charging above about 500 mA.
- [ ] VBAT from the divider agrees with a meter at the pack; record the offset.
- [ ] USB: a computer enumerates it and the limit rises to 500 mA; a wall
      charger never enumerates. Try both plug orientations.
- [ ] Host suspend: the charger enters HIZ and the badge keeps running.
- [ ] Switch off with USB: charging only, no BLE, dark matrix.
- [ ] Load shedding on a supply at the battery connector: brightness caps
      below about 3.55 V, the LED rail goes off below about 3.40 V, and it
      comes back from 3.70 V.
- [ ] Low battery: about 3.25 V unloaded with no USB enters System OFF.
      Measure the off current with the debugger unplugged.

## Clocks

- [ ] The 32.768 kHz crystal starts and runs at the right rate. Count RTC1
      over SWD for 10 s: it should be about 327,680.

## LED matrix

- [ ] Outputs off: `MTX_SAFE_SENSE` reads 1 and the TLC chain reads back.
- [ ] Stepper bench (`matrix-stepper.conf`, current-limited supply): rows
      with no LED lit, one LED, walks, patterns, then all-on at rising
      brightness. Record current and VLED at each all-on step.
- [ ] Stall test (stepper step 12): the one-shot blanks a lit LED in about
      1 ms. Halting the CPU mid-scan must also blank it.
- [ ] Brightness, refresh with BLE active, and the QR code with the diffuser.

## Sensors and wireless

- [ ] IMU reads about 1 g at rest and turns the display the right way.
- [ ] Microphone level rises when you clap.
- [ ] BLE pairs with the passkey on the matrix and the app works.
- [ ] Change the link from the app: the QR code and the NFC tag must match,
      powered and with the badge off. A new tag needs the wired setup first
      ([NFC tools](../scripts/nfc/README.md)).
- [ ] NFC range with the battery, magnets and carrier fitted.

## Mechanical

- [ ] Fits the carrier; the slide switch works in both positions.
- [ ] Magnet and lanyard hold, in portrait and landscape.

## Issues found

| # | Description | Severity | Workaround |
|---|---|---|---|
| | | | |
