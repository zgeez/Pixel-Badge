# 0021: Rev A board outline 118 × 76 mm

Status: Accepted

Date: 2026-10-16

## Decision

Use 118 × 76 mm as the Rev A hard maximum and actual board outline. The earlier
112 × 74 mm envelope grew 2 mm for the lower routing corridor, then 6 mm east
to route the module/power/NFC region cleanly.

Patching the smaller outline required many crossings/vias and left SYS
unrouted. A six-layer board would change cost and stack-up. Widening keeps
four layers and space for a more practical floorplan.

The coil, module/antenna region, and one carrier screw move east. Design the
carrier to the routed board, not the original envelope. Rounded corners,
accessible controls, and rear battery support remain. Retention, weight,
wearability, and NFC/BLE behavior still need physical tests.
[Hardware source](../../hardware/README.md) · [Original envelope](0006-rev-a-mechanical-envelope.md)
