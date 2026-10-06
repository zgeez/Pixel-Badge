# 0007: 2500 mAh pack, switching charger, and USB-powered charging state

- Status: Proposed
- Date: 2026-10-05
- Deciders: Project owner

## Context

[ADR 0005](0005-usb-c-charge-and-data.md) selected a USB-C sink with a
power-path charger, a protected pack of about 1200 mAh, and charging with the
physical switch off, with the BQ24074 as the provisional charger. With the
switch off the MCU was unpowered, could not read the CC pins, and left the
charger at its 100 mA default. The owner then chose a 2500 mAh pack and about
1 A charging. A linear charger at 1 A dissipates about 1.6 W next to a cell
that has no thermistor, and USB-A chargers that advertise only Default current
would still charge at about 100 mA.

## Options considered

1. Keep the BQ24074 linear charger with a 1 A setting: fewest changes, but
   about 1.6 W of heat at the start of fast charge, no BC1.2 detection, and
   switch-off charging still needs the MCU.
2. BQ25629 switching charger: BC1.2 detection and low loss, but no power-good
   output for waking the logic rail and very low distributor stock.
3. BQ25620 switching charger: BC1.2 detection, power-good output, ADC and JEITA
   temperature input with comparators active from reset; needs an I²C driver
   and more board parts.

## Decision

Use the Adafruit 328 protected 1-cell pack (2500 mAh, no thermistor) and a TI
BQ25620 switching charger with NVDC power path, targeting about 1.0 A charge
(the pack allows 1.2 A). The charger's power-good output enables the logic rail
whenever valid USB power is present, so the logic rail is enabled by the power
switch **or** USB. With USB attached and the switch off, firmware runs a
charging-only state: it reads CC and the charger's BC1.2 result and sets the
input limit, while the LED rail, display and other user-facing functions stay
off. Removing USB with the switch off turns the logic rail off in hardware.

A board-mounted NTC next to the pack feeds the charger's temperature input. Its
network places the charger's reset thresholds so that charging stops below
about 0 °C and above 45 °C board temperature without firmware. It measures the
board near the cell, not the cell. The charger's CE input is pulled high, so it
charges only after firmware has set the input limit and temperature settings.
The column-driver current is capped so that a firmware fault cannot exceed the
cell's 1.5 A continuous discharge rating, allowing for boost efficiency, the
highest LED-rail voltage, driver tolerance and the logic load.

This supersedes ADR 0005's provisional charger direction, its 1200 mAh capacity
target, and its implied meaning of the physical switch. The rest of ADR 0005
(USB-C sink without PD, separate Rd, VBUS to module pin 44, protected pack,
switched VBAT divider) is unchanged.

## Consequences

The power switch is a control input, not electrical isolation: the logic rail
runs whenever USB is attached. Firmware must implement the charger driver,
watchdog service, the input-current policy (100 mA for unconfigured hosts and
unknown adapters until enumeration or a valid CC advertisement), the
charging-only state, and temperature thresholds. Charge time and runtime are
not claimed until measured; 2500 mAh is more energy, not a runtime result. The
pack is 50.5 × 62.5 × 8.1 mm and 50 g, which constrains rear component
placement, the NFC coil and antenna clearance, and wearability. The battery
header follows Adafruit's pinout (pin 1 BAT−, pin 2 BAT+); each pack is
meter-checked before its first connection.

## References

- [Power, charging, and power states](../power.md)
- [TI BQ25620/BQ25622 datasheet, SLUSEG2D](../references.md#component-datasheets)
- [Adafruit 328 and PKCELL LIPO785060 specification](../references.md#component-datasheets)
