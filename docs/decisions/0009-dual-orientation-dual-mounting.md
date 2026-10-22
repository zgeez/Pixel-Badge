# 0009: Portrait and landscape wear; magnetic and lanyard mounting

Status: Accepted

Date: 2026-10-07

## Decision

Support portrait/landscape wear with both magnets and a lanyard. The carrier
takes the lanyard load; a PCB lanyard hole would consume routing area and
fix the hanging direction.

Rotate content in quarter turns using IMU input and hysteresis. QR takes its
orientation on entry and keeps it until the mode is left. Button symbols stay
orientation-independent.

Size magnets from pull-versus-gap data for ordinary clothing, then test holding
force and comfort. The lanyard covers thicker garments. Attachment points must
clear the battery, antennas, USB, switch, and SWD access, and hang near the
centre of mass in both orientations.
[Carrier tests](../testing/system.md#11-mechanical-wearability) ·
[Motion behavior](../motion-prototype.md)
