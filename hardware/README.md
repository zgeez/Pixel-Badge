# Hardware

KiCad 10.0.6 project for the Rev A badge PCB. The schematic is frozen for
layout (review 2026-10-07); later changes go through a recorded decision. See [docs/hardware.md](../docs/hardware.md) for the architecture.

## Layout

| Path | Contents |
|---|---|
| `smart-led-badge.kicad_pro` | KiCad project |
| `smart-led-badge.kicad_sch` | Root schematic: hierarchy and revision block |
| `sheets/` | One hierarchical sheet per block (USB/charger/battery, power tree, MCU module, debug, matrix columns, matrix rows and safety, LED matrix, IMU, microphone, NFC, user interface, test points) |
| `sym-lib-table`, `fp-lib-table` | Project library tables (paths relative to `${KIPRJMOD}`) |
| `libraries/symbols/` | Project symbol library (`smart-led-badge.kicad_sym`) |
| `libraries/footprints/` | Project footprint library (`smart-led-badge.pretty/`) |
| `libraries/3dmodels/` | 3D models referenced by project footprints |
| `mechanical/` | Mechanical drawings and carrier/mount sources |

Project-specific symbols and footprints go in the project libraries so the
design opens on any machine without local path changes.

| Library item | Source |
|---|---|
| Symbol `MDBT53-P1M`, footprint `Raytac_MDBT53-P1M` | KiCad 10.0.6 conversion of Raytac's `MDBT53_230719.lbr` (Footprint & Design Guide 241021); pads checked against the source; Raytac antenna keep-out added as a rule area; pin electrical types set from the Ver.E §2.5 pin table (power pins `power_in`, so ERC checks they are driven). Raytac's 3D model is not redistributed here |
| Symbol `TPS63802`, footprint `Texas_DLA0010A` | Drawn from TI SLVSEU9D: pins and electrical types from Table 7-1; pads from the DLA0010A land pattern example (drawing 4223750/D 03/2019), body 2.0 × 3.0 mm |
| Symbol `TPS61023`, footprint `Texas_DRL0006A` | Drawn from TI SLVSF14B: pins from the pin-function table; pads from the DRL0006A land pattern example (drawing 4223266/F 11/2024) |
| Symbol `ICM-42688-P` (footprint: KiCad `LGA-14_3x2.5mm_P0.5mm_LayoutBorder3x4y`) | Drawn from TDK DS-000347 Rev 1.2 Table 9; KiCad footprint pad centres (±1.1625, ±0.9125 mm) and numbering checked against §10.2 |
| Symbol `NT3H2111W0FHKH`, footprint `NXP_XQFN-8_SOT902-3` | Drawn from NXP NT3H2111_2211 Rev. 3.6: pins from Table 3; lands from the reflow footprint (Figure 32): 0.40 × 0.22 mm, paste 0.025 mm inside, no land in the 0.5 mm centre area |
| Footprint `Knowles_LGA-5_3.5x2.65mm_Hole0.6mm` | KiCad Knowles LGA-5 (pads and ring checked against the SPH0641LM4H-1 Rev D land pattern) with a 0.6 mm acoustic hole (Knowles: 0.6–1.0 mm), which keeps 0.21 mm to the ground ring for the fabricator's 0.2 mm unplated-hole-to-copper rule |
| Symbol `TLC59283` (footprint: KiCad `Texas_RGE0024C_VQFN-24-1EP_4x4mm_P0.5mm_EP2.1x2.1mm_ThermalVias`) | Drawn from TI SBVS199C Table 4-1 (RGE pin numbers); KiCad footprint is TI's RGE0024C package |
| Symbol `SN74LVC1G123` | Single-unit symbol from TI SCES586E Table 4-1 (DCT SSOP-8); KiCad's version splits the supply pins into a second unit |
| Footprint `SW_XUNPU_TS-1088` | From the XUNPU TS-1088 PCB welding drawing: two 1.05 × 2.00 mm pads, 3.40/5.50 mm |
| Symbol `BQ25620`, footprint `Texas_RYK0018A` | Drawn from TI SLUSEG2D: pins and types from Table 6-1 (BQ25620 column: pin 4 D−, pin 5 D+); pads from the RYK0018A land pattern example (drawing 4226526/A 02/2021), including the four L-shaped corner pads (two pads each), body 3.0 × 2.5 mm |
| Footprint `L_Coilcraft_XGL4020` | Coilcraft Document 1529-3 (rev 02/19/26) recommended land pattern: 0.98 × 3.4 mm pads, 2.37 mm centres; body 4.0 × 4.0 mm, 2.10 mm max |
| Footprint `TI_DQA0010A_USON-10_2.5x1.0mm_P0.5mm` | TPD4E05U06 lands from TI SLVSBO7O, DQA0010A land pattern and stencil examples (drawing 4220328/A 12/2015): 0.565 × 0.20 mm, pins 3/8 0.565 × 0.40 mm, rows 0.835 mm apart, pin 3/8 paste 0.36 mm wide. Replaces KiCad's generic USON-10 (0.55 × 0.30 mm pads, 0.77 mm rows). Keep other nets out from between pins 3 and 8, where the DQA0010B outline has exposed metal; outline from KiCad's USON-10 |
| Footprint `LED_0603_KENTO_KT-0603` | Matrix LED, from the Hubei KENTO KT-0603 suggested soldering pad (LCSC C2290 drawing): 0.70 × 0.70 mm pads, 0.70 mm gap. Pad 1 is the cathode (KENTO numbers it 2), marked by a chamfered pad and a fab line; no silkscreen; courtyard pads and body + 0.25 mm |
| Footprint `L_NFC_Coil_30x25mm_6T` | NFC PCB coil: 30 × 25 mm outer, 6 turns, 0.4 mm track, 0.3 mm gap, 45° corner chamfers so the outer turn clears the board corner; F.Cu with an In1.Cu crossover through two tented 0.5/0.25 mm via pads; net tie between its pads. Calculated 1.67–2.00 µH (NXP AN11276 method), tuned with the parallel capacitor footprints |
| Footprint `TestPoint_Pad_D0.8mm` | KiCad `TestPoint_Pad_D1.0mm` with a 0.8 mm pad and the same silk and courtyard margins; used for the rail and GND test points |
| Footprint `L_Murata_DFE201612E` | Murata DFE201612E body and electrodes (spec J(E)TE243A-0006B-01) match DFE201610P, so the pads follow KiCad's `L_Murata_DFE201610P` |

## Checks

Run ERC on the full hierarchy, check symbol pins against footprint pads, and
export the schematic PDF and BOM to `build/hardware/`:

```powershell
.\scripts\dev.ps1 hardware
```

```sh
make hardware
```

ERC errors and warnings both fail the check. A violation that has been reviewed
and accepted is excluded in KiCad with a comment explaining why; it is then
not counted. While sheets are still being drawn, a label whose other end
belongs on an undrawn sheet is excluded with a "Pending" comment naming that
sheet; the exclusion is removed when the sheet is drawn.

The pin/pad check fails when a placed symbol's pin numbers differ from its
footprint's numbered copper pads (unnumbered pads are mechanical tabs or paste
apertures). It compares pad-number sets only, so it catches a footprint with
missing, extra or differently numbered pads. A footprint with the same pad
numbers but different geometry (for example SOT-353 instead of SOT-23-5)
passes, and pin functions are not checked; footprint geometry and pin
functions are reviewed per part against the datasheet.

`python scripts/ci/check_hardware.py --self-test` confirms that ERC fails on an
injected dangling wire and the pin/pad check fails on a part given a footprint
with too few pads. The script requires KiCad 10.0.6 and
finds `kicad-cli` through `KICAD_CLI`, `PATH`, or the default Windows install
path. The check is local only for now; a CI job will be added once the
schematic has real content.

## Generated outputs

Gerbers, drill files, BOM, placement files, schematic PDFs, and STEP exports are
generated by scripts, CI, or at release time and published as release assets.
They are not committed.

## Rules

- Verify footprint pin numbering and polarity against the manufacturer
  datasheet; never assume it.
- Do not suppress ERC/DRC violations without a documented rationale.
- Keep SWD access and power test points on Rev A.
