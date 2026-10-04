# Validation

Status: **no results yet**. This document defines what will be measured and how.
Results are added only after measurement, with the setup and raw data needed to
reproduce them.

## Recording results

Each result records:

- date, hardware revision, firmware commit
- equipment and settings
- method (link to the procedure below)
- raw data location and summary
- pass/fail against the stated criterion

## Categories

### QR scanning

Purpose: confirm a QR code on the 32×32 matrix scans reliably before matrix
geometry is frozen.

Planned variables: QR version and error-correction level, quiet zone, pitch,
brightness, phone models and scanner apps, ambient lighting.

Acceptance criteria: TBD.

### Environmental, angle, and distance

Purpose: characterize scan reliability and readability across realistic use.

Planned variables: viewing angle, distance, indoor/outdoor lighting, motion.

Acceptance criteria: TBD.

### Matrix timing

Purpose: verify refresh rate, per-row timing, brightness control, and absence of
visible flicker or ghosting.

Planned method: logic analyzer/oscilloscope on drive signals; camera checks for
flicker.

Acceptance criteria: TBD.

### Power

Purpose: measure current per operating mode (display patterns, brightness
levels, BLE advertising/connected, sleep).

Planned method: power profiler or source-measure unit; documented firmware
configuration per measurement.

Acceptance criteria: TBD.

### Battery runtime

Purpose: measure runtime from full charge in defined usage profiles.

Planned method: full discharge runs with logged voltage; profiles defined before
testing.

Acceptance criteria: TBD.

### BLE / NFC

Purpose: verify connection, service behavior, and range for BLE; tap
reliability and read range for NFC across phone models.

Acceptance criteria: TBD.
