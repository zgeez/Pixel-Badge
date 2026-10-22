# 0005: USB-C charging and USB 2.0 data

Status: Accepted; charger/capacity/switch behavior amended by [0007](0007-battery-charger-power-states.md)

Date: 2026-10-04

## Decision

Use USB-C as a 5 V sink with USB 2.0 data and no PD. Fit independent 5.1 kΩ
Rd resistors on both CC pins, present without MCU power. VBUS feeds both the
charger and module USB supply; logic has its own regulator.

Use a protected 1S pack, separate logic/LED rails, and a default-off switched
VBAT divider for load shedding. A fuel gauge adds capacity estimation that
Rev A does not require. Charging with the switch off is required.

The original BQ24074/1200 mAh direction was provisional and later replaced.
USB current permission, inrush, off-state behavior, thermal limits, and pack
protection must be verified; Rd alone does not authorize 500 mA.
[Current power design](../power.md) · [Sources](../references.md)
