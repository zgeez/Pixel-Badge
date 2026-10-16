# P4 motion and fluid prototype

The nRF5340 DK prototype now includes IMU-driven Fluid mode and four-way
display orientation, with matching Pixel Badge controls. This is software
verification; the matching normal firmware and app are deployed, with physical
motion acceptance still pending.
The nRF52840/Rev A port remains separate. The currently deployed image enables
Record/Stop through the existing recording-bench fragment, alongside all P4
features. Default builds still retain their recording gate.

## Behavior

Fluid is mode 6. It uses 16–192 fixed-point particles in a 32×32 occupancy
grid, with one particle per pixel, bounded collision work and no allocation.
It advances in 20 ms steps with at most four catch-up steps; gaps over 100 ms
discard catch-up. Re-entering Fluid resets the particles. This is a
motion-responsive particle fluid effect, not a numerical FLIP/PIC solver.

A dedicated 2048-byte-stack worker registers a static data-ready trigger,
coalesces wakeups to at most 50 sensor fetches/s, and has a 40 ms polling
fallback. It uses the existing shared recursive I²C mutex. The interrupt
callback only signals a semaphore; main consumes timestamped snapshots.
Neither the ISR nor BLE callbacks perform sensor bus transactions. Status
reports trigger registration, which does not prove actual interrupt delivery.
Read errors retry; a sensor missing during device initialization can require
a reboot after reconnection.

On Badge, the Motion card provides automatic rotation, manual quarter turns,
response, particle amount and collapsible sensor-mounting controls. Refresh
reads status on demand; there is no additional background polling or tab.
Apply motion persists the complete eight-byte setting. Old firmware retains
its existing controls without exposing Motion.

The loose IMU must be held or temporarily mounted consistently relative to the
panel. Choose which signed sensor axes point toward the panel's right and
down edges. They must be different physical axes: 24 combinations are valid.
The default +X/+Y mapping is unverified on this fixture. To exchange axes in
the editor, first choose the third axis, then set the desired pair. Gravity
is the negative of the accelerometer's support-force vector.

Filtering uses one quarter of each new acceleration delta. Automatic
orientation holds near flat/diagonal poses and under large acceleration:
implementation candidates are 4000 mm/s² projected acceleration, 1.4 axis
dominance, 6000–14000 mm/s² total magnitude and 250 ms candidate dwell.
These are software parameters, not measured or owner-approved acceptance
thresholds. Manual rotation supports 0/90/180/270 degrees.

QR captures orientation on entry and retains it until re-entry. At startup,
one sensor attempt is allowed up to 100 ms before locking the initial QR;
if unavailable, it stays upright until re-entry. Text, artwork, audio,
pairing and recording overlays follow current orientation. Fluid uses physical
panel coordinates. Samples older than 250 ms freeze Fluid and show `IMU`;
other content retains its last orientation. Safe mode uses the upright default
QR and does not start the worker. Recording remains disabled in normal builds.

## Build and deploy

Run from the feature worktree (`build/prototype-audio`). The existing build
directory name is retained to reuse SDK artifacts; its output includes P4.

```powershell
.\scripts\dev.ps1 build -BuildDir build/p3-recording -SysbuildConfFile firmware/configs/ble-sysbuild.conf -ExtraConfFile firmware/configs/audio-recording-bench.conf
.\scripts\app\dev.ps1 build
# When ready to deploy the matching firmware and app:
west flash -d build/p3-recording --runner jlink --no-erase
adb install -r app/build/app/outputs/flutter-apk/app-debug.apk
```

The current app is 0.5.4/build 18. The initial software handoff used 0.5.0/build
14; follow-ups fix Sensor mounting state restoration, remove the separate
NFC verifier requirement and remove the app's Identity block. The matching
recording-enabled firmware and app are deployed.
Use `--no-erase`; it does not substitute for a settings backup. The wired NFC
repair unexpectedly returned the badge to defaults without a loaded bond,
despite this flag; the cause remains unresolved. The owner re-paired and
confirmed Sync again works. Old RF receipts no longer gate synchronization.
The new `badge/motion` setting is additive under schema v1; older settings
without that record receive defaults.

## Owner bench checks

1. Connect Pixel Badge and Refresh settings. Confirm Fluid is available and
   the Motion card reports IMU ready. If not, inspect IMU power, I²C and INT1
   wiring using the existing fixture pin map.
2. Hold or temporarily mount the loose IMU relative to the panel. Set the
   right/down axes and Apply motion. Select Fluid. Tilt in all four directions
   and flip it; particles should settle toward the corresponding physical
   edge. Try low/high response and particle amount.
3. Select Text and test four quarter-turn poses. Check that flat and diagonal
   poses do not cause jitter. Test manual 0/90/180/270, then restore Automatic.
4. Enter QR upright, rotate while staying in QR and scan it. It should retain
   its entry orientation. Leave and re-enter QR sideways, then scan again.
   Check that QR and NFC still open the same selected link.
5. While Fluid is active and no NFC operation is running, disconnect only the
   IMU. After sample expiry plus display-loop latency, `IMU` should appear;
   buttons, other modes and BLE should remain usable. Reconnect and check
   recovery; reboot if the sensor was absent at device initialization. Avoid
   disturbing the shared bus during tag programming.
6. Apply preferred motion settings and power-cycle with both buttons released.
   Confirm readback and behavior persist. Recheck artwork/animation, pairing,
   audio visualization and background BLE. Recording requires the separate
   P3 bench image and its existing visibility checks.

Record actual axis mapping, interrupt delivery, frame time, stack peaks and
shared-bus behavior under combined load. These remain unmeasured, as do the
nRF52840 compute/refresh limits. A build does not close hardware acceptance.
Software evidence: [P4 result](../validation/results/2026-10-15-dk-p4-motion/README.md).
