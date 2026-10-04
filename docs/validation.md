# Validation

Status: **no results yet**. This document defines what will be measured and how.
Results are added only after measurement, with the setup and raw data needed to
reproduce them. Acceptance criteria are listed here only once they have been
accepted; until then they are TBD.

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

Acceptance criteria: TBD.

### 2. Electrical correctness

Purpose: verify rails, power sequencing, behavior with the power switch off,
isolation between the logic and LED supply domains, and charger status signals.

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

Acceptance criteria: TBD.

### 7. IMU

Purpose: verify orientation and motion response driving the fluid simulation.

Acceptance criteria: TBD.

### 8. Microphone and audio processing

Purpose: verify response to sound and that the microphone is inactive outside
the audio mode.

Acceptance criteria: TBD.

### 9. Thermal

Purpose: measure the hottest components and the battery at worst-case display
patterns and while charging, worn.

Acceptance criteria: TBD.

### 10. Battery runtime

Purpose: measure runtime from full charge in defined usage profiles.

Planned method: full discharge runs with logged voltage; profiles defined before
testing.

Acceptance criteria: TBD.

### 11. Mechanical wearability

Purpose: verify attachment retention on clothing, comfort, thickness, and that
the battery is not mechanically stressed.

Acceptance criteria: TBD.

### 12. Power cycling, brownout, and recovery

Purpose: verify repeated power cycling, low-battery behavior (display load shed
before logic is affected), watchdog recovery, and settings/firmware recovery.

Acceptance criteria: TBD.
