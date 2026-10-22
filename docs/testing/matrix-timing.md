# Matrix scan timing bench

Standalone nRF5340 DK bench, without matrix/driver ICs. Its timing does not
describe the current grayscale driver or validate Rev A.

`firmware/nrf5340_prototype/bench/matrix_timing` drives the matrix scan signals with the planned
hardware timing, without a matrix attached, so they can be checked on a logic
analyzer. TIMER1 times each 160 µs row slot and drives SPIM4 and the LAT and
BLANK pins through DPPI and GPIOTE. TIMER2 counts 32 rows and then stops TIMER1.
The CPU only restarts each frame from the TIMER2 interrupt, so a late interrupt
leaves the matrix blanked instead of a row on.

| Time in slot | Event |
|---|---|
| 1 µs | LAT high: latches the row shifted during the previous slot |
| 2 µs | LAT low; SPIM4 starts shifting the next row (8 bytes at 8 MHz) |
| 4 µs | BLANK low: row on |
| 159 µs | BLANK high: row off |
| 160 µs | Next slot |

Each row is 32 row-select bits (the row's bit is 0, which turns its PFET on)
followed by 32 column bits (`A5`, row number, `5A`, inverted row number).
RTT logs the frame rate once a second (about 195 frames/s).

Measured results, with the setup and raw data, are in
[validation.md](../validation.md#result-scan-timing-on-the-nrf5340-dk-bench-no-matrix).

```powershell
.\scripts\dev.ps1 build -App firmware/nrf5340_prototype/bench/matrix_timing -BuildDir build/matrix_timing
```

```sh
APP_DIR="$PWD/firmware/nrf5340_prototype/bench/matrix_timing" BUILD_DIR=build/matrix_timing make build
```

Analyzer channels: SCK P0.08, MOSI P0.09, LAT P0.11, BLANK P0.12, frame
marker P0.26 (high while the frame interrupt runs), and GND. A 24 MS/s
analyzer decodes the 8 MHz shift (three samples per clock) and times edges to
about ±42 ns, but it distorts the SCK duty cycle; use a faster one to check
SCK itself. Leave Buttons 3 and 4 unpressed and keep terminals off
the DK's COM ports (see [prototype wiring](../prototype-wiring.md)).
