# System tests

Use an assembled badge with a recorded firmware build. Keep limits marked
proposed until results show the margin.

## Battery runtime

Start full, run a stated profile (mode, brightness, audio, BLE) until the
badge sheds or stops, and log energy, temperatures and time. Eight hours is
the target. Board 1 lasted 22 hours to load shed on a QR code at half
brightness, which is one light profile, not the target test.

## Wearability

Check pack insulation and swelling room, the leads, connector access, screw
and cap fit over repeated service, magnet hold, and button travel. Wear it on
the magnet and the lanyard in both orientations while walking and bending,
on a few kinds of clothing. The carrier, not the board, takes the lanyard
load. Recheck NFC, BLE, the diffuser and temperatures with the final stack.

## Brownout and recovery

Calibrate VBAT against a meter, using a current-limited battery simulator.
Sweep loaded and resting voltage, USB on and off, start-up and temperature,
and include stale or missing samples.

Under the heaviest display and radio load, capture VBAT, the logic rail, LED
enable and current, and the time from sample to shed. Test noisy rebounds,
restarts near empty, the 100 mA USB case, the step-up after recovery and the
retry backoff. Recovery must never loop through resets or flatten an empty
pack. Measure System OFF current and wake-up on the final image.
[Shedding policy](../power.md#battery-measurement).
