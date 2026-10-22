# Initial foundation checks

Historical P0 procedure, 2026-10-11. Current controls are in the
[firmware guide](../firmware.md#settings-and-controls).

The six checks covered default QR/name output, short/long button classification,
brightness persistence over reset/power loss, boot-button factory reset,
forced-watchdog safe mode, and optional peripheral checks.

Manual notes report all six passed. The saved
[RTT capture](../../validation/results/2026-10-11-dk-p0-foundation/README.md)
shows normal QR boot and healthy-counter clearing at 30.111 s; it does not
capture each button/reset sequence. The image was the initial working tree,
before later display/schema recovery fixes. Treat this as evidence for that
image rather than current firmware or Rev A.

The historical watchdog test stopped feeding an eight-second watchdog until
three consecutive resets selected safe mode, then checked a healthy 30-second
interval. Later changes need a fresh run with their own image identity.
