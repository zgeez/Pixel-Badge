# 0007: 2500 mAh pack, switching charger, and USB-powered charging state

Status: Accepted

Date: 2026-10-05

## Decision

Replace the linear-charger direction with BQ25620 switching/NVDC power path
and an Adafruit 328 protected 2500 mAh pack. The original charge target was
about 1 A; a linear charger would dissipate about 1.6 W near the cell. BQ25620
also provides BC1.2 detection, ADC, and power-good output.

Enable logic from the switch **or** USB power-good. With USB and the switch
off, run charger/current-policy management while display, audio, and radio stay
off. Removing USB then turns logic off in hardware.

CE defaults inactive until firmware configures the charger. The board NTC
measures board temperature, so cell/board offset needs testing. Independent
pack protection remains required. Cap column current against the pack's
discharge rating and meter-check connector polarity.

The original 1 A target is not permission to charge an unverified pack.
[Current charge limits and first-charge configuration](../power.md) ·
[Electrical tests](../testing/electrical.md)
