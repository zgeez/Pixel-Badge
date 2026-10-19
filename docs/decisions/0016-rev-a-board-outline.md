# 0016: Rev A board outline 118 × 76 mm

- Status: Accepted
- Date: 2026-10-16
- Deciders: Project owner

## Context

[ADR 0006](0006-rev-a-mechanical-envelope.md) set 112 × 74 mm as the hard
board maximum, with 108 × 72 mm nominal. The Rev A layout then outgrew it:

- The E73 module ([ADR 0015](0015-nrf52840-e73-module.md)) is larger than the
  module it replaced.
- On 2026-10-15 the board grew 2 mm taller (112 × 76 mm) for routing room
  under the shift registers.
- The east side still could not be routed cleanly. The router-made east
  routing averaged about 4.4 vias per net, left SYS unrouted and stacked
  signals broadside in the routing corridor. Each remaining connection would
  have crossed 5–18 existing nets.

## Options considered

1. Patch the existing east routing within 112 × 76 mm: keeps the outline;
   many vias and crossings, with crosstalk risk in the corridor.
2. Six-layer stack-up at the same size: more routing layers, a different
   stack-up and higher board cost. Not chosen.
3. Widen to 118 × 76 mm and re-plan the east side from a new floorplan: more
   room for the power, charger, NFC and module area, at the cost of a larger
   badge.

## Decision

Option 3. The Rev A hard board maximum is **118 × 76 mm**, and Rev A uses it.
The 108 × 72 mm nominal target and the rest of ADR 0006 are unchanged:
rounded corners, edge-accessible USB-C, buttons and switch, and the rear
3D-printed carrier.

## Consequences

- The badge is 6 mm wider and 2 mm taller than the ADR 0006 maximum. The
  NFC coil, the module and its antenna keep-out, and one carrier screw move
  6 mm east.
- The rear carrier is designed to the routed board outline, not the earlier
  envelope.
- Wearability, retention, weight and NFC/BLE performance with the carrier,
  battery and magnets still need physical validation.
- A smaller outline remains a later-revision option.

## References

- [ADR 0006: Rev A PCB and mechanical envelope](0006-rev-a-mechanical-envelope.md)
- [Hardware constraints](../hardware.md)
- [Hardware design files](../../hardware/README.md)
