# 0009: Portrait and landscape wear; magnetic and lanyard mounting

- Status: Proposed
- Date: 2026-10-07
- Deciders: Project owner

## Context

[ADR 0008](0008-landscape-front-buttons.md) fixed the badge as worn landscape
and placed the two buttons on the front, below the LED field, when they fit.
[ADR 0006](0006-rev-a-mechanical-envelope.md) set a rear 3D-printed carrier
holding the battery and the attachment magnets. The owner now wants the badge
to work like an ID badge: worn landscape or portrait, either held by magnets
through clothing (shirt, blazer, jacket or similar) or hung from a lanyard.

The 32 × 32 display is square, so content can be rotated in firmware. The
IMU (ICM-42688-P) can report the orientation. The magnets must hold through a
range of ordinary clothing, not one measured garment, and the lanyard is the
intended fallback for clothing that is too thick.

## Options considered

1. Keep landscape-only wear and magnets only (ADRs 0006 and 0008 as written):
   simplest, but does not meet the new requirement.
2. Both orientations and both mountings, with the lanyard load taken by the
   rear carrier (attachment points for landscape and for portrait) and the
   magnets placed for either orientation.
3. As option 2, with a lanyard hole in the PCB itself: fewer carrier features,
   but the board takes the load, the hole costs routing area, and it fixes one
   hanging orientation.

## Decision

Option 2.

- **Orientation:** the badge is worn landscape or portrait. Firmware rotates
  the displayed content (0°, 90°, 180°, 270° where practical) from the IMU's
  orientation, with hysteresis so the image does not flip back and forth. QR
  mode reads the orientation once on entry and keeps it until the mode is
  left. No single edge is assumed to be the bottom.
- **Mounting:** both magnetic attachment and lanyard attachment are required
  for Rev A. The rear carrier takes the lanyard load through attachment
  points for landscape and portrait hanging; the PCB carries no lanyard hole.
  The attachment points must not block USB-C, the power switch, the NFC
  coil, the module antenna, battery servicing, or the SWD header.
- **Magnets:** sized from pull-versus-gap data for a range of ordinary
  clothing, from thin shirts to typical blazers and jackets, with margin.
  Compatibility with any thickness is not claimed; the lanyard covers thicker
  clothing. Holding force and comfort need prototype validation.
- **Controls:** the two buttons stay on the front strip below the LED field
  (ADR 0008's placement rule) and are treated as orientation-independent:
  symbols or functions that do not depend on which edge is up.

## Consequences

This supersedes ADR 0008's landscape-only wear. ADR 0008's button placement
(front strip below the LED field when it fits, else the bottom edge below the
field) and its front-face consequences stay in force; "bottom edge" there now
means the edge below the field in the landscape layout, not the edge nearest
the ground. ADR 0006's magnetic attachment is extended to magnetic plus
lanyard mounting.

- The carrier gains lanyard attachment points and must be laid out around
  the battery, magnets, NFC coil and antenna for both hanging orientations;
  the hanging points line up with the centre of mass so the badge hangs
  level.
- Firmware gains orientation-aware rendering for the name, animations and QR
  views, and a locked orientation in QR mode.
- Validation covers both orientations and both mountings on a range of
  clothing (thin shirt, blazer, jacket), plus NFC and BLE with the magnets,
  battery and carrier installed.

## References

- [ADR 0006: Rev A PCB and mechanical envelope](0006-rev-a-mechanical-envelope.md)
- [ADR 0008: Landscape wear and front-face buttons](0008-landscape-front-buttons.md)
- [Hardware](../hardware.md)
- [Validation](../validation.md)
