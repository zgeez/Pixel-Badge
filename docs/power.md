# Power and USB

Status: **design requirements; no schematic or measured results yet**. The
charger/regulator parts, exact battery pack, and final component values remain
provisional. Sources are indexed in [references.md](references.md).

## USB supply connections

USB-C is a standard 5 V sink with USB 2.0 data; no USB Power Delivery. VBUS
branches to the charger/power-path input **and** the MDBT53-P1M dedicated USB
VBUS input (pin 44). D+ connects to pin 46 and D− to pin 48. The module's
normal device supply is provided separately by the regulated logic rail.

Nordic's nRF5340 Product Specification, **USBREG — USB regulator control**, says
the USB peripheral needs the dedicated 5 V VBUS supply in addition to the main
VDD/VDDH supply. It requires input/output USB-regulator decoupling; **Reference
circuitry** recommends a series resistor for VBUS hot-plug transient immunity.

Raytac **MDBT53-1M & MDBT53-P1M Specification, Ver.E (2024-10-25)**,
§2.5 (p.17) identifies the module USB/power pins. Its normal-voltage DC/DC
reference circuit, §8.1.2 (p.44), shows **R22 = 2.2 Ω** in the dedicated module
VBUS branch and **C25 = 4.7 µF** from the module side of that resistor to ground.
These are manufacturer reference values, not a completed Rev A BOM. The
charger branch must not be fed through this module-only series resistor.

Follow the module-level reference circuit and check the selected supply mode
against Nordic's specification. DECUSB is a bare-SoC decoupling pin, not an
exposed pin in Raytac's §2.5 module pin table; do not invent a module pad for it.
Confirm the module's internal USB-regulator decoupling coverage with Raytac
before schematic sign-off, alongside the external VBUS circuit. Select final
capacitor voltage rating, tolerance and effective capacitance, resistor rating,
and transient/ESD parts from their primary specifications. Check the combined
USB input capacitance/inrush and source-resistance limits. A fitted 0 Ω test
link is a measurement provision, not protection or off-state isolation.

Review and then measure USB attachment/removal with the switch on/off, battery
present/absent, and normal supplies present/absent. Charging must remain possible
with the switch off. VBUS availability alone does not imply USB operation when
the MCU logic rail is off. Resolve any unsupported power sequencing or
back-power path before releasing the schematic; do not assume a test link fixes
it. Product USB firmware uses beyond routing the data interface remain future
work; a diagnostic USB image can validate the hardware interface.

## Independent battery protection

Rev A requires independent hardware battery protection. Prefer a commercially
protected 1-cell LiPo pack. Obtain the exact pack/cell specification and evidence
of its protection functions. If an unprotected cell is used, add an appropriate
dedicated protection circuit before schematic/fabrication release.

Protection must cover overdischarge, overcurrent, and short circuit, and provide
charge protection appropriate to the cell. Choose thresholds, delays, recovery
conditions, and charge/discharge current ratings from the selected pack/cell and
protector specifications; these values are **TBD**, not inferred from capacity.

The provisional BQ24074 is a charger and power-path controller. TI's **BQ2407x
datasheet, Rev.N**, documents charger/system protections, including OUT
short-circuit behavior (§§9.3.4.1.3 and 9.3.4.2); those functions do not replace a
dedicated battery protector covering the full battery discharge path. Firmware
matrix load shedding and low-battery warnings supplement hardware protection.

## Runtime and validation

The starting capacity target is approximately 1200 mAh. The runtime design
target is **about eight hours under representative career-fair use**, with no
runtime claim until measured. Agree on the brightness, display/mode mix, audio
duty, BLE activity, and end-of-use condition before running a full-charge to
normal-stop test. Use measured mode currents and usable pack capacity to revise
the budget; do not discharge beyond the cell's allowed limits.

Before wearable use, verify pack identity/protection, charger configuration,
current/thermal margins, low-battery behavior, and protection recovery. Use
current-limited fixtures or a battery simulator for fault tests; do not create
an uncontrolled short on a LiPo. Record source-derived limits and measured
results separately. See [validation.md](validation.md) and
[bringup.md](bringup.md).
