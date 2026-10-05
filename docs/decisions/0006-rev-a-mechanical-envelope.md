# 0006: Rev A PCB and mechanical envelope

- Status: Accepted
- Date: 2026-10-04
- Deciders: Project owner

## Context

The wearable assembly must accommodate the display, module antenna, passive
NFC coil, battery, controls, and debug/rework access. Extreme compactness would
compete with RF placement, battery safety, and development access.

## Options considered

1. Optimize immediately for minimum size/thickness: compact but increases
   layout, antenna, battery-fit, and rework constraints before validation.
2. Define a bounded Rev A envelope with a separate rear carrier: preserves
   wearable intent and allows practical battery/magnet and rework provisions.

## Decision

Use a four-layer exposed PCB with black soldermask, a primarily LED front, and
most other components on the rear. Set **112 × 74 mm as the hard board maximum**,
with 108 × 72 mm nominal and the exact outline prototype-dependent. Use rounded
corners, edge-accessible USB-C/buttons/power switch, and a rear 3D-printed carrier
for the battery and magnetic attachment. Total thickness near or below 12 mm is
a target, not a measured envelope.

## Consequences

Battery/magnet placement must preserve vendor antenna keep-outs and avoid
stressing the cell. The pack is serviceable, not hot-swappable. Validate the
complete stack for NFC/BLE, retention, thickness, comfort, and battery protection.
Preserve SWD/test points and rework access. Actual outline, carrier geometry,
cell, and magnets need physical fit/retention evidence before release.

## References

- [Hardware constraints](../hardware.md)
- [Wearability, thermal, and wireless validation](../validation.md)
- [Raytac antenna keep-out and module drawings](../references.md#raytac-mdbt53-p1m)
