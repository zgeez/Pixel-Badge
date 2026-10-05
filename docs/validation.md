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

Record the protected pack's identity and protection specifications, or review
the dedicated hardware protection circuit for an unprotected cell. Verify
overdischarge, overcurrent, short-circuit protection and documented recovery,
using controlled fixtures/battery simulation where practical. Check operating
peaks against pack/protector ratings. Do not rely on the BQ24074 or firmware
alone, and do not perform uncontrolled short-circuit tests on a LiPo.

Acceptance criteria: TBD.

### 3. Matrix timing, flicker, and ghosting

Purpose: verify refresh rate, row timing, blanking, absence of visible flicker
or ghosting, and that the matrix switches off if the scan stalls.

Planned method: logic analyzer/oscilloscope on drive signals; camera checks.

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

Required outcome: the exact updated configuration URL is shared by QR and NFC
while powered and remains available passively after power removal. Read-range
thresholds and the phone test set still need acceptance.

Acceptance criteria: TBD.

### 7. IMU

Purpose: verify orientation and motion response driving the fluid simulation.

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

Purpose: verify attachment retention on clothing, comfort, thickness, and that
the battery is not mechanically stressed.

Acceptance criteria: TBD.

### 12. Power cycling, brownout, and recovery

Purpose: verify repeated power cycling, low-battery behavior (display load shed
before logic is affected), watchdog recovery, and settings/firmware recovery.

Acceptance criteria: TBD.
