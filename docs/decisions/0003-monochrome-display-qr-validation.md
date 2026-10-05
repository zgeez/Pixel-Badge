# 0003: 32×32 monochrome display and QR-first validation

- Status: Accepted
- Date: 2026-10-04
- Deciders: Project owner

## Context

The contact QR must be reliably readable on an emissive LED matrix before
committing to matrix geometry. The available HUB75 prototype is RGB and does
not establish the custom white-LED matrix's optical or electrical performance.

## Options considered

1. Freeze the final geometry and current before testing: speeds layout but
   leaves the core contact-sharing function unverified.
2. Select 32×32 monochrome output and gate geometry/current choices on QR
   prototype evidence: preserves the product direction and exposes optical risk
   before fabrication.

## Decision

Use 32×32 individual monochrome white LEDs with binary pixels and discrete
global brightness levels. Validate QR optics on the DK/HUB75 prototype before
freezing final geometry, then repeat on Rev A. Nominal 2.0 mm pitch, LED MPN,
peak current, current-setting resistor, brightness cap, and the preferred 1:32
scan/driver parts remain provisional or prototype-dependent.

Use Version 1 as the standard display baseline: 21 modules plus four quiet-zone
modules on each side occupy 29×29. Version 2 requires 33×33 and cannot fit this
matrix at one LED per module. Additional encoder versions are useful only where
output geometry permits; reject unsupported display payloads rather than
silently clipping the quiet zone. Prefer payloads that fit, such as a short URL.

## Consequences

QR acceptance gates geometry; prototype screening does not prove final-board
scan reliability. Payload/error-correction choices must fit the display and be
tested with real phones. Drive current, timing, camera behavior, and thermal
limits need separate verification. Schematic topology can progress with
adjustable values while optical evidence is gathered.

## References

- [QR validation methodology](../validation.md#1-qr-scan-reliability)
- [Prototype and final hardware](../hardware.md)
- [DENSO WAVE QR dimensions and quiet zone](../references.md#standards)
