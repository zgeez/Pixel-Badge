# QR experiment: Version 3 with a reduced quiet zone (HUB75 panel)

Nonstandard experiment requested by the owner (2026-10-11): Version 3 codes
(29 x 29 modules) in the 32 x 32 window leave a 1-2 module quiet zone instead
of the standard 4. Not a candidate default without a separate owner-approved
deviation (validation plan, QR section).

| Item | Value |
|---|---|
| Codes | owner's links (not stored in the repository; names and lengths only): GitHub V1-L (uppercase, standard quiet zone, for comparison), GitHub V3-H, portfolio V3-M, LinkedIn V3-M, resume V3-L |
| Generation | `scripts/qr/gen_qr_test_codes.py --out-cpp` into the ignored build folder; every code decoded by ZXing-C++ on a simulated matrix render, both polarities |
| Firmware | QR screening build with `CONFIG_APP_QR_CODES_FILE` pointing at that file |
| Settings | 30: 5 codes x normal/inverted x brightness 255, 64, 16 (Button 1 next, Button 2 back); resume V3-L (44 chars) added as 25-30 after the first run |

Same procedure as the 2026-10-11 screening (3 tries per setting, Google Lens).
Record the working distance range per setting if it varies. In inverted
polarity the panel's unlit surround continues the (dark) quiet zone; in normal
polarity the lit quiet zone is only 1-2 modules wide before the dark surround.

## Result, settings 1-24 (2026-10-11, owner)

All 24 settings scanned (3 tries each, Google Lens, Nothing 4a Pro), including
every Version 3 code with the 1-2 module quiet zone, in both polarities. In a
lit room the owner had to move back for a dimmer code. Decode times and the
distance ranges were not recorded. Settings 25-30 (resume V3-L) also passed
(owner, same session).
