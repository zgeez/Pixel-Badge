# Validation

Status: **no results yet**. This document defines what will be measured and how.
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
settings (CE driven low by a test build that does not configure it), and
again after an I²C watchdog expiry with firmware's settings, drive the TS pin with a resistance decade or a heated/cooled NTC and check
that charging stops below about 0 °C and above 45 °C board temperature; then
repeat with firmware's thresholds. Check that charging stays off from
plug-in until firmware drives CE low, that an MCU reset stops charging, and
measure the input current on an SDP host before firmware has configured the
charger (≤ 100 mA).
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

Acceptance criteria: TBD.

### 4. Current and power

Purpose: measure current per operating mode and brightness level, including
peaks during BLE activity.

Planned method: power profiler or source-measure unit; documented firmware
configuration per measurement.

Acceptance criteria: TBD.

### 5. BLE

Purpose: verify pairing, configuration through the companion app, and range,
worn and unworn.

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

Purpose: verify response to sound and that the microphone is inactive outside
the audio mode.

Required privacy review: trace raw samples from PDM/DMA capture through DSP and
buffer disposal. Raw microphone samples may exist only in volatile/transient
processing buffers. They must never be written to flash, NVS/settings, external
storage, logs, crash dumps, or any other persistent storage. Review error paths,
debug builds, logging backends, telemetry, and crash-dump configuration as well
as the normal audio path. Persistent dumps must exclude sample buffers or be
disabled; do not preserve whole RAM containing samples.

Where practical, feed a known sample pattern and instrument persistence/logging
paths during audio mode, mode changes, reset, and faults. Inspect storage writes
and dump/log outputs for sample leakage; record test limitations alongside the
code/data-flow review. Synthetic test fixtures may be committed, but captured
microphone audio must not become a test artifact. Privacy passes only when no
raw audio reaches a persistent sink; retain the README's no-audio-storage
guarantee. Product audio code does not exist yet, so this is a future release
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

- Review the selected pack/protector and regulator limits, divider/switch
  calculations, ADC configuration/error budget and **PROPOSED** threshold,
  hysteresis, sampling and reaction-time policy before schematic freeze.
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
