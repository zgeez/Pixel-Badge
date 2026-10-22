# System tests

Use the assembled badge and a recorded firmware/configuration. Keep acceptance
limits proposed until results establish the operating margin.

## 10. Battery runtime

Start fully charged and run a stated brightness/mode/audio/BLE profile to the
normal-stop condition. Log usable energy, temperatures, and elapsed time.
Eight hours is a target; capacity alone does not establish it.

## 11. Mechanical wearability

Check pack insulation/swelling clearance, leads, connector access, thread/cap
fit and repeated service, magnet retention, and button travel. Test magnetic
and lanyard use in both orientations, including walking/bending and a range
of clothing. The carrier takes the lanyard load. Recheck NFC/BLE, optics, and
thermals with the final stack. Accepted CAD dimensions do not establish retention.

## 12. Power cycling, brownout, and recovery

Calibrate VBAT against a meter with a current-limited battery simulator. Sweep
loaded/relaxed voltage, USB presence, startup, and temperature; include probe
loading, ADC settling, off-state leakage, and missing/stale/invalid samples.

Under worst-case display/radio load, capture VBAT, logic rail, LED enable/current,
and sample-to-shed latency. Establish margin above cell/protector/regulator limits.
Test rebound/noise, restart near empty, 100 mA USB supplement behavior, qualified
recovery, ramp-up, and retry backoff. Recovery must not create repeated resets
or drain an empty pack. Repeat on the final image and measure System OFF/wake.

Keep independent battery protection active. [Proposed policy](../power.md#battery-state-measurement)
