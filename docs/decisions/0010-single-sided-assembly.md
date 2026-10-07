# 0010: Single-sided assembly with all components on the front

- Status: Proposed
- Date: 2026-10-07
- Deciders: Project owner

## Context

[ADR 0006](0006-rev-a-mechanical-envelope.md) chose a primarily LED front
with most other components on the rear, and
[ADR 0008](0008-landscape-front-buttons.md) added the buttons to the front, so
the board was assembled on both sides. The owner wants the board assembled on
one side only: the LED matrix and every other component on the front, and
the rear holding only the battery, its connector and the carrier.

## Options considered

1. Two-sided assembly (ADRs 0006 and 0008 as written): a clean LED front, but
   two assembly sides and components on the side that faces the battery and
   the wearer.
2. Single-sided assembly, all components on the front; the battery connector
   is a through-hole part whose body sits on the rear and whose pins are
   soldered from the front.
3. As option 2 with the battery connector on the front and the pack lead
   passing through a slot or around the board edge: fully single-sided, but
   the lead shows on the front.

## Decision

Option 2.

- All components are on the front, around the LED field: the row switches
  beside the field, the column drivers, row registers and buttons in the
  strip below it, and the power, charger, MCU, sensors, USB-C, switch and
  SWD header in the region beside the NFC coil.
- The rear holds only the battery, the battery connector body (JST
  S2B-PH-K-S, through-hole, side entry) and the carrier. No other part, test
  point, exposed pad, protruding pin or hot part is under the battery; inner
  copper and tented vias are allowed, with an insulating layer between board
  and pack.
- The battery connector is inserted from the rear and soldered from the
  front. It is hand-soldered after assembly by default; the assembler fits it
  only if it explicitly confirms rear insertion. Pin 1 = BAT−, pin 2 = BAT+;
  each pack's polarity is checked with a meter before its first connection.
- The bottom-port microphone keeps its PCB acoustic hole; the carrier gives
  the hole a vent channel to the board edge.
- Tall front parts are replaced by lower-profile parts where a suitable part
  exists (the LED-rail and charger inductors become 2.1 mm Coilcraft
  XGL4020-102MEC); the USB-C receptacle and the SWD header stay at front
  edges.
- Thickness targets are regional: ≈ 12 mm over the LED field, ≈ 13.5 mm over
  the electronics region, with local edge exceptions of ≈ 14.5 mm at USB-C
  and ≈ 16.5 mm at the SWD header. These are calculated targets, not
  measurements.

## Consequences

This supersedes ADR 0006's "most other components on the rear" and ADR 0008's
"assembled on both sides". The front is no longer LEDs only: it shows the LED
field and a populated electronics region. The status LED moves to the front.
The board NTC sits on the front at the edge over the pack: it measures board
temperature, not cell temperature, through the PCB, so its coupling to the
cell is weaker than a part touching the pack, and the board-to-cell offset is
measured during validation. Front part heights add directly to the total
thickness, which replaces ADR 0006's single ≈ 12 mm target with the regional
targets above. Assembly is one SMT side plus one hand-soldered through-hole
part.

## References

- [ADR 0006: Rev A PCB and mechanical envelope](0006-rev-a-mechanical-envelope.md)
- [ADR 0008: Landscape wear and front-face buttons](0008-landscape-front-buttons.md)
- [ADR 0009: Portrait and landscape wear; magnetic and lanyard mounting](0009-dual-orientation-dual-mounting.md)
- [Hardware](../hardware.md)
