# 0010: Single-sided assembly with all components on the front

Status: Accepted

Date: 2026-10-07

## Decision

Assemble every SMT component on the front. Keep only the battery, connector
body, and carrier on the rear. Insert the through-hole JST connector from the
rear and solder from the front, by hand unless the assembler confirms this step.
Pin 1 is BAT− and pin 2 BAT+; verify each pack with a meter.

Keep parts, exposed pads, protruding pins, and heat sources out from under the
pack. Inner copper/tented vias are allowed; add insulation between PCB and cell.
The bottom-port microphone needs an acoustic hole and a carrier vent.

This reduces assembly-side complexity but puts electronics beside the LED field.
Calculated thickness targets are 12 mm over the LEDs, 13.5 mm over electronics,
with 14.5 mm USB and 16.5 mm SWD edge exceptions. They are not measurements.
The front NTC's board-to-cell temperature offset requires testing.
[Hardware](../hardware.md) · [Power/thermal tests](../testing/electrical.md)
