# ADR 0017: NFC synchronization without a phone verifier

Status: **Accepted** by owner instruction on 2026-10-15: remove the NFC verifier
requirement. Supersedes the runtime/provisioning prerequisite in ADRs 0013/0014.

## Decision

The app may request link Apply or Sync again without a previous NFC-ready flag.
Firmware establishes readiness from guarded I²C protection-policy and NDEF URL
readback, without a saved RF-proof receipt or two phone authentication taps.
The existing status bit means synchronized local state, not an RF credential
test. A failed Apply is never presented as confirmed.

Keep the current protected profile: public passive reads, password-protected
RF writes and MCU/I²C updates. Existing credentials are not changed by boot,
URL Apply or Sync again. Readable policy checks still reject an unexpected
address or configuration. Pin the first accepted UID for the current boot;
explicit wired setup may replace it. A restart repeats the checks, including
after a badge factory reset. Old NVS RF receipts are ignored, never promoted
to a new security claim.

For a fresh/misconfigured tag, explicit trusted-host `badge_nfc.py configure`
generates transient non-factory credentials, stages them in the debug mailbox,
and finalizes with matching UID and the existing deadline/field guards. It
requires no phone, Android helper or Enter prompt. No credentials are committed
or logged; transient buffers are wiped on success and failure. No irreversible
configuration locks are added. Optional historical `provision` RF diagnostics
remain available; they do not gate the product flow.

QR and NFC continue sharing one URL. Display submission, guarded tag update,
NDEF readback and settings persistence still gate completion. Existing rollback,
field-detect guards, EEPROM deadlines, secure BLE bonding and error handling
remain in force. A reader-field interruption fails the current operation;
a later explicit retry can reconcile without another verifier session.

## Evidence limits

PWD/PACK are masked on I²C reads. Local policy/NDEF checks cannot prove hidden
credentials were programmed correctly or that unauthenticated RF writes are
rejected. The owner removes that RF proof as an operational prerequisite;
REQ-N8 protection behavior and physical bench/release tests remain required.
Do not label local synchronization as RF authentication or close hardware
protection/timing risks based on it.
