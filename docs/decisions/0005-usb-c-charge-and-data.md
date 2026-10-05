# 0005: USB-C charging and USB 2.0 data

- Status: Proposed
- Date: 2026-10-04
- Deciders: Project owner

## Context

Rev A needs rechargeable battery power, charging with the badge switched off,
and a routed USB data interface for future diagnostics/recovery. Independent
battery protection is required for the wearable power system.

## Options considered

1. Charge-only USB-C: simpler routing, but omits the selected future data path.
2. Standard 5 V USB-C sink with charge/power path and USB 2.0 data: supports the
   selected interfaces without adding USB Power Delivery.

## Decision

Use USB-C as a standard 5 V sink for charging and USB 2.0 data, without USB PD.
VBUS supplies both the charger/power-path input and module VBUS pin 44 through
the module USB supply conditioning/decoupling required by primary references.
Route D+ to module pin 46 and D− to pin 48. Supply the normal MCU logic rail
separately; dedicated VBUS is also required for USB operation.

Prefer a commercially protected 1-cell LiPo pack. An unprotected cell requires
a dedicated hardware protection circuit; the provisional BQ24074 direction
does not replace full independent battery protection. Charging remains possible
with the physical switch off. Regulated logic and firmware-switchable LED rails
support low-battery load shedding.

## Consequences

Pack, charger/regulator parts, protection limits, and final circuit/BOM values
remain open/provisional. Check hot-plug, inrush, off-state back-powering, charge
and load current margins, thermal behavior, and protection/recovery before
release. Approximately 1200 mAh and eight hours of representative use are
design targets, not measured claims. USB firmware features such as CDC and DFU
are future work; hardware diagnostics can use a dedicated test image.

## References

- [Power, USB circuit references, and battery protection](../power.md)
- [Nordic, Raytac, and TI primary specifications](../references.md)
- [Electrical and runtime validation](../validation.md)
