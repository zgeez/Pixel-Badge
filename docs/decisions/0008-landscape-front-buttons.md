# 0008: Landscape wear and front-face buttons

- Status: Accepted
- Date: 2026-10-06
- Deciders: Project owner

## Context

[ADR 0006](0006-rev-a-mechanical-envelope.md) set the 108 × 72 mm nominal board
(112 × 74 mm maximum) with edge-accessible USB-C, buttons and power switch, and
a primarily LED front. The owner has since specified that the badge is worn in
landscape orientation, with the two buttons below the LEDs if they fit there.
With the 32 × 32 field at 2.0 mm pitch (about 64 mm square) in a 72 mm tall
board, the space below the field is only a few millimetres, so the button
placement depends on the outline and the final LED pitch.

## Options considered

1. Keep side-actuated buttons on an edge (ADR 0006 as written): no front parts
   besides the LEDs, but the buttons are harder to find and press while worn.
2. Top-actuated buttons on the front face, in the strip below the LED field:
   easy to press while worn; needs the field moved toward the top edge and a
   small enough switch, and adds two parts to the LED face.
3. Option 2 when it fits, otherwise option 1 on the bottom edge, directly below
   the field.

## Decision

Option 3. The badge is worn landscape. The two buttons (Mode/Next,
Action/Select) are top-actuated tactile switches on the front face, in the
strip below the LED field, if the switch land pattern fits there with LED and
board-edge clearance when the outline is fixed. With the field about 1 mm from
the top edge, a 72 mm tall board leaves a strip of roughly 7 mm below it. The
selected XUNPU TS-1088-AR02016 needs 2.0 × 5.5 mm of pads. If the strip is too
small, for example after a pitch increase from QR testing, the buttons become
side-actuated parts on the bottom edge below the field. USB-C stays on the
bottom edge and the power switch on an edge, as in ADR 0006.

## Consequences

This supersedes ADR 0006's edge-accessible button requirement and its LED-only
front: the front carries the LEDs and, in the preferred case, the two buttons.
The board is assembled on both sides in any case. The LED field moves toward the
top edge, and the top-left corner radius must clear the corner LED. The final
placement is confirmed at outline freeze and checked again if the LED pitch
changes.

## References

- [ADR 0006: Rev A PCB and mechanical envelope](0006-rev-a-mechanical-envelope.md)
- [Hardware](../hardware.md)
- [XUNPU TS-1088 tact switch drawing](../references.md#component-datasheets)
