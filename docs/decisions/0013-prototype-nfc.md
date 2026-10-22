# ADR 0013: guarded NFC updates and explicit RF provisioning

Status: Accepted 2026-10-12; RF proof amended by [0014](0014-positive-nfc-provisioning-proof.md), runtime prerequisite by [0017](0017-local-nfc-synchronization.md)

Date: 2026-10-11

## Decision

Separate bounded NDEF encoding/transaction logic from board GPIO, I²C, and timers.
Serialize the shared DK bus. Use STOP-separated guarded frames, write-completion
polling within 5 ms, readback, and bounded retries for the NTAG errata.

Stage URL changes without persisting settings. Commit only after display and
NFC verification; on failure restore the display and attempt guarded tag
rollback. Late completion cannot commit a cancelled change.

The original setup used transient host-generated PWD/PACK, phone RF checks,
and a non-secret UID/profile receipt. Later amendments replace the proof and
remove the receipt from runtime readiness. No irreversible configuration lock
is implemented. Interrupted writes can still require explicit reconciliation
because tag EEPROM and settings are separate devices.
[NFC policy](../nfc.md) · [Setup](../nfc-prototype.md)
