# Validation

Status: **two DK bench results**
([QR screening](#result-qr-screening-on-the-hub75-bench-panel),
[matrix timing](#result-scan-timing-on-the-nrf5340-dk-bench-no-matrix)); no
Rev A results. This document defines what will be measured and how.
Results are added only after measurement, with the setup and raw data needed to
reproduce them. Numerical acceptance thresholds are listed here only once they
have been accepted; until then they are TBD. Required functional and privacy
checks below come from the Rev A requirements; they are not completed tests.

## Recording results

Each result records:

- date, hardware revision, firmware commit
- equipment and configuration/settings
- method (link to the procedure below)
- raw-data location and summary
- acceptance threshold and pass/fail

Raw data goes in `validation/results/<date>-<name>/` with a README listing each
file and its SHA-256. Files too large for the repository (more than about 5 MB
per result) go in a GitHub release instead, with their SHA-256 in the record.

## Categories

### 1. QR scan reliability

Purpose: confirm the QR code on the 32×32 matrix scans quickly and reliably
before matrix geometry is frozen, and again on the finished badge.

Planned variables: QR version and error-correction level, quiet zone, polarity,
pitch, brightness, refresh rate, phone models and scanner apps, distance, angle,
ambient lighting.

Use a full four-module quiet zone for the standard QR path. Version 1 has
21×21 modules, hence 29×29 including the quiet zone; Version 2 needs 33×33 and
does not fit the 32×32 display at one LED per module. Version 1 is the Rev A
display baseline. Additional encoder versions do not establish valid output on
this matrix. Record the exact payload and reject configurations that cannot fit
with the required quiet zone; any different geometry or nonstandard experiment
requires a separately documented decision and validation. Geometry alone does
not establish optical scan reliability. See the DENSO references in
[references.md](references.md).

Acceptance criteria: TBD.

#### Result: QR screening on the HUB75 bench panel

Screening only (one phone, three tries, distance varied), not the acceptance
test.

| Field | Value |
|---|---|
| Date | 2026-10-11 |
| Hardware | nRF5340 DK v2.0.3; 64 × 64 P2 HUB75 panel (2.0 mm pitch, the badge's pitch) through two SN74AHCT245N at 5.00 V; 32 × 32 window, panel turned so the image reads upright |
| Firmware | QR screening build (`firmware/configs/qr-screening.conf`) of the working tree before commit `b340b0db37b6`, which holds the QR mode (squash-merged to `main` as `414fbf5`; for the QR mode, the merge differs from it only in `hub75.c`, the later brightness fix). Differences in that commit: HUB75 blanking fixes (blanking is not used by the QR mode) and a run-time settings count. The owner-link codes came from a local code file (`CONFIG_APP_QR_CODES_FILE`), not in the repository |
| Equipment | Nothing 4a Pro phone (owner's wording) with Google Lens; bench supply |
| Settings | Run 1: Version 1 codes, levels L, M, Q, representative payloads (19 and 16 characters), full quiet zone (5 and 6 modules). Run 2: the owner's links, one Version 1 code at level L (uppercase, 24 characters, full quiet zone) and four Version 3 codes at levels H, M, M, L (24 to 44 characters) with a 1 to 2 module quiet zone. Each code in normal and inverted polarity at brightness 255, 64 and 16; 195 Hz refresh |
| Method | Procedure in the result folders' README. Deviations: 3 tries per setting instead of 10, Google Lens instead of the native camera, distance adjusted per setting, decode times not recorded |
| Raw data | [`validation/results/2026-10-11-hub75-qr-screening/`](../validation/results/2026-10-11-hub75-qr-screening/) and [`validation/results/2026-10-11-hub75-qr-v3-reduced-quiet-zone/`](../validation/results/2026-10-11-hub75-qr-v3-reduced-quiet-zone/): per-setting counts (no photos or video) |
| Threshold | None accepted. The proposed acceptance rule (9 of 10 within 1 s, 10 of 10 within 3 s) does not apply to a screening |
| Pass/fail | Screening pass: every setting decoded in all 3 tries (48 settings) |

Observation: the working distance depended on brightness and room light; in a
lit room the phone had to be further away for a dimmer code. Version 3 codes
with the reduced quiet zone also scanned, but they remain a nonstandard
experiment.

### 2. Electrical correctness

Purpose: verify rails, power sequencing, behavior with the power switch off,
isolation between the logic and LED supply domains, and charger status signals.

Trace USB-C VBUS to both the charger input and module VBUS pin 44. Check the
module USB supply conditioning/decoupling against [power.md](power.md), then
measure hot-plug/removal and USB attachment with the switch on/off and battery
present/absent. Verify no unsupported back-powering; use a diagnostic USB image
to check enumeration/data with the normal MCU supply enabled.

Verify CC1 and CC2 each have independent Rd to ground, within the complete
circuit's allowed resistance tolerance (power.md). With a compliant C-to-C
source, test both plug orientations, powered/switch-off and battery-absent
states: source enables VBUS and powers the charger input; verify actual charging
with a pack present. Repeat USB enumeration/data in both orientations with
logic powered; check the reviewed
source-current policy, including unconfigured/suspended and switch-off states.

Measure both CC-sense SAADC pins against VDD with a 1.5 A and a 3 A source
attached in both orientations: switch OFF, battery absent, and across power-up
and power-down (oscilloscope). The pin voltage must stay within the nRF5340
absolute maximum (VDD + 0.3 V, i.e. ≤ 0.3 V while VDD is 0 V) at all times, and
the measured effective Rd must stay within tolerance. Repeat for any other
analog input fitted from a source that is live while the MCU is off.

Record the protected pack's identity and protection specifications, or review
the dedicated hardware protection circuit for an unprotected cell. Verify
overdischarge, overcurrent, short-circuit protection and documented recovery,
using controlled fixtures/battery simulation where practical. Check operating
peaks against pack/protector ratings. Do not rely on the BQ25620 or firmware
alone, and do not perform uncontrolled short-circuit tests on a LiPo.

Measure the battery charge current on a USB-C source that advertises 1.5 A or
more and on a BC1.2 dedicated charger, with the switch OFF (charging-only
state) and ON, at several battery voltages: it must stay ≤ 1.2 A (the pack
limit), with the input limit set as the policy requires. Record charger and
board-NTC temperatures and any thermal regulation during a full charge; record
the empty-to-full time only as a measurement. With the charger at its reset
settings (CHG_EN driven high by a test build that does not configure it), and
again after an I²C watchdog expiry with firmware's settings, drive the TS pin with a resistance decade or a heated/cooled NTC and check
that charging stops below about 0 °C and above 45 °C board temperature; then
repeat with firmware's thresholds.
Board-NTC qualification (release gate before charging is approved): the NTC
is on the front and measures board temperature, not cell temperature
([ADR 0010](decisions/0010-single-sided-assembly.md)), so a warm board can sit
next to a cold cell. With a thermocouple taped to the pack:

- Cold cell, warm board: soak the pack at or below 0 °C and at 2–5 °C,
  connect it to a board at room temperature, plug in USB, and log the cell
  and board-NTC temperatures until the charger would start. Charging must not
  start while the cell is below 0 °C.
- Transitions: move the badge from a cold environment indoors and back, and
  charge from cold, logging the worst-case offset and lag between cell and
  board-NTC readings (including charger self-heating).
- Hot side: cell near 45 °C with a cooler board; charging must stop.

If the board reading can allow charging while the cell is below 0 °C or above
45 °C, charging is not approved until one fallback is in place and re-tested:
firmware COLD/HOT limits moved inward by the measured offset plus margin; a
charge inhibit after plug-in until the readings have settled for a measured
time; or a sensing change (an NTC bonded to the pack, or a pack with a
thermistor).
Check that charging stays off from
plug-in until firmware drives CHG_EN high, that an MCU reset stops charging,
and that charging stays off with USB present and the logic rail held off
(R209 link open or EN_3V3 held low); measure the input current on an SDP host
before firmware has configured the charger (≤ 100 mA).
VBUS attach (USB 2.0 inrush requirement, power.md): on the same source,
capture attach current and VBUS droop for the badge and for a 10 µF ∥ 44 Ω
reference load; the badge must not draw more attach charge. If it does,
fit the documented C402/R401 values and repeat.
Measure the average battery current with all pixels lit at the configured
peak current and maximum brightness at the lowest operating VBAT and at the
highest VLED setting; it must stay ≤ 1.5 A (the cell's continuous rating).
Measure VLED across the full battery range to confirm the pass-through
envelope.
Inductor current (release gate for L202 and L301): with a current probe,
measure the TPS61023 switching frequency at the lowest VIN, and the peak and
ripple inductor current at maximum LED load, at start-up and with the LED
rail overloaded into current limit; measure the BQ25620 inductor current at
the 1.5 A input limit. The calculated figures in [power.md](power.md) are
estimates, not bounds; record any operating point where the current exceeds
the XGL4020's 10% (3.8 A) or 30% (8.8 A) inductance-drop rating.
Verify the power states: no USB and switch OFF leaves the logic rail off;
USB with switch OFF starts the charging-only state with the LED rail off;
removing USB with the switch OFF turns the logic rail off. Record the pack
protection board's actual trip behavior only with a current-limited fixture.

Acceptance criteria: TBD.

### 3. Matrix timing, flicker, and ghosting

Purpose: verify refresh rate, row timing, blanking, absence of visible flicker
or ghosting, and that the matrix switches off if the scan stalls.

Planned method: logic analyzer/oscilloscope on drive signals; camera checks.
Measure the scan-stall timeout: stop MTX_LAT and time SAFE rising after the last
latch, at room temperature and at the hot and cold ends of use. The calculated
bound is 0.76–1.41 ms: TI guarantees 1.0–1.1 ms at 10 kΩ / 0.1 µF (switching
characteristics, VCC 3.3 V ± 0.3 V, −40 to 125 °C), scaled as tw ∝ R × C for the
1% resistor and the X7R capacitor's ±10% tolerance and ±15% over temperature;
capacitor DC bias only shortens it. The longest value
is the single-row fault exposure to compare with the LED's pulse rating.

LED headroom (qualification gate for the VLED setting and the KT-0603W):
KENTO specifies forward voltage only at 5 mA; its curves at higher current are
typical, so the headroom at the operating point is measured, not calculated.
With a full row lit, measure the actual column current and the voltage at every
TLC59283 output, at the lowest VLED the hardware can produce (lowest setting,
regulator tolerance, lowest battery) and at the cold end of use. Every output
must stay at or above the 0.8 V design headroom with its current in
regulation. If not, raise VLED within the TPS61023 and 74HC595 limits or lower
the peak current, and repeat.

Acceptance criteria: TBD.

#### Result: scan timing on the nRF5340 DK (bench, no matrix)

| Field | Value |
|---|---|
| Date | 2026-10-09 |
| Hardware | nRF5340 DK v2.0.3 (nRF5340 QKAA). No matrix or driver ICs attached; Rev A pins mapped onto the DK by the bench overlay. |
| Firmware | `firmware/bench/matrix_timing` at commit `d541e00880b3` (squash-merged to `main` as `f4c577f`, with identical firmware and scripts); nRF Connect SDK v3.4.1, board `nrf5340dk/nrf5340/cpuapp`. `zephyr.bin` SHA-256 `4c89e24ee02b8efc24603a943fdee9fcaf797bb62044e5425d7ec8292dc91b53`: the DK's flash read back after the captures and a pristine build of this commit both match it. |
| Equipment | FX2-based 8-channel logic analyzer (sigrok `fx2lafw` driver) with PulseView (libsigrok 0.6.0-git-883c2ac, from the session file); the DK's onboard J-Link (SEGGER J-Link software V9.82) for flashing and halting |
| Settings | 24 MS/s, 200 M samples (8.33 s). D0–D4 = SCK P0.08, MOSI P0.09, LAT P0.11, BLANK P0.12, frame marker P0.26. Edge resolution ±42 ns; the SCK duty cycle is not measurable at this rate. |
| Method | [Matrix scan timing](firmware.md#matrix-scan-timing). Halt: J-Link command file `connect`, `h`, `sleep 2000`, `g` during the capture. Values measured from the capture by `analyze.py`. |
| Raw data | [`validation/results/2026-10-09-dk-matrix-timing/`](../validation/results/2026-10-09-dk-matrix-timing/): sigrok session, screenshots, analysis script and output |
| Threshold | Refresh 150–200 Hz (project target). Qualitative: LAT only while BLANK is high, no row lit across the frame restart, BLANK high while the CPU is halted. The blank window length has no accepted threshold (the design estimate is at least 2–4 µs). |
| Pass/fail | **Pass.** Behavior under CPU and Bluetooth load not tested. |

Summary (6.3 s of scanning: 1234 frames, 39 485 rows):

- Row slot 160.0 µs; rows on for 155.0 µs every slot. BLANK high for 5.0 µs
  around each latch, from 2 µs before LAT rises to 3 µs after.
- LAT pulse 1.0 µs, always inside BLANK high. SPIM4 starts 0.33–0.62 µs after
  LAT falls; 64 clocks span 7.9 µs and end 151 µs or more before the next latch.
- All 39 485 shifts decode to the test pattern (row-select bit 0, then `A5`,
  row, `5A`, inverted row), rows in order, and each frame starts with row 1.
- Frame restart: the frame interrupt starts 2.0 µs after the last BLANK rise
  and runs for 2.0–2.2 µs; BLANK stays high for 7.8 µs. Frame period 5123 µs
  (195.2 Hz). In 5 of 1233 frames the interrupt started 12–15 µs after the
  last BLANK rise instead of 2 µs (cause not identified); BLANK stayed high for
  up to 21 µs and the frame took up to 5136 µs.
- CPU halted: the scan completed its frame (32 of 32 latches) and stopped with
  BLANK high, LAT and SCK low, for 2.01 s, then resumed when the CPU ran.

### 4. Current and power

Purpose: measure current per operating mode and brightness level, including
peaks during BLE activity.

Planned method: power profiler or source-measure unit; documented firmware
configuration per measurement.

Acceptance criteria: TBD.

### 5. BLE

Purpose: verify pairing, configuration through the companion app, and range,
worn and unworn. Pairing must be authenticated LE Secure Connections (MITM
protected); the configuration, audio and recording-control characteristics
must refuse a link whose bond is unauthenticated, such as Just Works.

Acceptance criteria: TBD.

### 6. NFC

Purpose: verify tap reliability and read range across phone models with the
battery, magnets, and rear carrier installed, with the badge on and off.

Required content-update test (in addition to read-range tests):

1. Set a new, distinctive contact URL through the intended companion-app/MCU
   configuration path. Record the exact configured URL and firmware revision;
   a factory-preprogrammed tag does not satisfy this test.
2. Verify the MCU writes the new NDEF URI to the NTAG **EEPROM over I²C** and
   checks write completion/readback. Volatile SRAM/pass-through alone cannot
   establish off-state persistence (NXP NT3H2111/2211 Rev.3.6, §§2.3 and 8.3).
3. Read the NDEF with an NFC phone while powered. Compare the decoded URI
   exactly with the configured URL, including URI-prefix expansion; opening a
   redirect destination alone is insufficient evidence of the stored payload.
4. Switch the badge off, remove the reader field and tap again. Confirm the
   exact same updated URL is readable passively with the full mechanical stack.
   Repeat without battery/USB power to cover the empty-battery requirement.
5. Decode the displayed QR and confirm it matches both the QR-configured URL
   and the updated NFC URI. Repeat with a second distinct URL to detect stale
   content; record powered/off-state payloads and results per phone.
6. Interrupt a configuration/write and exercise I²C errors or RF/I²C contention.
   Verify error reporting, retry/reconciliation after restart, and that update
   success is not reported while NFC and QR differ. Any mismatch blocks this
   functional check; document the recovery behavior before implementation.
   Include NXP errata Rev.4.1's RF-transition, verification and recovery cases;
   record any fixture limitations instead of claiming exhaustive timing coverage.

Required RF-write-protection test:

1. Provision according to [nfc.md](nfc.md) for the selected part. Record the
   non-secret memory/access configuration and tool/firmware revision. Never
   include passwords or credential-bearing dumps in evidence committed here.
   Before the credential is discarded, prove it was programmed: in a fresh RF
   session, authenticate with the generated PWD and check the returned PACK;
   in another fresh session, confirm the factory-default PWD is rejected.
   Unauthenticated-rejection tests alone cannot show this, because PWD/PACK
   always read back as zero.
2. With the badge switched **off** (and again with no battery), start a fresh RF
   session and, without authentication, attempt to write the capability
   container (including its read-access and size bytes), the static and dynamic
   lock bits, the NDEF/URI pages and the configuration pages. Without powering
   the badge, read it passively with phones: the exact URL must still be
   readable, proving the CC lock holds before any MCU repair. Then power the
   badge and confirm the MCU detects and clears any lock bits that were set.
   Start each further test in a fresh RF session (remove/reapply the field so a
   previous successful authentication cannot carry over). With no password
   authentication, attempt to replace the URL using an NFC writer; include
   writes to NDEF metadata and configuration that could bypass protection.
   Record rejection and re-read the exact NDEF/URI and relevant non-secret
   controls to prove they are unchanged.
3. Repeat while powered, switched off, and without battery/USB, and after power
   cycling. Public reads must still work without authentication. Wrong-password
   and retry-limit tests follow the reviewed policy on a sacrificial tag if
   irreversible locking is possible; do not lock the event tag by accident.
4. Change to another URL through the intended MCU path. Verify I²C updates still
   work, exact powered/passive NFC reads match the decoded QR, and the new URL
   remains protected against unauthenticated RF writes. Recheck after reset and
   interrupted provisioning/update recovery.

Required outcome: the exact updated configuration URL is shared by QR and NFC
while powered and remains available passively after power removal. Read-range
thresholds and the phone test set still need acceptance. Unauthenticated RF
writes cannot alter the URL or its interpretation; protection must not prevent
public passive reads or future MCU/I²C updates.

Acceptance criteria: TBD.

### 7. IMU

Purpose: verify orientation and motion response driving the fluid simulation
and the display rotation.

The ICM-42688-P I²C driver has passed only build and emulator tests. On real
hardware (a breakout before Rev A where possible, then Rev A), verify: I²C
waveforms and address (AP_CS high, AP_AD0 strap), reset and WHO_AM_I,
configuration read-back including the I²C-mode settings, accelerometer and
gyroscope readings against known orientations and rotation, the INT1
data-ready interrupt at the configured rate, and wake-on-motion once
implemented. Record any I²C failure that triggers the SPI fallback.

Display rotation: in all four 90° orientations the name and animation views
turn upright without visible flicker or repeated flipping on small movements
or when the badge lies flat, and QR mode keeps the orientation it entered
with until it is left.

Acceptance criteria: TBD.

### 8. Microphone and audio processing

Purpose: verify response to sound, that the microphone is inactive outside the
audio modes, and that recording is always visible: the recording indicator is
on the matrix for the whole recording, and audio leaves the badge only in a
recording the user started, over the encrypted link to the paired app.

Required privacy review: trace audio data, raw samples and the encoded ADPCM
stream alike, from PDM/DMA capture through DSP, encoding, the Bluetooth
transmit queues (including any retransmission buffers) and buffer disposal.
Audio data may exist only in volatile/transient buffers and must never be
written to flash, NVS/settings, external storage, logs, telemetry, crash
dumps, or any other persistent storage; during a recording the encoded stream
is only sent to the paired app. Trace key material (the app key, per-recording
keys and key-exchange secrets) the same way, on the badge and in the app,
through normal and error paths: no key in logs, telemetry or dumps. Check
that:

- no audio is captured for a recording before the matrix driver confirms the
  REC frame is shown: compare the capture timestamps of the first sent
  samples with the frame-swap time, including a start from a visualization
  mode;
- no audio is captured or sent after REC leaves the matrix: for each stop
  (button, app, length cap, display off, blanking, LED rail shed, driver
  fault), compare the timestamp of the last captured and last sent sample
  with the frame-off or rail-off time, and check that queued audio is
  discarded;
- a recording cannot start, from the app or a long-press, without a
  connected, subscribed companion app that has proved the app key on this
  connection; over a link whose bond is unauthenticated (Just Works); while
  the LED rail is shed for low battery or the display is off; or with the
  matrix driver faulted. A long-press then shows that recording is
  unavailable when the matrix is on;
- a long-press captures nothing until the app returns a fresh authenticated
  accept; with the app suspended or stalled while still connected and
  subscribed, no capture starts and the badge shows that recording is
  unavailable;
- app-key enrollment needs the code confirmed on the badge, and a second app
  trying to enroll in the same pairing window cancels it; after pairing
  again and after a factory reset, the old app key and counters are erased
  and commands made with them are refused;
- recording-control commands with a wrong key, a replayed or reused counter,
  or sent by a second app on the same phone after the companion app has
  authenticated are refused;
- a second app subscribed on the same phone during a recording receives only
  data it cannot decrypt, and altered audio packets are rejected by the app;
- each recording uses a distinct key (review the derivation and compare the
  keys of two recordings), and packets from one recording injected into
  another are rejected;
- the app stores recordings only in its private storage: another app cannot
  read the file, it is not in shared media or file storage, and it is absent
  from a device backup and a cloud backup of the phone; it leaves the app
  only through an explicit export or share;
- the mode cannot be left or hidden while recording;
- the user can stop a recording with a button press and from the app;
- a recording also stops when the link drops or the app unsubscribes, the
  display is turned off or blanked, the LED rail is shed or the matrix driver
  faults, or the length cap is reached; the firmware ceiling holds with the
  length setting missing, corrupt or out of range in storage, and an
  over-limit settings command is clamped or refused;
- a recording decodes correctly end to end: play a known synthetic waveform
  (for example a tone sweep with timing marks) to the microphone during a
  recording, with REC on the matrix and representative Bluetooth conditions,
  then decode the app's stored file and check its duration, sample rate,
  sample order and continuity, and that induced packet loss is detected and
  handled as specified rather than shifting later audio.

REC visibility: with the enforced recording brightness and the badge worn on
the chest, an observer who is not told the mode reads whether the badge is
recording at 1 m and 2 m, head-on and from 45° to the side and 30° above,
indoors (room light), outdoors in shade, and outdoors in direct sun, with the
measured illuminance recorded for each. Proposed pass rule (not yet accepted):
10 of 10 trials per condition, each answered within 2 s. Recording is enabled
in release firmware only if every condition passes; if one fails even at full
brightness, release firmware ships with recording disabled until a fix passes
this test.

Review error paths, debug builds, logging backends, telemetry, and crash-dump
configuration as well as the normal audio path. Persistent dumps must exclude
audio buffers or be disabled; do not preserve whole RAM containing audio data.

Where practical, feed a known sample pattern and instrument persistence/logging
paths during audio mode, mode changes, reset, and faults. Inspect storage writes
and dump/log outputs for sample leakage; record test limitations alongside the
code/data-flow review. Synthetic test fixtures may be committed, but captured
microphone audio must not become a test artifact. Privacy passes only when
neither raw nor encoded audio reaches a persistent sink on the badge and audio
leaves it only in a visible, user-started recording streamed to the paired
app. Product audio code does not exist yet, so this is a future release
requirement, not a claim that the audit has already passed.

Acceptance criteria: TBD.

### 9. Thermal

Purpose: measure the hottest components and the battery at worst-case display
patterns and while charging, worn.

Acceptance criteria: TBD.

### 10. Battery runtime

Purpose: measure runtime from full charge in defined usage profiles.

Design target: **about eight hours under representative career-fair use**, not
a measured result or an accepted numerical pass threshold.

Planned method: agree on and record brightness, mode/time mix (including audio),
BLE activity, selected pack, ambient conditions, and usable end-of-run condition
before testing. Run from full charge to the normal safe stop/load-shed endpoint
with logged voltage and actual hours of usable operation; do not force discharge
past cell limits. Record deviation from the eight-hour target and any reduced
brightness/features needed to reach it. Accept the numerical runtime threshold
and profile before calling a run pass/fail.

Acceptance criteria: TBD.

### 11. Mechanical wearability

Purpose: verify attachment retention, comfort, thickness, and that the battery
is not mechanically stressed, for both mountings and both orientations
([ADR 0009](decisions/0009-dual-orientation-dual-mounting.md)):

- Magnet mount, landscape and portrait, on a thin shirt, a blazer and a
  jacket: walking, sitting, bending; record the garment and any detachment.
- Lanyard, landscape and portrait hanging: the badge hangs level, the
  attachment point does not cover USB-C, the power switch, the NFC coil or
  the antenna, and a pull on the lanyard does not load the PCB.
- Assembled thickness against the regional targets: ≈ 12 mm over the LED
  field, ≈ 13.5 mm over the electronics region, ≈ 14.5 mm at USB-C and
  ≈ 16.5 mm at the SWD header (local edge exceptions).

Acceptance criteria: TBD.

### 12. Power cycling, brownout, and recovery

Purpose: verify repeated power cycling, low-battery behavior (display load shed
before logic is affected), watchdog recovery, and settings/firmware recovery.

Required battery-measurement and load-shed procedure:

- The pack/protector and regulator limits, divider/switch calculations, ADC
  configuration/error budget and **PROPOSED** threshold policy were reviewed at
  the schematic freeze ([power.md](power.md#battery-state-measurement)); this
  procedure measures them.
- With a current-limited battery simulator, sweep the intended VBAT range and
  compare ADC readings with a calibrated meter at VBAT/sense test points. Test
  loaded and relaxed states, USB present/absent, startup and switch-off leakage/
  back-powering. Include probe loading and temperature limitations in the record.
- Ramp through the proposed shed threshold with worst-case display + BLE load;
  capture VBAT, logic rail, LED enable/current and sample-to-shed timing. Verify
  the LED rail is disabled with margin before regulator instability or protector
  cutoff; neither compile success nor a battery-voltage log proves this margin.
- With USB attached as a Default source at the 100 mA input limit and a depleted pack, verify
  the display stays off until battery-qualified recovery and that the charger's
  supplement current does not drain the pack through repeated re-enables. With a
  1.5 A or 3 A source, verify input-qualified controlled re-enable, the
  brightness cap below the input limit, and back-off after a failed attempt.
- Inject missing/stale/invalid samples and verify conservative matrix-off
  behavior. Test voltage rebound, threshold noise, restart near empty and
  charger attachment/removal; controlled recovery must avoid repeated resets
  or re-enable loops. Retain independent protection for faults firmware cannot
  react to.
- Characterize the selected pack's discharge under the agreed use profile.
  Accept final accuracy and shed/recovery values only from the error budget and
  measured sag/cutoff margins; voltage-based sensing must not be reported as
  calibrated state of charge or remaining runtime. Repeat on the event build.

Acceptance criteria: TBD.
