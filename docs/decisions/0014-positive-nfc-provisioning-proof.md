# ADR 0014: positive NFC authentication for provisioning

Status: Accepted; optional diagnostics after [0017](0017-local-nfc-synchronization.md)

Date: 2026-10-12

## Decision

Use helper v0.5/proof3 to authenticate a generated non-factory password in
two fresh RF sessions. This replaced the inconclusive factory-password test:
an Android I/O error cannot prove a wrong-password rejection.

Each session checks UID/type, public CC, exact generated PWD/PACK authentication,
and another public CC read. Physically remove the phone between sessions.
Accept only a matching-UID, one-use proof with both successes; errors and
malformed replies never qualify. Remove the RF field before guarded finalization.

Under NXP's one-password model this supports replacement of the factory password;
it does not directly observe factory rejection or establish every write-protection
path. Clear transient credentials on completion, failure, or pause. Keep
unauthenticated NDEF/metadata-write rejection as a separate test.

The receipt no longer gates boot/Apply. This workflow remains available for
diagnostics. [NFC tools](../../scripts/nfc/README.md) ·
[NXP datasheet](https://www.nxp.com/docs/en/data-sheet/NT3H2111_2211.pdf)
