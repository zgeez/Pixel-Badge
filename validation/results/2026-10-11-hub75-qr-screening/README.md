# QR screening on the HUB75 bench panel

Screening of Version 1 QR codes on the DK's 64 × 64 P2 HUB75 panel (2.0 mm
pitch, the badge's pitch), using a 32 × 32 window. Representative payloads,
not the badge's final URL. Results go in `results.csv`, one row per setting.

| Item | Value |
|---|---|
| Firmware | `firmware` app built with `firmware/configs/qr-screening.conf` (working tree before the QR-mode commit; see the record in `docs/validation.md`) |
| Codes | `scripts/qr/gen_qr_test_codes.py`: V1-L and V1-M `HTTPS://EXAMPLE.COM`, V1-Q `HTTPS://IANA.ORG`, alphanumeric mode, 21 × 21 modules at offset 5 (quiet zone 5 and 6 modules); each decoded back by ZXing-C++ in both polarities |
| Settings | 18: 3 codes × normal/inverted × brightness 255, 64, 16 (row on-time fraction); refresh 195 Hz (160 µs row slot, 1/32 scan) |
| Controls | DK Button 1 next setting, Button 2 previous; RTT logs the setting number |
| Display | Panel turned so the orientation marker reads top-left; normal polarity lights the background and quiet zone, inverted lights the dark modules |

## Procedure (stage B screening)

One phone, native camera app, 40 cm, straight on, normal indoor light, 10
trials per setting:

1. Camera open, pointed away from the panel.
2. Point at the panel. Count the time from the code being fully in view to the
   link prompt appearing: up to 1 s, up to 3 s, or no prompt within 3 s (fail).
3. Point away for at least 2 s before the next trial. Restart the camera app
   every 10 trials.
4. Record the counts in `results.csv` and anything notable (glare, banding,
   partial reads) in `notes`.

A second phone recording at 30 fps or more gives exact times; counting by eye
is enough for screening but is recorded as such. Proposed pass rule (not yet
accepted): at least 9 of 10 within 1 s and 10 of 10 within 3 s.

## Result (2026-10-11, owner)

Phone: Nothing 4a Pro (owner's wording), Google Lens, 3 tries per setting.
All 18 settings decoded in all tries. The scanning distance had to be adjusted
to the matrix brightness and the room lighting, so the fixed 40 cm condition
was not held, and decode times were not recorded.

Deviations from the procedure above: 3 tries instead of 10, Google Lens
instead of the native camera, distance varied. This is a screening pass with
representative payloads, not the acceptance test; the distance needed per
brightness level is the open item for the acceptance run.
