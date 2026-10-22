# ADR 0017: NFC synchronization without a phone verifier

Status: Accepted

Date: 2026-10-15

## Decision

Let boot, Apply link, and Sync again establish readiness from guarded local
policy/NDEF checks. Do not require saved RF proof or two phone taps. A previous
false readiness flag must not prevent a recovery request.

Keep public passive reads, protected RF writes, and MCU updates. Normal boot/
Apply/sync never change credentials. Pin the first accepted UID for the boot;
explicit wired setup may replace it. Factory reset triggers fresh local checks,
not phone verification.

Fresh tags use trusted-host `configure`: generate transient credentials, stage
them over SWD, then finalize policy/URL with matching UID and existing deadlines.
No helper or irreversible lock is needed. QR submission, guarded tag readback,
settings persistence, rollback, and error handling still gate success.

Masked PWD/PACK cannot be authenticated by I²C readback. Local synchronization
does not prove RF authentication or write rejection; those remain physical tests.
[NFC setup](../nfc-prototype.md) · [Protection policy](../nfc.md)
