# First DK checks (2026-10-11)

This is the old first-step procedure, kept for the record. Today's buttons are in
the [firmware guide](../firmware.md#buttons).

Six checks: the default QR code and name, short and long presses, brightness
kept across reset and power loss, factory reset from the boot button, safe
mode after a forced watchdog loop, and the optional peripherals.

All six were reported passing by hand. The saved
[RTT capture](../../validation/results/2026-10-11-dk-p0-foundation/README.md)
shows a normal QR boot and the healthy counter clearing at 30.111 s, but not
each button or reset sequence. That image predates later display and settings
recovery fixes, so it says nothing about current firmware or Rev A.

The watchdog test stopped feeding an 8-second watchdog until three resets in
a row selected safe mode, then checked a healthy 30-second run.
