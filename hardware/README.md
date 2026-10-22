# Hardware source

KiCad **10.0.6** project for the Rev A badge. The 118 × 76 mm, four-layer board
is in production; bring-up measurements are pending. All assembled components
are on the front. The rear holds the battery, its connector body, and carrier.

Open `smart-led-badge.kicad_pro`. Project libraries use `${KIPRJMOD}` paths;
install the matching KiCad standard libraries as well.

| Path | Contents |
|---|---|
| `smart-led-badge.kicad_sch`, `sheets/` | Root schematic and subsystem sheets |
| `smart-led-badge.kicad_pcb` | Routed Rev A board |
| `smart-led-badge.kicad_dru` | Custom design rules |
| `sym-lib-table`, `fp-lib-table` | Project library tables |
| [libraries/](libraries/README.md) | Symbols, footprints, and clearance-model provenance |
| `mechanical/` | Nine current 3MF print files for the carrier, controls, fit coupon, and PCB dummy |

The PCB dummy omits the rear battery connector. It is a print-fit aid, not a
complete populated-board clearance model. Carrier sizing is accepted; retention,
service life, battery clearance, RF, and thermal checks remain open.

## Board notes

- 1.6 mm board, 3 mm corner radius, JLC04161H-7628 stack-up; tented vias.
- Rows route on F.Cu; column vias connect on In2.Cu. In1.Cu is primarily ground,
  with three low-current links and nearby ground stitching.
- Minimum track/space is 0.127 mm; vias start at 0.45/0.2 mm. Custom rules allow
  the intentional courtyard overlap between adjacent matrix LEDs.
- NFC, antenna, and battery regions have dedicated keep-outs. The rear pack
  area has no assembled parts or exposed test pads.
- Test points, battery polarity, control symbols, and board identity are marked
  on silkscreen. Reference designators are omitted from the printed legend.

## Checks and outputs

```powershell
.\scripts\dev.ps1 hardware
```

Linux: `make hardware`. This runs schematic ERC, compares symbol pin numbers
with footprint pad numbers, and exports PDF/BOM files under `build/hardware/`.
`python scripts/ci/check_hardware.py --self-test` exercises negative cases.
The pin/pad comparison does not verify footprint geometry or pin functions;
those need review against the source drawings. PCB DRC is a separate check.

Gerbers, drill, BOM, placement files, and STEP exports are generated outputs,
kept outside source control. See the [hardware overview](../docs/hardware.md)
and [bring-up checklist](../docs/bringup.md).
