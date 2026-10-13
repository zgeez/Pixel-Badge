# Hardware

KiCad 10.0.6 project for the Rev A badge PCB. The schematic is frozen for
layout (review 2026-10-07); later changes go through a recorded decision. See [docs/hardware.md](../docs/hardware.md) for the architecture.

## Layout

| Path | Contents |
|---|---|
| `smart-led-badge.kicad_pro` | KiCad project |
| `smart-led-badge.kicad_sch` | Root schematic: hierarchy and revision block |
| `smart-led-badge.kicad_pcb` | Board (layout in progress: placement and matrix routing; not for fabrication) |
| `smart-led-badge.kicad_dru` | Custom DRC rules (see Board) |
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
| Symbol `E73-2G4M08S1C`, footprint `Ebyte_E73-2G4M08S1C` | U401 (nRF52840 module). Pin numbers, names and the land pattern from the Ebyte E73-2G4M08S1C specification (pin table; section 3 drawing, bottom view mirrored to the top view): 28 castellated pads at 1.27 mm and 15 inner 0.8 mm pads; castellated lands extend 0.3 mm beyond the 13 × 18 mm body. Antenna keep-out (all layers) at the ceramic-antenna end as a rule area; Ebyte gives no keep-out drawing. Symbol pins reuse the earlier MDBT53 pin locations so the MCU sheet wiring carried over. The footprint uses the project envelope model (below) |
| Symbol `MDBT53-P1M`, footprint `Raytac_MDBT53-P1M` (no longer placed) | KiCad 10.0.6 conversion of Raytac's `MDBT53_230719.lbr` (Footprint & Design Guide 241021); pads checked against the source; Raytac antenna keep-out added as a rule area; pin electrical types set from the Ver.E §2.5 pin table (power pins `power_in`, so ERC checks they are driven). Raytac's 3D model is not redistributed here; the footprint uses the project envelope model (below) |
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
| Footprint `USB_C_Receptacle_HRO_TYPE-C-31-M-12` | Project copy of KiCad's footprint (unchanged pads), so it can carry the project envelope model; KiCad's library has no model for this part |
| Footprint `TestPoint_Pad_D0.8mm` | KiCad `TestPoint_Pad_D1.0mm` with a 0.8 mm pad and the same silk and courtyard margins; used for the rail and GND test points |
| Footprint `L_Murata_DFE201612E` | Murata DFE201612E body and electrodes (spec J(E)TE243A-0006B-01) match DFE201610P, so the pads follow KiCad's `L_Murata_DFE201610P` |

3D models: every assembled part has one. Where KiCad's library has the part or
an identical body, the footprint uses it (pads and fab outline checked to match):
SOT-563 for `Texas_DRL0006A`, Coilcraft XAL4020 for `L_Coilcraft_XGL4020`
(same 4.0 × 4.0 × 2.1 mm body), Murata DFE201610P scaled to 1.2 mm height for
`L_Murata_DFE201612E`, TS-1088-xR020 for `SW_XUNPU_TS-1088`, Knowles
SPH0645LM4H-6 (same 3.5 × 2.65 mm LGA body) for the microphone. The others use
envelope models in `libraries/3dmodels/`, generated as coloured boxes from the
fab outline (the datasheet body size) and the datasheet maximum height, for
clearance and carrier checks only:

| Model | Body (mm) | Source |
|---|---|---|
| `Texas_DLA0010A.step` | 2.0 × 3.0 × 1.0 max | TI drawing 4223750/D (VSON-HR, 1 mm max height) |
| `Texas_RYK0018A.step` | 3.0 × 2.5 × 0.8 max | TI drawing 4226526/A (WQFN-HR, 0.8 mm max height) |
| `NXP_XQFN-8_SOT902-3.step` | 1.6 × 1.6 × 0.5 | NXP SOT902-3 package description |
| `Ebyte_E73-2G4M08S1C.step` | 13.0 × 18.0 × 3.0, antenna end coloured | Ebyte specification section 3 (body and thickness) |
| `Raytac_MDBT53-P1M.step` | 9.3 × 14.3 × 1.6, antenna end coloured | Raytac footprint outline and module height |
| `USB_C_Receptacle_HRO_TYPE-C-31-M-12.step` | 8.94 × 7.3 × 3.26 shell | Footprint outline, HRO height |

Test pads, mounting holes and the PCB coil have no model (nothing is assembled).

## Board

Layout in progress; the values below are the current design, not validated.

- 112 × 74 mm, 3 mm corner radius, 4 layers, 1.6 mm; vias tented on both sides.
  All parts on the front; the rear carries only the battery and the J302 body.
- Design-rule minimums sit above the fabricator's published multilayer
  limits: 0.127 mm track and space, vias from 0.45/0.2 mm, 0.3 mm copper to
  edge. Silkscreen follows its published legend minimums: 0.15 mm line width
  and pad clearance, text from 1.0 mm. Reference designators are not printed.
  The edge-mounted switch SW201 and the module have outlines that run to the
  board edge (5 DRC warnings, clipped by the fabricator).
- Decoupling capacitors sit next to the pins they serve: within about 2–5 mm
  for the regulator, charger and module DC/DC parts. C708 (LED-rail bulk) still
  needs a place near the bottom row switches.
- Net classes set routing defaults: `Power_HI` (charger, boost and battery
  paths) 0.5 mm with 0.6/0.3 mm vias, `Power` (3V3, GND) 0.3 mm, `Row` 0.3 mm,
  `USB` 0.2 mm, `NFC` 0.4 mm, others 0.15 mm; 0.5/0.3 mm vias by default.
  The inductor loops get copper pours rather than wider tracks.
- In1.Cu is a GND plane. It is kept free of tracks under the three switching
  regulators (U201/L201, U202/L202, U302/L301) and the module; elsewhere tracks
  may cross it.
- `smart-led-badge.kicad_dru` waives courtyard overlap between matrix LEDs
  only: at 45° on a 2.0 mm grid their courtyards overlap by design while the
  pads keep ≈ 0.7 mm. Every other courtyard and copper check applies.
- Rule areas: no copper on any layer inside the NFC coil, no pours, tracks or
  vias over the coil, no parts, pads or tracks on the rear over the battery
  (tented vias allowed), and the module's antenna keep-out on every layer.
- Matrix routing: each row is a straight F.Cu track through its 32 anodes;
  each cathode has a short F.Cu stub to a 0.6/0.3 mm via, and the vias of a
  column are joined on In2.Cu.

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
