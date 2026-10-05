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
3. For battery monitoring, a dedicated fuel gauge versus a switched divider to
   SAADC: a gauge adds capacity estimation; voltage sensing addresses load
   shedding with fewer BOM/firmware dependencies for Rev A.

## Decision

Use USB-C as a standard 5 V sink for charging and USB 2.0 data, without USB PD.
Fit separate 5.1 kΩ, 1% Rd resistors from CC1 and CC2 to ground, effective even
without MCU/battery power; do not join the CC pins.
VBUS supplies both the charger/power-path input and module VBUS pin 44 through
the module USB supply conditioning/decoupling required by primary references.
Route D+ to module pin 46 and D− to pin 48. Supply the normal MCU logic rail
separately; dedicated VBUS is also required for USB operation.

Prefer a commercially protected 1-cell LiPo pack. An unprotected cell requires
a dedicated hardware protection circuit; the provisional BQ24074 direction
does not replace full independent battery protection. Charging remains possible
with the physical switch off. Regulated logic and firmware-switchable LED rails
support low-battery load shedding.

Use a default-off high-side-switched divider from protected-pack VBAT to an
nRF5340 SAADC input for loaded battery-voltage sensing. A fuel gauge is not
selected: accurate remaining-capacity/runtime estimation is not required for
Rev A load shedding. Divider/switch values, measurement error/timing and final
shed/recovery thresholds await calculations and pack characterization.

## Consequences

Pack, charger/regulator parts, protection limits, and final circuit/BOM values
remain open/provisional. Check hot-plug, inrush, off-state back-powering, charge
and load current margins, thermal behavior, and protection/recovery before
release. Approximately 1200 mAh and eight hours of representative use are
design targets, not measured claims. USB firmware features such as CDC and DFU
are future work; hardware diagnostics can use a dedicated test image.
Resolve source-compliant input-current limits before schematic freeze; Rd alone
does not authorize fixed 500 mA or higher current. Validate both orientations
and dead-battery attachment. Record a proposed load-shed policy/margin before
freeze, then measure it before accepting thresholds; invalid/stale measurements
keep the matrix disabled. This is additional control, not battery protection.

## References

- [Power, USB circuit references, and battery protection](../power.md)
- [Nordic, Raytac, and TI primary specifications](../references.md)
- [Electrical and runtime validation](../validation.md)
