# 0004: BLE configuration and external passive NFC

- Status: Accepted
- Date: 2026-10-04
- Deciders: Project owner

## Context

Users configure the badge from a companion phone app. The contact link must be
readable by NFC even with the badge switched off or its battery empty, and must
match the QR-configured URL. MCU-driven NFC alone does not establish this
unpowered contact-sharing path.

## Options considered

1. Use only the nRF5340's native NFC peripheral: reduces components but ties
   tag behavior to the MCU architecture and does not provide the selected
   independent passive tag storage path.
2. Use BLE for configuration and an external passive I²C tag for NFC: adds a
   tag/coil and synchronization work, but provides persistent passive content.

## Decision

Use BLE through a custom companion app for configuration and a separate passive
NTAG I²C plus-family tag with a PCB coil for contact sharing. The MCU writes the
configured URL as NDEF into tag EEPROM over I²C while powered. The tag retains
that same URL for passive reading after power removal. The exact tag variant
and antenna tuning remain provisional.

Provision protected RF writes with public RF reads, including without badge
power, while retaining MCU/I²C user-EEPROM updates. Factory-default password
protection is disabled; use the policy and part-specific review in
[nfc.md](../nfc.md). No credentials belong in repository content.

## Consequences

The app, firmware configuration, QR renderer, and tag writer share one URL
contract. Validate a newly configured URL through the complete MCU/I²C/NDEF/RF
path, powered and unpowered, against the decoded QR; preprogrammed-tag reads
alone are insufficient. Specify interrupted-update reconciliation and report
errors instead of claiming success with inconsistent URLs. Validate the coil
with battery, magnets, and carrier installed; no RF results exist yet.
RF-write rejection, protection persistence and continued MCU updates are
release checks. Review the final memory map, protection exclusions, irreversible
locks and NXP RF/I²C errata; exact provisioning bytes remain provisional.

## References

- [Architecture](../architecture.md)
- [NFC provisioning and recovery](../nfc.md)
- [NFC update, persistence, and recovery tests](../validation.md#6-nfc)
- [NXP NT3H2111/2211 Rev.3.6](../references.md#component-datasheets)
