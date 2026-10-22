# Motion and fluid

The IMU drives four-way display rotation and a particle fluid effect.
Firmware uses fixed-point particles in a 32 × 32 grid, with bounded collision
work and no allocation. This is a visual effect rather than a FLIP/PIC solver.

## Implementation

- 16–192 particles; 20 ms simulation steps, at most four catch-up steps.
- A sensor worker handles data-ready events, up to 50 fetches/s, with a 40 ms
  polling fallback. ISR callbacks only signal work. NFC and IMU share a bus mutex.
- Automatic rotation uses filtering, hysteresis, and 250 ms dwell. QR locks
  orientation on entry; text, artwork, and audio follow subsequent rotation.
- Samples older than 250 ms freeze Fluid and show `IMU`. Other content keeps
  its last orientation. Safe mode leaves the sensor worker off.

The app exposes rotation, response, and particle count. It preserves the
configured sensor-axis mapping; mount the loose IMU consistently with that map.
Settings persist and work without the phone.

## Try it

Build/flash [Prototype](../firmware/nrf5340_prototype/README.md) and install the
[app](../app/README.md#development). Recording is not needed for motion tests.

1. Refresh settings and check that the IMU is ready.
2. Select Fluid. Tilt in four directions; particles should settle toward the
   corresponding edge. Try low/high response and particle count.
3. Test automatic and manual quarter-turn rotation. Flat/diagonal poses should
   not cause repeated flips.
4. Enter QR, rotate the fixture, and scan. QR should keep its entry orientation
   until the mode is left and entered again.
5. With no NFC transaction running, disconnect the IMU. Check the stale-sample
   indication and that buttons/BLE remain usable. Reconnect; reboot if needed.
6. Restart without erasing flash and verify the saved settings.

Record axis mapping, interrupt delivery, frame time, and shared-bus behavior
under BLE/audio load. These still need physical testing on Rev A.
[Software test record](../validation/results/2026-10-15-dk-p4-motion/README.md)
