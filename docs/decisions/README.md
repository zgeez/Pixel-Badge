# Architecture decision records

Significant architecture, hardware, and tooling decisions are recorded here as
ADRs once the project owner formally accepts them. Provisional directions live
in the other documents in `docs/` and are marked TBD or provisional.

## Process

1. Copy [template.md](template.md) to `NNNN-short-title.md` (next number,
   zero-padded).
2. Fill it in with status `Proposed` and open a pull request.
3. Change the status to `Accepted` when approved and merged.
4. Do not rewrite accepted ADRs; supersede them with a new ADR and update the
   old one's status.

## Index

No ADRs have been accepted yet.

The records below capture the owner-selected Rev A scope and architecture.
They remain `Proposed` until this pull request is approved and merged, following
the process above. Acceptance records a design choice, not measured validation;
prototype-dependent values remain provisional within each decision.

| ADR | Decision | Status |
|---|---|---|
| [0001](0001-rev-a-product-scope.md) | Rev A product scope and engineering priorities | Proposed |
| [0002](0002-nrf5340-module-zephyr-platform.md) | nRF5340 / MDBT53-P1M / NCS-Zephyr platform | Proposed |
| [0003](0003-monochrome-display-qr-validation.md) | 32×32 monochrome display and QR-first validation | Proposed |
| [0004](0004-ble-passive-nfc.md) | BLE configuration and external passive NTAG NFC | Proposed |
| [0005](0005-usb-c-charge-and-data.md) | USB-C charging and USB 2.0 data architecture | Proposed |
| [0006](0006-rev-a-mechanical-envelope.md) | Rev A PCB and mechanical constraints | Proposed |
