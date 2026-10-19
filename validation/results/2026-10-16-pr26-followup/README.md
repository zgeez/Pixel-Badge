# PR26 NFC diagnostic and shared-bus follow-up - 2026-10-16

Source: fw/prototype-motion after 9c331da, existing nRF5340 DK implementation.
Changes below are local pending owner signing. No nRF52840 acceptance claim.

## Per-badge NFC diagnostics

The controller retained nfcResult/nfcBusy across disconnect. A Studio badge's
-13 or -19 could therefore appear as a repair/wiring fault on a later v1 badge,
which has no detailed NFC-status characteristic. Disconnect now clears result,
busy and synchronized state before awaiting transport cleanup. Attach already
uses disconnect, so an implicit switch also clears them.

Four regressions cover both errors and explicit disconnect versus direct
reattach to a v1 badge. All four failed before the correction with the previous
negative result still present, then passed. The v1 badge remains unverified and
uses generic synchronization feedback without inheriting the prior diagnostic.

## Shared I2C trigger path

The reported unlocked DRDY register read is not present at reviewed commit
9c331da. Existing icm42688_i2c_work calls reg_read, which acquires
badge_bus_lock around bank selection and the I2C transfer. NFC's bench_bus_lock
forwards to that same recursive Zephyr mutex. The driver lock and regression
were already included by merged PR21 (11cfc23); no driver change is required.

The existing test_data_ready_trigger holds badge_bus_lock, raises emulated
INT1, yields for 10 ms and verifies no callback and uncleared INT_STATUS. After
unlock, it requires the callback and cleared status. The native driver suite
was rerun: all 8 cases passed in one native_sim configuration (42.06 seconds).
The motion guide now names the deferred read and shared lock explicitly.
This establishes software mutual exclusion, not real-bus latency or NFC timing.

## Verification

- Flutter: all 84 tests pass, including the four new switch regressions.
- Analyzer: no issues; Dart formatting and repository/whitespace checks pass.
- Native IMU driver: 8/8, including deferred-trigger mutual exclusion.
- No firmware source changed; no new target build, APK, flash or phone bench.
- No commit, push, external comment or review-thread resolution in this task.

Local logs: app/build/pr26-nfc-diagnostic-before-actual.log,
app/build/pr26-nfc-diagnostic-tests.log,
app/build/pr26-nfc-diagnostic-analyze.log, build/pr26-imu-lock.log,
build/pr26-imu-lock/twister.json, build/pr26-followup-check.log.

Physical IMU/NFC combined-load timing, P3/P4 acceptance, prior state-loss cause
and the later nRF52840 port remain pending.
