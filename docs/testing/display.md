# Display tests

Record the board/image, instruments, settings, raw captures, and results using
the [validation format](../validation.md#recording-results).

## 1. QR scan reliability

Use Version 1 with the full quiet zone: 21 + 8 = 29 pixels. Reject oversized
payloads. Test the actual contact URL across brightness, polarity, distance,
angle, lighting, phones, and reader apps. Record decode times and failures.
Repeat with the assembled Rev A optics; HUB75 screening is not transferable.
Numerical pass limits still need agreement.

## 3. Matrix timing, flicker, and ghosting

- Capture SCK, LAT, BLANK, row select, and LED current. Check row mapping,
  transition blanking, refresh, ghosting, and rolling-shutter artifacts.
- Repeat under BLE/audio/motion load and during flash operations. Late work
  must extend blanking rather than the illuminated pulse.
- Stop LAT and measure independent SAFE blanking at room/hot/cold conditions.
  The calculated one-shot range is 0.76–1.41 ms; measure the actual board.
- With a full row on, measure every TLC output at minimum VLED/battery and
  low temperature. Keep at least 0.8 V design headroom with regulated current;
  otherwise raise VLED within component limits or reduce current and repeat.

The LED datasheet specifies forward voltage at 5 mA, so higher-current typical
curves do not establish headroom. [Hardware sources](../references.md).
