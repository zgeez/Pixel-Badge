# Electrical tests

Start from the [bring-up checklist](../bringup.md) and the
[expected values](../bringup-expected.md), on a current-limited supply. Stop
at the first rail or current you can't explain.

## Electrical checks

- Check the pack's identity, protector and polarity before charging. Use a
  battery simulator for fault cases, never a real short.
- Measure rails, start-up, reset and off states, ripple, and back-powering
  between domains. VLED must stay off until enabled.
- Plug USB-C both ways, with and without the pack, switch on and off,
  through host configuration and suspend. Compare the input current with the
  [USB policy](../power.md#usb).
- Scope the CC sense inputs through power-up and power-down, and check the
  small threshold margin against ADC error.
- Measure the attach inrush against the 10 µF ∥ 44 Ω reference. Nominal
  capacitance alone doesn't prove it.
- Exercise the TS thresholds with a probe on the cell: cold cell on a warm
  board, hot cell on a cool board, and the transitions. The board NTC must
  not allow charging outside the cell's rating.
- Look at the charger and regulator switch nodes and inductor currents at
  start-up, minimum input, full load and overload.

Board 1 charges with `configs/charge-bench.conf` (480 mA, attended) until the
thermal test passes. The product image keeps charging off.

## Current

Measure average and peak current per mode and brightness, with BLE, audio,
uploads, charging-only and System OFF.

For a discharge curve, build Main with `configs/battery-log.conf`. Ten
seconds after USB is unplugged, the badge writes a record every 120 s to the
flash reserve: both VBAT readings, mean battery current, brightness cap,
temperatures and power flags. It stops when USB comes back or the badge
turns off, and holds about 51 hours. Keep the mode and brightness fixed.

```powershell
python scripts/bench/battery_log.py read --csv build/battery-log.csv
python scripts/bench/battery_log.py clear
```

Firmware never erases the log. `clear` refuses a chip with a bootloader, such
as the Pro Micro, so it can't damage one.

## Thermal

Log the cell, board NTC, charger, boost and driver temperatures through a
full charge and through the heaviest display and radio load, open and in the
carrier. Note the room temperature, duration and any hot spots.
