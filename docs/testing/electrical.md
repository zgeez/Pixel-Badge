# Electrical and power tests

Start with the [bring-up checklist](../bringup.md), current-limited supplies,
and [expected values](../bringup-expected.md). Stop at the first unexplained
rail/current result. Record limits from the confirmed pack specification.

## 2. Electrical correctness

- Verify pack identity, protector, polarity, and recovery limits before charging.
  Use a battery simulator/controlled fixture for faults.
- Measure rails, startup/reset/off states, ripple, and cross-domain back-powering.
  VLED must stay off until deliberately enabled.
- Test C-to-C attachment/enumeration in both orientations, battery present/absent,
  switch on/off, plug/unplug, host configuration, and suspend/resume. Check
  actual input current against [USB policy](../power.md#usb-supply-connections).
- Scope CC/ADC inputs against VDD through power-up/down; verify effective Rd
  loading and the nRF52840 input limits. Calibrate the small CC threshold margin.
- Measure attach charge/droop against the 10 µF ∥ 44 Ω reference. Nominal
  capacitance alone does not establish inrush compliance.
- Test charger gates/readback, pack-presence probing, missing pack, and pack
  removal before the watched 480 mA first charge. Check USB limit and charge
  current separately.
- Exercise TS thresholds and watchdog behavior with controlled resistance/
  temperature. With a thermocouple on the cell, test cold-cell/warm-board,
  hot-cell/cool-board, and temperature transitions. The board NTC must not allow
  charging outside the confirmed cell rating; otherwise revise sensing/limits.
- Measure module supply/clock behavior and regulator/charger switch-node,
  inductor peak/ripple currents at startup, minimum VIN, maximum load, and overload.
- Check independent pack protection and recovery without uncontrolled shorts.

Keep charging disabled until board-to-cell thermal offset and charging behavior
are understood. [Circuit settings](../power.md) are design values, not test results.

## 4. Current and power

Measure average and peak current for each mode/brightness, BLE activity, audio,
uploads, charging-only, and System OFF. Include total input power and conversion
losses when checking recovery brightness limits.

For a discharge curve, build Main with `configs/battery-log.conf` (bench only).
After USB is unplugged for 10 s, the badge records SAADC and charger-ADC VBAT,
mean IBAT, matrix ceiling, temperatures and power flags every 120 s into the
24 KiB flash reserve, until USB returns or System OFF (about 51 h capacity).
Keep the display mode and brightness fixed for the run. Read it with
`python scripts/bench/battery_log.py read --csv <file>` and clear it with
`... clear`; firmware never erases the log.

## 9. Thermal

Log cell, board NTC, charger, boost, and driver temperatures during full charge
and worst-case display/radio load, both open and with the carrier fitted.
Record ambient temperature, duration, hot spots, and thermal regulation.
