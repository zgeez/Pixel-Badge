# nRF5340 prototype wiring

The DK fixture uses a 64 × 64 P2 HUB75 panel through SN74AHCT245 buffers, plus
an external NTAG, IMU, and PDM microphone. Firmware displays a 32 × 32 window.
These pins belong to the DK fixture; Rev A has a separate pin map.
The [devicetree overlay](../firmware/nrf5340_prototype/boards/nrf5340dk_nrf5340_cpuapp.overlay)
is the build's source of truth.

| Signal | DK pin |
|---|---|
| HUB75 R1 / G1 / B1 | P1.11 / P1.12 / P1.13 |
| HUB75 CLK / LAT / OE | P1.14 / P1.15 / P1.08 |
| HUB75 A / B / C / D / E | P0.04 / P0.05 / P0.06 / P0.07 / P0.25 |
| Shared IMU/NTAG SCL / SDA | P1.02 / P1.03 |
| IMU INT1 | P1.04 |
| NTAG field detect | P1.06 |
| PDM CLK / DATA | P1.07 / P1.09 |
| Timing bench SCK / MOSI / MISO | P0.08 / P0.09 / P0.10 |
| Timing bench LAT / BLANK / frame marker | P0.11 / P0.12 / P0.26 |

Use the panel's rated external supply and a common ground. Keep the level
translators between the DK GPIO and panel inputs; the fixture does not power
the panel from a GPIO or the DK logic supply.

Leave DK Buttons 3 and 4 unpressed: their pins carry matrix SPI signals.
Close terminals on the DK's J-Link COM ports and use RTT. Opening a terminal
can make the interface MCU drive pins shared with the bench; power-cycle the
DK if a terminal was opened. SW6 in nRF ONLY disconnects that interface MCU,
including SWD, so flashing and RTT are unavailable in that position.

The external panel has no power-good feedback to the MCU. Advancing GPIO scan
counters cannot prove the panel is lit. Check actual visibility separately.
