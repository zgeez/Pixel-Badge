# Power and USB

Status: **parts selected; regulators/power-tree and USB/charger/battery sheets
drawn; no measured results**. Values marked PROVISIONAL remain so until prototype
measurements. Sources are indexed in [references.md](references.md).

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
The BQ25620 charger sets its input current limit (IINDPM) over I²C. Firmware
applies this policy (USB Type-C R2.5 §4.6.2.1; sink thresholds from Table 4-36
for Rd ±10%; tSinkAdj 60 ms; BC1.2 detection by the charger on D+/D−):

| Source state | Input limit (IINDPM) |
|---|---|
| CC advertises 1.5 A or 3.0 A (vRd ≥ 0.746 V) | 1.5 A |
| BC1.2 dedicated or charging downstream port (DCP/CDP) | 1.5 A |
| USB host port (SDP) or unknown 5 V adapter, before USB configuration | 100 mA |
| SDP after the badge is configured by the host | 500 mA |
| USB suspended | Suspend current |

The BQ25620 runs BC1.2 detection itself at plug-in and sets 500 mA for an SDP
or unknown adapter. Its CE input is pulled up to 3V3, so it does not charge
until firmware has configured it: firmware sets IINDPM to 100 mA, reads the
detection result, sets the temperature thresholds and only then drives CE low.
Until then the input carries the logic load only (the LED rail is held off),
not charge current; that this stays below 100 mA is checked at bring-up. An
MCU reset releases CE and stops charging. If the charger's I²C watchdog
expires, it re-enables charging at half ICHG; it does not reset IINDPM or the
temperature thresholds (SLUSEG2D §8.6), and both the reset and the firmware
thresholds stay within the cell's range (below).

Firmware holds 100 mA until enumeration or a valid CC advertisement. It samples
CC1 and CC2 at least every 20 ms while VBUS is present, uses the higher reading
as the active CC, treats readings in the specification's threshold bands as the
lower advertisement, and returns to the lower limit within tSinkAdj when the
advertisement drops.

The CC voltage (up to 2.042 V per Table 4-36) is present whenever USB is
attached, including with the switch OFF or no battery, when the MCU supply is
0 V. Nordic's absolute maximum rating for I/O pins is VDD + 0.3 V (nRF5340 PS,
Absolute maximum ratings), so a series resistor alone is not acceptable. Each
CC sense path is: CC → 100 kΩ → AO3400A N-MOSFET pass gate (drain on the CC
side, gate tied to 3V3) → SAADC pin, with **12 kΩ and 100 nF** from the pin to
ground (ratio 0.107).

- **Unpowered:** the MOSFET is off (VGS = 0). Its leakage is at most 5 µA
  (AOS: IDSS at 30 V, 55 °C), so the pin stays ≤ 60 mV. Even with the MOSFET
  failed short, a compliant CC voltage (≤ 2.042 V) gives ≤ 0.22 V. A 1 MΩ
  pull-down, as first proposed, does not meet the limit: leakage × 1 MΩ can far
  exceed 0.3 V.
- **Powered:** the pin never exceeds 0.22 V, so VGS ≥ 3.0 V, well above the
  1.45 V maximum threshold; RDS(on) (< 48 mΩ at 2.5 V) is negligible against
  100 kΩ and readings do not clip. The body diode (pin side to CC side) never
  conducts. During power-up the MOSFET cannot conduct before VDD exceeds its
  threshold, so the pin stays within VDD + 0.3 V.
- **Thresholds at the pin:** Default ≤ 0.612 V → ≤ 0.066 V; 1.5 A ≥ 0.746 V →
  ≥ 0.080 V (±1.6% resistor ratio). Use gain 2 with the 0.6 V reference
  (0.3 V full scale). Confirm SAADC offset/gain error after calibration
  against this ±6 mV margin.
- **Loading:** 112 kΩ in parallel with Rd while powered gives 4.88 kΩ, within
  Table 4-28's ±10%; no load while unpowered.
- **Filtering:** 100 nF with 10.7 kΩ (τ ≈ 1.1 ms) averages PD signalling from a
  PD-capable source; wait ≥ 8 ms after boot before trusting readings (100 mA
  applies meanwhile).

Verify the pin voltage against VDD with USB attached in both orientations at
switch-off, with the battery absent, and through power-up/down (validation
§2).

USB presence powers the logic rail through the charger's PG output (see
[Power states](#power-states)), so the MCU can read CC and set the input limit
with the power switch OFF. No PD is used.

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
with the switch off. With USB present the logic rail is on (PG path), so the
module's VBUS and VDD supplies are present together. Resolve any unsupported
power sequencing or back-power path before releasing the schematic; do not
assume a test link fixes it. Product USB firmware uses beyond routing the data
interface remain future work; a diagnostic USB image can validate the hardware
interface.

## Power states

The power switch is a signal-level control, not electrical isolation:
`EN_3V3 = switch ON or charger PG (valid VBUS)`.

| USB | Switch | 3V3 | Behavior |
|---|---|---|---|
| No | OFF | Off | Badge off; only battery-side quiescent currents remain |
| No | ON | On | Normal operation |
| Yes | OFF | On | **Charging-only** firmware state: charger management, CC/USB detection and the charger ADC on; LED rail, matrix, microphone, IMU, NFC I²C side and BLE off |
| Yes | ON | On | Normal operation while charging; charge current falls as system load rises |
| Removed while OFF | OFF | Off (hardware) | PG releases and the enable pull-down turns the logic rail off; no firmware involvement |

The switch drives EN through 10 kΩ with a 20 kΩ pull-down; PG drives a P-MOSFET
from SYS. EN stays below SYS (< 4.6 V; TPS63802 EN abs max 6 V). Leakage of the
off P-MOSFET (≤ 5 µA) gives ≤ 85 mV on EN. The MCU reads the switch through an
open-drain N-MOSFET (`PWR_SW_N`), so no SYS-level signal reaches a GPIO. The
LED rail stays off at every boot (LED_EN pull-down); firmware enables it only in
normal operation. Passive NFC works in every state.

## Independent battery protection

Rev A requires independent hardware battery protection. The selected pack is
**Adafruit 328** (PKCELL LIPO785060, 3.7 V, 2500 mAh, about 10 Wh, genuine
2-pin JST-PH, integrated protection board, no thermistor):

| Item | Value | Source |
|---|---|---|
| Capacity | 2375 mAh min, 2500 mAh typ | PKCELL specification |
| Maximum charge current | 1200 mA (½C or less recommended) | Adafruit (PKCELL allows 1500 mA; the stricter limit applies) |
| Maximum continuous discharge | 1500 mA | PKCELL specification |
| Charge / discharge temperature | 0–45 °C / −20 to 60 °C | PKCELL specification |
| Cell discharge cut-off | 3.0 V | PKCELL specification |
| Protection IC | Ricoh R5478N101CD: overcharge 4.280 V, over-discharge 2.300 V (release 3.000 V), excess discharge 0.130 V across the MOSFET pair, short 0.75 V | PKCELL drawing; Ricoh EA-291-121009 |
| Protection trip current | Depends on the pack's MOSFET resistance; not specified | — |
| Size / mass | 50.5 × 62.5 × 8.1 mm max with protection board; 50 g | PKCELL drawing; Adafruit |

The protection board's over-discharge threshold (2.3 V) is below the cell's
rated 3.0 V cut-off, so firmware load shedding must act well above it. The
board header uses pin 1 = BAT− and pin 2 = BAT+, as in Adafruit's own charger
boards (product 328 is stated to match them; the "reverse plug" note on the
cell drawing describes that reversal). Meter-check each pack before its first
connection.

The BQ25620 is a charger and power-path controller; its protections do not
replace a dedicated battery protector covering the full discharge path.
Firmware matrix load shedding and low-battery warnings supplement hardware
protection.

## Regulators and charge current

**Charger.** TI BQ25620 switching charger (SLUSEG2D) with NVDC power path:
SYS is held at or above the minimum system voltage (3.52 V) while USB is
present and follows the battery otherwise. The **input** limit (IINDPM, table
above) and the **battery charge** current (ICHG) are separate settings; system
load has priority and the charge current falls when the input limit is reached.
ICHG powers up at 1040 mA (±5.5%: 0.98–1.10 A), below the pack's 1.2 A limit
without firmware; firmware targets about 1.0 A. When the I²C watchdog expires
the charger halves ICHG. VREG powers up at 4.200 V. A switching charger
dissipates far less than a linear one at this current; thermal performance
still needs measurement. Charge time depends on precharge, the CV taper,
input limiting and thermal regulation; it is not stated until measured.

**Temperature.** A 10 kΩ NTC (Murata NCP15XH103F03RC, B25/50 3380 K) on the
PCB next to the battery feeds the charger's TS pin through RT1 = 10 kΩ from
REGN, with the RT2 position left open. The TS comparators act from reset, with
no firmware. TI's 5.23 kΩ / 30.1 kΩ network for a 103AT NTC would place them at
0 °C and 60 °C, and the reset JEITA settings (REG0x1A/REG0x1B) keep full charge
current up to 60 °C, outside the cell's 0–45 °C range. With the selected
network the reset thresholds fall at these board temperatures (worst case over
the comparator limits, ±1% resistor, and the NTC's ±1% R25 and ±1% B):

| Threshold (reset setting) | TS, % of REGN | Board temperature |
|---|---|---|
| COLD: charging suspended below | 72.8–73.8 | −1.5 to 1.1 °C |
| COOL: ICHG 20% below | 67.9–68.9 | 4.2–6.7 °C |
| WARM: VREG − 200 mV above | 44.3–45.3 | 29.5–31.9 °C |
| HOT: charging suspended above | 33.9–34.9 | 41.7–44.5 °C |

Firmware selects `TS_TH4_TH5_TH6 = 011`, which moves WARM to about 38–40 °C
and leaves HOT unchanged. Only a power-on or register reset returns to the
table above; the I²C watchdog does not. Fitting 30.1 kΩ at RT2 restores TI's 0–60 °C
network if the board-to-cell offset proves it necessary. The charger's ADC
reports the reading. **Rev A has no direct cell-temperature sensing:** the NTC
measures the board next to the cell, which charger heat can make warmer than
the cell (conservative at HOT, optimistic at COLD).

**Logic rail.** TPS63802 buck-boost, 3.30 V (FB 112 kΩ / 20 kΩ, VFB 500 mV),
Murata DFE201612E-R47M (0.47 µH; Isat 5.5 A at −30% L) against a calculated
1.21 A requirement (boost mode, VIN 2.9 V, 0.5 A design load, L 0.37 µH,
1.4 MHz, +20%). MODE low (power-save); forced-PWM option DNP. Enabled as
described in [Power states](#power-states).

**LED rail.** TPS61023 boost with load disconnect; `LED_EN` with a 100 kΩ
pull-down keeps it off while the MCU is off, in reset or in the charging-only
state. VLED is **PROVISIONAL** at 4.77 V typ (33 kΩ / 4.7 kΩ, VREF 580–610 mV:
4.65–4.89 V). The TPS61023 only boosts: when its input exceeds the setting it
stops switching and passes the input through (SLVSF14B §7.3.5), so
`VLED ≈ max(setting, SYS − drop)`. SYS reaches about VBAT + 50 mV with charging
done or disabled (SLUSEG2D §8.3.4.1), about 4.25 V on a full pack, so settings
below about 4.4 V are not regulated. The trim positions cover settings of about
4.4–5.2 V once the LED is chosen. Coilcraft XGL4020-102MEC (1.0 µH ±20%,
2.10 mm max; Isat 3.8 / 6.3 / 8.8 A at 10 / 20 / 30% inductance drop at
25 °C, Coilcraft Document 1529-1).

The inductor currents below are **calculated estimates with stated
assumptions, not guaranteed bounds**. TI gives the TPS61023 switching
frequency (1.0 MHz) and valley current limit (2.7 A minimum, 3.7 A typical)
without a guaranteed minimum frequency or maximum current limit. With VIN
2.9 V, VOUT 5.33 V, 0.66 A (32 × 19.4 mA + 5% for temperature [estimate],
plus logic), η 0.8 and L 0.72 µH (−20% tolerance, −10% saturation), the
normal-operation peak is 2.66 A at 1.0 MHz and 2.94 A at an assumed 0.8 MHz; it
would reach the 3.8 A (10% drop) point only if the frequency fell to about
0.5 MHz. Overload and current-limit peaks are not bounded by the datasheet.
The charger uses the same inductor (L301): 3.0 A at the 1.5 A input limit
(efficiency taken as 100% as an upper bound, VBUS 5.5 V, SYS 3.52 V,
1.35 MHz, the BQ25620's specified minimum), 5.7 A if firmware left the 3.2 A
power-on input limit.

**Release gate:** on Rev A, measure the TPS61023 switching frequency at
minimum VIN and the L202 and L301 peak and ripple current at maximum load, at
start-up and in current limit (LED-rail overload), before the inductors are
considered qualified ([validation.md](validation.md#2-electrical-correctness)).

**Battery discharge limit.** The cell's 1.5 A continuous rating bounds the
display. The worst case is a firmware fault with all 32 columns sinking their
full current at 100% on-time; the column drivers fix the total at
32 × I_column whatever the rows do. The battery current is then

`I_BAT = 32 × I_column,max × VLED,max / (η × VBAT) + I_logic`

with η = 0.8 (LED boost), VBAT = 3.0 V (cell cut-off), I_logic ≈ 60 mA and
I_column,max = 1.08 × the R_IREF setting (TLC59283 IOLC 32.9–37.7 mA at a
35.3 mA target at 25 °C, plus the 1% resistor; drift over temperature,
SBVS199C Figure 5-9, and the logic load are checked at bring-up). Holding
I_BAT ≤ 1.5 A gives `I_column ≤ 0.1 V·A / VLED,max`:

| VLED maximum | Nominal column-current setting | R_IREF (E96, 1%) |
|---|---|---|
| 5.33 V (5.2 V trim, VREF high) | 18.0 mA | 2.94 kΩ |
| 4.89 V (4.77 V setting, VREF high) | 19.8 mA | 2.67 kΩ |

A 25 mA cap would allow 1.8–2.0 A, so it does not protect the cell. Rev A uses
the first row: **18.0 mA nominal, R_IREF 2.94 kΩ 1%**. With driver and resistor
tolerance the column current can reach about 19.4 mA (1.08 × 18.0 mA at 25 °C),
which is the value the limit above uses, so it holds over the full VLED trim range. Each driver's R_IREF has a fitted 0 Ω in series, so adjustment can
only raise the resistance and lower the current; nothing may be fitted in
parallel with it. The final peak current at or below that setting remains
prototype-dependent.

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

Drawn circuit: AO3401A high-side P-MOSFET between the pack and a 47 kΩ/47 kΩ
1% divider (per-board calibration), gate pulled to VBAT by 47 kΩ (off by
default) and switched by an AO3400A from `VBAT_MEAS_EN` with a 100 kΩ
pull-down; 100 nF at the ADC node (sample after ≥ 16 ms, about seven time
constants). The node stays ≤ 2.1 V at 4.2 V (gain ¼, 2.4 V full scale). With
the MCU unpowered, P-MOSFET leakage (≤ 5 µA, 55 °C) × 47 kΩ keeps the node
≤ 0.24 V, and N-MOSFET leakage × 47 kΩ keeps the P-MOSFET gate within 0.24 V of
VBAT, below its 0.5 V minimum threshold.

**PROPOSED** Rev A firmware defaults (configurable; not accepted thresholds
until characterized with the selected pack):

| Condition | Default action |
|---|---|
| Loaded VBAT < 3.55 V | Cap brightness (≤ 50 % on-time) |
| Loaded VBAT < 3.40 V for 3 consecutive 100 ms samples | Disable the LED rail |
| After a shed: battery-qualified (unloaded VBAT ≥ 3.70 V for 10 s), **or** input-qualified (charger input limit ≥ 500 mA established, USB present, and unloaded VBAT ≥ 3.50 V) | Allow controlled re-enable (below) |
| Unloaded VBAT < 3.30 V | Enter the lowest-power firmware state |
| Missing, stale or invalid reading | Keep the matrix disabled |

USB presence alone does not qualify recovery: on a 100 mA input limit the
display load exceeds the input and the battery supplies the remainder.
Controlled re-enable starts at the lowest brightness level and steps up one
level every 5 s while watching loaded VBAT. Input-qualified recovery is allowed
only while the worst-case **total** system input current stays below the
input limit actually set (500 mA or 1.5 A, using the charger's minimum
accuracy) with a PROPOSED 20% margin: the limit covers the whole system load
plus charging. The total includes the matrix through the LED boost, the logic
rail and radio through their regulator, and conversion losses at the lowest
expected system voltage; the brightness cap is derived from that total, not
from the display alone. Use datasheet maxima until power measurements
(validation §4) calibrate the estimate. If VBAT falls below the shed threshold
within 60 s, the matrix is shed again and further attempts back off (60 s,
doubling to 15 min). After three failed attempts the display stays off until
battery-qualified recovery. All numbers are PROPOSED.

These values must still be checked against the selected pack's protection
cutoff and discharge behavior, as listed below.

Before schematic freeze:

- Confirm the switch, divider ratio/tolerances and any filter from the selected
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

The selected pack provides 2500 mAh (about 10 Wh). The runtime design target is
**about eight hours under representative career-fair use**, with no runtime
claim until measured; the larger pack is more energy, not a runtime result.
Agree on the brightness, display/mode mix, audio duty, BLE activity, and
end-of-use condition before running a full-charge to normal-stop test. Use
measured mode currents and usable pack capacity to revise the budget; do not
discharge beyond the cell's allowed limits.

Before wearable use, verify pack identity/protection, charger configuration,
current/thermal margins, low-battery behavior, and protection recovery. Use
current-limited fixtures or a battery simulator for fault tests; do not create
an uncontrolled short on a LiPo. Record source-derived limits and measured
results separately. See [validation.md](validation.md) and
[bringup.md](bringup.md).
