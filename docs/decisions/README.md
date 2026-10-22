# Design decisions

These records explain the main design choices and their dates. Later records
identify the decisions they amend; historical choices are not current setup
instructions. Acceptance records a design choice, not a hardware test result.

Use [template.md](template.md) for a new decision. Keep the context, choice,
trade-off, and relevant sources brief.

| ADR | Decision | Status |
|---|---|---|
| [0001](0001-rev-a-product-scope.md) | Rev A product scope and engineering priorities | Accepted; partly superseded by 0011 |
| [0002](0002-nrf5340-module-zephyr-platform.md) | nRF5340 / MDBT53-P1M / NCS-Zephyr platform | Accepted; SoC and module superseded by 0015 |
| [0003](0003-monochrome-display-qr-validation.md) | 32×32 monochrome display and QR-first validation | Accepted |
| [0004](0004-ble-passive-nfc.md) | BLE configuration and external passive NTAG NFC | Accepted |
| [0005](0005-usb-c-charge-and-data.md) | USB-C charging and USB 2.0 data architecture | Accepted; partly superseded by 0007 |
| [0006](0006-rev-a-mechanical-envelope.md) | Rev A PCB and mechanical constraints | Accepted; partly superseded by 0008, 0009, 0010 and 0021 |
| [0007](0007-battery-charger-power-states.md) | 2500 mAh pack, BQ25620 switching charger, USB-powered charging state (amends 0005) | Accepted |
| [0008](0008-landscape-front-buttons.md) | Landscape wear; buttons on the front below the LED field when they fit (amends 0006) | Accepted; partly superseded by 0009 and 0010 |
| [0009](0009-dual-orientation-dual-mounting.md) | Portrait and landscape wear with firmware rotation; magnetic and lanyard mounting (amends 0006 and 0008) | Accepted |
| [0010](0010-single-sided-assembly.md) | Single-sided assembly: all components on the front; rear holds the battery, its connector and the carrier; regional thickness targets (amends 0006 and 0008) | Accepted |
| [0011](0011-visible-voice-recording.md) | Visible voice recording streamed to the phone app; no audio stored on the badge (amends 0001) | Accepted; enrollment/duration amended by 0016 |
| [0012](0012-prototype-foundation.md) | Prototype settings, QR encoder and MCUboot flash layout | Accepted |
| [0013](0013-prototype-nfc.md) | Guarded NFC updates and explicit two-session RF provisioning | Accepted; provisioning proof amended by 0014 |
| [0014](0014-positive-nfc-provisioning-proof.md) | Two fresh positive NFC authentication sessions for the provisioning receipt | Accepted 2026-10-12; provisioning passed, broader acceptance pending |
| [0015](0015-nrf52840-e73-module.md) | Ebyte E73-2G4M08S1C (nRF52840) module for Rev A (amends 0002) | Accepted 2026-10-13 |
| [0016](0016-simple-audio-controls.md) | Record/Stop using the bonded phone, automatic session setup and storage-based recording limits (amends 0011) | Accepted 2026-10-14 |
| [0017](0017-local-nfc-synchronization.md) | NFC synchronization and wired setup without a phone verifier (amends 0013/0014) | Accepted 2026-10-15 |
| [0018](0018-grayscale-studio-recording-view.md) | 8-bit Studio, bounded 32-frame artwork and recording visualizers | Accepted 2026-10-16; hardware acceptance pending |
| [0019](0019-large-artwork-fixed-workspaces.md) | 128-frame flash artwork and fixed app workspaces | Accepted 2026-10-16; physical/port gates pending |
| [0020](0020-separate-firmware-targets.md) | Separate nRF5340 prototype and nRF52840 Rev A applications with shared logic | Accepted 2026-10-17; hardware acceptance pending |
| [0021](0021-rev-a-board-outline.md) | Rev A board outline 118 × 76 mm (amends 0006) | Accepted 2026-10-16 |
