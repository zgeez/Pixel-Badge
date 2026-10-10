# Raw data: matrix scan timing on the nRF5340 DK (2026-10-09)

Result record: [validation.md](../../../docs/validation.md#result-scan-timing-on-the-nrf5340-dk-bench-no-matrix).

| File | Content | SHA-256 |
|---|---|---|
| `halt-capture.sr` | sigrok session, 24 MS/s, 200 M samples (8.33 s): normal scanning, a 2 s CPU halt, and the restart. Channels D0 SCK, D1 MOSI, D2 LAT, D3 BLANK, D4 frame marker. | `ab88694f5ac35481bbd1c15a8f8e3362f8eae83e708c4c5b88da93696b42129b` |
| `shift.png` | Screenshot, separate capture: one row shift and its latch (row 14) | `e5377116d90130d6dc3730ea6959de672380d550925b13d3ab3070d8fb0524fa` |
| `row-slot.png` | Screenshot, separate capture: two row slots with cursors | `955f13ba91c16224e1a7832dbc4c5c16cad50e0557332d8cd6fa550c1e8352d2` |
| `frame-boundary.png` | Screenshot, separate capture: frame restart around the frame marker | `5822d3a46ec665851540024274859d4caaa2cd596b2b46a3b5faf44f764e13f9` |
| `halt.png` | Screenshot of `halt-capture.sr` | `c9f9078283b5cdd76f278714583ea794d1c98b4534dc4b6dbf51391ffb2c17e6` |
| `analyze.py` | Measures the timing in a `.sr` session | — |
| `analysis.txt` | Output of `analyze.py halt-capture.sr` | — |

The sessions behind the first three screenshots were not saved; every value in
the record comes from `halt-capture.sr`, which covers the same signals.

Open `halt-capture.sr` in PulseView, or reproduce `analysis.txt` with a Python
that has numpy (the nRF Connect SDK toolchain's Python does):

```sh
python analyze.py halt-capture.sr
```
