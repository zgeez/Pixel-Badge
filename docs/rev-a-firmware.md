# Rev A nRF52840 firmware port

Status: first software port increment; hardware validation pending. The owner
selected E73/nRF52840 in [ADR0015](decisions/0015-nrf52840-e73-module.md) and
requested separate prototype/Rev A applications on 2026-10-17. Remaining final
prototype physical checks are deferred to board bring-up, never recorded as
passes. No release or wearable-use acceptance follows from a successful build.

## Source and configuration

`firmware/nrf5340_prototype` preserves the DK application, radio sysbuild,
HUB75 backend and legacy custom-board compile checks. `firmware/rev_a_nrf52840`
owns the real E73 board and single-core application/controller build.
`firmware/common` holds the shared app/services, codec/settings formats and
project drivers; `firmware/tests` remains shared. Build commands and variants
are in the [target README](../firmware/rev_a_nrf52840/README.md).

The nRF52840 target retains authenticated LESC BLE configuration, synchronized
QR/NFC URL transactions, flash-backed artwork, text, motion and audio service
code. Source compatibility and linking are established; concurrent timing and
physical behavior on this chip are not yet established.

## Pins and peripherals

The board definition follows the E73 schematic, not the superseded MDBT53 map.
`firmware/rev_a_nrf52840/pinmap.json` records all 43 module pads. The compiled-DT
checker compares actual GPIO/polarity, PSEL, buses, ADC channels, UICR and flash
against that contract; optional KiCad XML compares the contract with U401 nets.

| Resource | Allocation |
|---|---|
| Matrix | SPIM3 at 8 MHz; P0.12 SCK, P0.04 MOSI, P0.08 MISO; P1.09 LAT |
| Matrix control | P0.05 BLANK, P0.06 ARM, P0.26 SAFE, P0.07 LED_EN; TIMER3, one GPIOTE task, two PPI connections |
| IMU and charger | TWIM0 at 400 kHz, P0.15/P0.17; IMU interrupt P0.09 |
| NTAG | TWIM1 at 100 kHz, P0.22/P0.24; P1.00 VCC, P1.02 FD |
| Microphone | PDM, P0.20 CLK/P0.13 DIN |
| Sense inputs | SAADC AIN6 VBAT, AIN0 CC1, AIN1 CC2, AIN5 VLED |
| Clock/reset | External 32.768 kHz crystal/caps; reset and NFCT-as-GPIO UICR settings |

All 29 available GPIO are allocated. There are no UART/spare pads or IMU SPI
fallback. SWD/RTT are the debug path. REG0 is disabled for VDDH=VDD operation;
REG1 retains LDO mode until module DC/DC support is verified. Power draw is not
measured. The Bluetooth SDK retains its own radio timing resources.

```powershell
C:/ncs/toolchains/4f5b6ad6dd/opt/bin/python.exe scripts/ci/check_reva_contract.py --zephyr-base C:/ncs/v3.4.1/zephyr --build-dir build/rev_a_nrf52840 --self-test
```

Only use trusted locally generated `edt.pickle` files. Add `--netlist path.xml`
to check a KiCad schematic XML export too. The negative self-check must reject
an incorrect latch GPIO and overlapping flash.

## Display backend

The new backend shifts an 8-byte row/column command through the two TLC59283s
and four HC595s using the SDK SPI/DMA driver in a dedicated thread. Pure packing
tests cover all 32 rows/eight planes, active-low one-hot row selection, columns
and invalid indices. Two 1024-byte framebuffers support frame-boundary swaps.

Both binary and grayscale use eight weighted bitplanes. A 16 MHz TIMER3 and
PPI/GPIOTE bracket the BLANK pulse independently of thread/IRQ scheduling; the
IRQ only wakes the worker after the one-shot stops. Delayed workers extend the
blank interval rather than the illuminated pulse. ARM stays low while VLED
settles, a complete row shifts before LAT, and SAFE is checked before light.
Blank/recovery epochs discard an old SPI row. SPI errors or timeout blank and
disarm the matrix; the shared bounded recovery loop can retry initialization.
After a failed frame, the next retry blanks and reinitializes the display,
then requires scan progress before declaring recovery. Successful configuration
without scan progress does not reset the existing failure/reboot deadline.

The generic `badge_display_wait_visible` hook replaces the HUB75-specific name
for either backend. Recording still requires a submitted visible frame. The
new backend is disarmed by default; an explicit matrix bench fragment enables
it. The intentional disarmed result is excluded from watchdog failure tracking,
while physical display readiness remains false. BLE/NFC service startup can
continue, and 30 healthy seconds clear the retained watchdog-reset counter,
without permitting invisible pairing codes or capture; other driver
errors retain the recovery/restart policy. This implementation does not establish
refresh rate, optical intensity
steps, row/column orientation, current limits or monostable protection.

## Flash and RAM

Both app and controller fit a 320 KiB primary firmware slot. Keep a matching
secondary slot rather than trading away update capacity. No OTA transport is
implemented; development MCUboot signing keys are not release keys.

| Region | Start | Size |
|---|---|---|
| MCUboot | `0x00000` |64KiB |
| Primary, including 512-byte pad | `0x10000` |320KiB |
| Secondary | `0x60000` |320KiB |
| Two artwork banks | `0xb0000` |264KiB |
| Reserve | `0xf2000` |24KiB |
| Settings/bonds | `0xf8000` |32KiB |

Total: 1 MiB. Artwork/settings offsets and formats match the prototype, but
cross-chip bond or settings migration is not assumed. See the dated validation
record for current link allocations. Static RAM includes reserved thread
stacks/buffers; free allocation is not a measured stack or timing margin.

## nRF5340 to nRF52840 trade-offs

Nordic specifies two M33 cores on [nRF5340](https://www.nordicsemi.com/Products/nRF5340):
an application core up to 128 MHz and network core 64 MHz, with 512 KiB + 64 KiB RAM and
1 MiB + 256 KiB flash. [nRF52840](https://www.nordicsemi.com/Products/nRF52840) has a
single 64 MHz M4F, 256 KiB RAM and 1 MiB flash. The prototype used 64 MHz for its app,
so our current app clock is not halved; separate radio capacity and 128 MHz
upgrade headroom are lost.

| Loss or change | Port response and remaining limit |
|---|---|
| Radio and app share a core | SDK controller priorities, DMA, short ISR, bounded FFT/fluid work and no simultaneous artwork upload/recording. Combined-load deadlines still need measurement. |
| Half the app RAM; no separate radio RAM | Flash-backed animations, bounded frame/capture buffers; measure the combined link allocation and stack high-water usage at bring-up. |
| Controller shares 1 MiB flash | Combined image fits 320 KiB; preserve two firmware slots, 128-frame capacity and artwork/settings banks. Future features must stay within the slot. |
| Different peripheral routing/PPI | New SPIM3/TIMER3/PPI driver and actual E73 pin contract. nRF5340 timing evidence does not validate it. |
| Lower microphone sample-rate capability | Native 16 kHz/1.28 MHz capture on Rev A; bypass 32-to-16 kHz decimation. Phone packet/playback format stays 16 kHz. FFT bands now cover 0-8 kHz rather than 0-16 kHz. |
| No M33 TrustZone isolation | Retain authenticated BLE, volatile audio keys, app encryption and MCUboot. Software cannot recreate TrustZone; previous firmware did not use TF-M isolation. |
| Selected module exposes fewer pins | All 29 GPIO used; SWD/RTT replaces UART, no spare pins or IMU SPI fallback. This is a board/module constraint, not just a CPU difference. |

Both radios support 1M/2M/Coded BLE PHYs; the switch does not inherently halve
BLE speed. GATT upload speed also depends on phone negotiation, connection
intervals, acknowledgements and flash programming. Our audio is encrypted GATT
streaming, not standard LE Audio. Do not infer newer Bluetooth features from a
version label alone. See Nordic's [radio specification](https://docs.nordicsemi.com/r/bundle/ps_nrf52840/page/radio.html)
and [security comparison](https://www.nordicsemi.com/Products/Technologies/Security/Features).
The [nRF52840 PDM specification](https://docs.nordicsemi.com/r/bundle/ps_nrf52840/page/pdm.html)
and [Knowles microphone specification](https://www.knowles.com/docs/default-source/default-document-library/sph0641lm4h-1_morello_datasheet.pdf)
support the 16 kHz/1.28 MHz design; actual gain/noise and acoustic path remain unmeasured.

## Bare nRF52840 development-board bench

The owner provided a Teyleten Robot Pro Micro-type board on2026-10-18. The
separate [headless bench](../firmware/bench/promicro_nrf52840/README.md) uses the
SDK `promicro_nrf52840/nrf52840/uf2` target and factory bootloader, with shared
BLE/settings/rendering and a USB pairing-code display. No passkey is added to
RTT or BLE readback, and the getter is compiled only in this bench target.
Missing NFC reports-ENODEV; microphone/motion capabilities are disabled.
This enables real SoC/BLE/flash/app tests before Rev A arrives, while preserving
the full Rev A build as the capacity reference and hardware acceptance target.

## Remaining implementation and acceptance

Next increment: BQ25620 initialization/current/temperature policy, USB-C/BC1.2
and enumeration policy, switched battery/rail sensing, invalid/stale-sample
handling, load shedding, charging-only/off state and low-power transitions.
The diagnostic image checks IMU/interrupts, guarded NTAG reads and PDM levels,
and the power path (`common/src/app/power_diag.cpp`):
- reset cause and the 32.768 kHz clock source;
- the switched VBAT divider (sampled at least 25 ms after enable), VLED sense
  and CC1/CC2 sense through the SAADC;
- BQ25620 identity, faults, TS zone, D+/D- result and its ADC (VBUS, IBUS,
  VBAT, IBAT, VSYS, TS, TDIE);
- button and power-switch edges and the charger's latched event flags, with
  a status-LED heartbeat.

It samples CC every 20 ms and applies the documented input-limit policy
within tSinkAdj: 1.5 A for a 1.5/3.0 A CC advertisement or a BC1.2 DCP/HVDCP
port, otherwise 100 mA. This image has no USB stack and never enumerates, so a
CDP port gets 100 mA (an unenumerated CDP reverts to SDP after 2.5 s,
SLUSEG2D §8.3.3.3). A failed CC, status or input-limit access drops CHG_EN at
once. It sets `TS_TH4_TH5_TH6 = 011` and keeps the charger I2C watchdog
alive. Charging stays off unless the image is built with
`diagnostics-charge.conf`; even then CHG_EN goes high only while:
- VBUS is present;
- the charger reports no fault and TS is normal;
- the thresholds and ICHG (480 mA) read back as set, the input limit matches
  the policy, and VREG is 4200 mV;
- the charger and SAADC agree on a pack voltage in 3.0-4.25 V;
- a pack-presence probe less than 30 s old passed.

A charger regulates BAT even with no pack, so VBAT in range does not prove a
pack is there. The probe pauses charging, turns on the charger's ~30 mA forced
discharge (`FORCE_IBATDIS`) for 60 ms, and requires the divider to still read
at least 3.0 V. It runs before the first enable, every 30 s while charging,
and every 30 s while a missing pack holds charging off. Its behaviour with and
without a pack is confirmed at bring-up before the first unattended charge.

Register fields come from SLUSEG2D §8.6 and are unit-tested
(`firmware/tests/power`).

The diagnostics image also checks the matrix with its outputs disabled
(`common/src/app/matrix_diag.cpp`):
- MTX_SAFE_SENSE must read high while ARM is low.
- Five non-repeating 32-bit patterns are shifted through the two TLC59283
  registers at 1 MHz and read back on MTX_MISO. They must return at one
  consistent 31-33 bit offset; a stuck MISO or a broken chain fails.
- No LAT pulse is sent. With VLED off, the translator isolates the HC595
  chain, which cannot be read back; check it on a logic analyzer.

`matrix-stepper.conf` adds the button-stepped matrix bench for a
current-limited supply. MODE steps through:
1. rows scanning with zero column current
2. single LEDs at brightness 1 (top-left, bottom-right)
3. row and column walks
4. orientation marker, checkerboard and inverse
5. all on at brightness 8, 32 and 128

ACTION blanks and resumes at any time. The chain logic is unit-tested
(`firmware/tests/matrix_chain`). The product charger policy (USB
enumeration, load shedding, power states) remains.
NFC VCC is enabled on first transport preparation and retained; dynamic power
cycling and the related recovery policy remain to implement. The wired setup
helper still defaults to the nRF5340 debug target; explicit nRF52840 SWD target
selection/setup packaging is follow-up work before personalizing a Rev A tag.

Physical bring-up must establish clocks, pin/chain mapping, disabled-output
states, timing/refresh/ghosting, maximum 128-frame uploads, optical QR/grayscale,
BLE reconnect/throughput/security, audio gain/visualizer visibility/endurance,
IMU orientation/fluid, passive NFC and combined load. Battery protection,
charging/thermal/USB and fault shedding are prerequisites to wearable use.
No numerical hardware pass or risk closure is inferred from host tests/builds.
