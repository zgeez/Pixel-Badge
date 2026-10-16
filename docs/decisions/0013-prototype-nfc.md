# ADR 0013: guarded NFC updates and explicit RF provisioning

Current amendment: [ADR 0017](0017-local-nfc-synchronization.md) removes the
phone-verifier/receipt prerequisite; guarded synchronization remains required.

Status: **Accepted** by the owner on 2026-10-12 (proposed 2026-10-11).
Provisioning proof partly superseded by [ADR 0014](0014-positive-nfc-provisioning-proof.md)
on 2026-10-12; guarded update behavior remains accepted.
Software tests do not constitute hardware
acceptance of the NTAG protection or errata timing.

## Context

The prototype uses an NTAG I2C plus 1 kB on the DK's shared IMU bus. Its URL
must remain passively readable, protected from unauthenticated RF alteration
and updateable by the MCU. PWD/PACK readback is masked; successful EEPROM ACKs
and register readback alone cannot establish that a password was programmed.

## Decision

Use a portable fixed-buffer NDEF encoder and guarded transaction engine,
with board-specific GPIO/I2C/timer callbacks. A shared mutex protects tag
frames and bring-up IMU transactions. Poll NS_REG against a 5 ms hardware
deadline after each EEPROM write, verify readable bytes, and bound retries.
Compress HTTP(S) case-insensitively and preserve the rest of the URI exactly.

Provision only through a development configuration and explicit host command.
Generate PWD/PACK with the host CSPRNG and pass them transiently over SWD.
An Android USB helper authenticates the generated password/PACK and rejects
the factory password in separately established RF sessions, bound to the tag
UID and a one-use loopback challenge. Operator field removal establishes the
fresh sessions; this remains a bench requirement. Then set the reversible CC
lock pair and save a versioned, non-secret UID/profile receipt in NVS.
No REG_LOCK or irreversible configuration lock is implemented.

Invalidate the receipt before any normal I2C synchronization or provisioning.
Republish it only after clean policy/NDEF verification. RF transitions during
frames or an unreliable FD guard revoke the cached credential proof; a restart
or lost receipt then requires explicit provisioning. An unchanged verified URL
avoids EEPROM writes. Updates clear extra RF locks while retaining CC locks.
Display and NFC completion both gate bench configuration success.
Preview URL changes without persisting settings; commit only after both
verifications pass. On error/timeout restore the previous display state and
attempt guarded tag rollback. Late completion cannot commit a cancelled change.

## Consequences

The receipt is evidence of a trusted local provisioning workflow, not an
anti-cloning credential or authenticated storage. SWD/MCU access is trusted.
Interrupted updates can leave an incomplete external NDEF; errors keep the
tag out of service until recovery. Factory reset erases the receipt but does
not erase external tag EEPROM. BLE configuration belongs to P2.

The DK retains its existing 400 kHz shared bus, unmeasured FD margin and manual
power recovery. The service does not claim Rev A switched-supply support or
measured errata compliance. Bench steps and evidence limits are in
[nfc-prototype.md](../nfc-prototype.md); the part-specific policy and primary
sources are in [nfc.md](../nfc.md).
