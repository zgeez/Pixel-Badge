# Bare Pro Micro nRF52840 bench

Date: 2026-10-18. Source: uncommitted `fw/rev-a-nrf52840`, based on main
`58a50197c0b278a0fcc95d849aad2d1132e657eb`. Hardware: my Teyleten Robot
Pro Micro-type nRF52840 board, USB serial identity C62A57C22E5050EF; no external
peripherals. SDK/toolchain: pinned nRF Connect SDK v3.4.1.

## Build and deployment

Separate application `firmware/bench/promicro_nrf52840` builds for
`promicro_nrf52840/nrf52840/uf2`. Linked allocation: 228532 bytes flash and
90149 bytes RAM, including reserved stacks/buffers. This headless application
excludes audio/motion and physical matrix drivers; the complete Rev A
matrix/recording build remains the full-feature capacity reference
(254448 bytes flash / 100869 bytes RAM).

The application occupies 0x26000 through the padded end 0x5dd00. UF2 family,
block sequence/count, address bounds and reset vector were checked. UF2 SHA-256:
`f196193482ceaef61da7a9e4a4c951ccebe636133c4720d0d628b9b2a63050bf`.

The factory application appeared on COM8. A 1200-baud DTR transition entered
the factory serial-only bootloader on COM9 (USB VID239A/PID00B3); no mass-storage
drive appeared. Local adafruit-nrfutil 0.5.3.post16 generated and installed an
application-only package explicitly requiring S140 6.1.1 FWID0x00B6. The
manifest contained no SoftDevice or bootloader update, and the packaged binary
matched the build. Serial DFU reported activation and "Device programmed."
The Zephyr application then appeared on COM10 (VID2FE3/PID0004).
No mass erase, global tool installation or factory-application backup occurred.
Bootloader contents were not read back or hashed.

## Direct observations

USB status reported settings load=0, advancing uptime/frame counters, QR mode,
initial brightness128 and NFC=-19 (no tag). Three samples advanced from
uptime286777/frames13844 to292822/14136 without a restart. A later live status
reported brightness13, matching the app's 5% preset; this alone does not prove
the name value or every app confirmation.

Windows BLE discovery found **Pixel Badge Bench**, advertising service
`f3b00001-6e9b-4f4c-a8f8-731a9d46e1c0`. No PC pairing/bond was created. One
advertisement sample is not an RF range/throughput measurement.

The interactive USB helper displays the pairing code locally. Its optional
JSON contains only whitelisted public counters; no pairing codes are saved.
The bench getter is present in the bench ELF and absent from both production
target ELFs. LESC/MITM and the shared one-bond policy remain enabled.

Manual observation: successful phone pairing and "Applied and confirmed" for name
BENCH and 5% brightness. Direct USB status corroborates brightness13. During
the content check, mode changed to Animation and frame CRC changed while
uptime continued. I subsequently confirmed matching content readback and
disconnect/reconnect without a new pairing code. After closing the USB helper
and unplugging/replugging the board, I confirmed name, 5% brightness
and saved content retained, with reconnect requiring no new code. The scoped
bare-board BLE/app/persistence checks passed by manual observation, corroborated by
the narrower direct USB/advertising observations above. RAM frame rendering is
not evidence of physical display visibility or optical behavior.

## Software checks

- Pro Micro pristine build: passed; nRF5340 recording and safe Rev A regression
  builds after the shared entry-point/BLE changes: passed.
- USB public-status parser tests: 2 passed, including passkey exclusion.
- C/C++ formatting, YAML/workflow lint, repository hygiene and whitespace:
  passed. Hosted CI has not run for these uncommitted changes.
- App source/APK is unchanged by this bench increment. The full shared native
  suite was not repeated; its earlier port results remain separately recorded.

Ignored local evidence: `build/promicro-dfu/flash.log`,
`build/promicro-runtime-check.json`, `build/promicro-scan/advertising.json`,
`build/promicro-public-status.json`, `build/promicro-app-test-status.json`,
`build/promicro-bench-build.log` and the
two `build/promicro-regression-*.log` files.

## Limits

This tests real nRF52840 CPU/BLE/flash operation with simulated display output.
URL/NFC synchronization must fail without a tag, preserving the old URL.
No E73 antenna, Rev A pin/clock/power/charger, physical LED, microphone, IMU,
combined scan/audio/radio load, battery or wearable-use acceptance is implied.
The bench retains factory bootloader reservations and has no MCUboot/secondary
slot; its layout must not be substituted for the Rev A layout.

The new bench application does not implement the factory application's
1200-baud bootloader-reset behavior. Future updates may require physical reset
entry or SWD. See the [bench instructions](../../../firmware/bench/promicro_nrf52840/README.md).
