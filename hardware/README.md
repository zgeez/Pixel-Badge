# Hardware

The KiCad 10.0.6 project for the Rev A badge: a 118 × 76 mm, four-layer board
with every assembled part on the front. Boards are built and running
([bring-up record](../validation/results/2026-11-03-rev-a-bringup/README.md)).

Open `smart-led-badge.kicad_pro`. Project libraries use `${KIPRJMOD}` paths,
and you'll also need KiCad's standard libraries.

| Path | Contents |
|---|---|
| `smart-led-badge.kicad_sch`, `sheets/` | Schematic |
| `smart-led-badge.kicad_pcb` | Routed board |
| `smart-led-badge.kicad_dru` | Custom design rules |
| [libraries/](libraries/README.md) | Symbols, footprints and where they came from |
| `mechanical/` | 3MF print files, below |

## Board

- 1.6 mm, 3 mm corner radius, JLC04161H-7628 stack-up, tented vias.
- Rows route on F.Cu and columns via to In2.Cu. In1.Cu is ground apart from
  three short links.
- 0.127 mm minimum track and space, 0.45/0.2 mm vias. The custom rules allow
  the matrix LEDs' courtyards to touch.
- Keep-outs for the NFC coil, the antenna and the battery. Nothing is
  assembled or exposed under the pack.
- Test points, battery polarity and controls are labelled on silkscreen.
  Reference designators aren't printed.

## Print files

| File | Part |
|---|---|
| `rear-carrier.3mf`, `front-face.3mf` | The two halves of the carrier |
| `matrix-pixel-grid.3mf`, `matrix-diffuser.3mf` | Black 2 mm grid and white diffuser over the matrix |
| `mode-button-cap.3mf`, `action-button-cap.3mf`, `button-plunger.3mf` | Button parts |
| `magnetic-backing.3mf`, `magnet-retaining-cap.3mf` | Magnetic mount |
| `m2-thread-coupon.3mf` | Test print for the M2 threads |
| `pcb-no-rear-jst.3mf` | Board dummy for fit checks (without the rear connector) |

The diffuser is what makes the QR code readable for phones. Fit, latch
retention and wear still need checking on prints.

## Checks

```powershell
.\scripts\dev.ps1 hardware
```

On Linux, `make hardware`. It runs ERC, checks that symbol pin numbers match
footprint pads, and exports PDF and BOM files to `build/hardware/`. It
doesn't check footprint geometry or pin functions, and DRC is separate.
Gerbers, drill, placement and STEP files are generated and kept out of the
repository.

[Hardware overview](../docs/hardware.md), [bring-up checklist](../docs/bringup.md).
