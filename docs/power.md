# Power and USB

Status: **design requirements; no schematic or measured results yet**. The
charger/regulator parts, exact battery pack, and final component values remain
provisional. Sources are indexed in [references.md](references.md).

## USB supply connections

The receptacle must present **independent Rd to ground on both CC1 and CC2**.
Use two discrete **5.1 kΩ, 1%** resistors; do not short CC1 to CC2 or switch Rd
with MCU power. USB-IF **USB Type-C Cable and Connector Specification Release
2.5 (March 2026)**, §4.5.1.3.2/Figure 4-9 requires both terminations. Table 4-28
allows 5.1 kΩ ±20% for attachment without current detection, or ±10% for
current detection, including all parallel resistance. The selected 1% component
tolerance is an engineering choice within those limits; account for ESD/sense
loading in the complete circuit. No PD controller is required for this choice.
Verify attachment and VBUS availability with a compliant C-to-C source in both
orientations, including switch-off and battery-absent states.

Rd identifies a sink; it does not authorize an arbitrary charging current.
Before schematic freeze, resolve charger input-current limits for the supported
source states, including Default USB Power, unconfigured/suspended USB and
switch-off charging (§4.6.2.1 and its USB-specification requirements). Do not
assume a fixed 500 mA mode is permitted before USB configuration, or draw
1.5/3 A without detecting the source's advertisement. This current-policy review
remains open.

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

## Battery-state measurement

Rev A selects a **default-off, high-side-switched VBAT divider into nRF5340
SAADC**, measuring the protected pack output upstream of the charger power
path. SYS can differ from VBAT on USB; measuring the regulated MCU supply does
not establish battery voltage. No dedicated fuel gauge is selected.

| Consideration | Dedicated fuel gauge | Switched voltage divider + SAADC (selected) |
|---|---|---|
| Accuracy/usefulness | Can estimate state of charge and remaining capacity with suitable chemistry/configuration | Measures loaded voltage for shedding; voltage alone is not accurate state of charge or remaining runtime |
| Quiescent current | Device/mode-dependent continuous draw | Disabled-switch leakage plus periodic measurement cost; calculate and measure, not assumed zero |
| BOM/area | Additional IC, supporting parts and integration | Divider, switch/default bias and optional filtering; uses MCU ADC |
| Firmware/schedule | Gauge driver, battery configuration and validation | Sampling, calibration, threshold policy and validation; fits the existing Rev A plan |

Voltage sensing is appropriate for the intended load-shedding policy if the
error/timing margins below are demonstrated. A gauge's runtime-estimation
benefit does not justify adding it
for Rev A's limited budget/schedule. Reconsider if reliable remaining-capacity
estimates become a requirement. TI's BQ27427 Rev.B is a primary example of a
gauge with chemistry selection and capacity reporting, not a selected part.
Its small 9-ball DSBGA package also adds assembly/rework considerations; actual
cost and occupied area depend on selected parts/footprints, not sophistication.

Before schematic freeze:

- Select the switch, divider ratio/tolerances and any filter from the selected
  pack maximum voltage and minimum logic supply. Nordic SAADC **Reference**
  requires AIN within VSS–VDD even if the gain/reference implies a larger
  conversion range. Check normal operation, startup, charging and MCU-off states.
- Disconnect VBAT on the divider's high side by default when logic is off or in
  reset; hold the ADC node at a defined safe potential. A low-side-only switch
  can leave VBAT driving the unpowered input. Confirm leakage/back-powering
  against component specifications; final switch and resistor values are TBD.
- Choose ADC reference/gain, offset calibration, acquisition time for the
  divider's source impedance, and settling delay after enabling it (Nordic
  SAADC **Digital output**, **Reference**, **Acquisition time**). Disable the
  measurement path after sampling. Budget switch leakage and measurement duty.
- Provide VBAT and sense-node measurement access; account for probe loading
  when comparing ADC readings with a calibrated meter. Record an error budget
  covering ADC/divider errors, leakage, settling, temperature and load noise.
- Define **PROPOSED** shed/recovery thresholds, hysteresis, sampling interval
  and reaction time with margin above selected pack/protector cutoff and the
  regulator's usable input/load limit, allowing for power-path drop, measurement
  error and load sag. Characterize the actual pack before accepting final values;
  no final voltage or accuracy threshold is asserted here.

Firmware must cap brightness and then disable the LED rail before logic becomes
unstable or protection trips, under the validated operating load envelope.
Missing, stale or invalid measurements keep the matrix disabled until a valid
safe reading is available. Recovery needs hysteresis/controlled re-enable to
avoid a shed–voltage-rebound–restart loop. Independent hardware protection is
still the final backstop. Validate the measurement and timing margins using a
battery simulator, then the selected pack discharge profile; see
[validation.md](validation.md#12-power-cycling-brownout-and-recovery).

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
