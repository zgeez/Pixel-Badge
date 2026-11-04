# Display tests

Record each run the [usual way](../validation.md#recording-results).

## QR scan reliability

The code is QR Version 1 with a full quiet zone, 29 × 29 LEDs. Test the real
contact link with the diffuser fitted. Vary brightness, polarity, distance,
angle, lighting, phone and reader app, and record decode times and failures.
Results from the HUB75 bench don't carry over. Pass limits aren't agreed yet;
the proposal is 9 of 10 within 1 s and 10 of 10 within 3 s.

To compare polarities, add `configs/qr-polarity-bench.conf` to a Main build.
In QR mode a short Action press then swaps lit-light and lit-dark modules.
It isn't saved. Change brightness from the app.

## Matrix timing and ghosting

- Capture SCK, LAT, BLANK, a row select and the LED current. Check row order,
  blanking at row changes, frame rate, ghosting and rolling-shutter effects.
- Repeat with BLE, audio and motion running, and during flash writes. Late
  work must lengthen the dark time, never the lit pulse.
- Halt the CPU mid-scan, reset it, and pull ARM low. Each must blank at once.
- Stop LAT and time the one-shot at room, hot and cold. Board 2 measured
  969–974 µs at room temperature, against 0.76–1.41 ms calculated.
- With a full row lit, measure each TLC output at the lowest VLED and battery
  voltage, cold. Keep at least 0.8 V of headroom.

The LED datasheet gives forward voltage at 5 mA only, so it can't tell you
the headroom at 18 mA. [Sources](../references.md).
