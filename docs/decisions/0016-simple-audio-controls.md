# 0016: Record and stop over the paired Bluetooth connection

Status: Accepted; recording view amended by [0018](0018-grayscale-studio-recording-view.md)

Date: 2026-10-14

## Decision

Use ordinary authenticated Bluetooth pairing as the trust step. Record/Stop
needs no separate enrollment or matching-code confirmation. A P-256/HKDF
exchange prepares RAM-only session keys automatically; replay counters and
per-recording AES-GCM remain. This trusts the bonded phone, not one distinct
app on that phone.

Remove selectable/five-minute duration limits. Stop on explicit request,
low storage, app/link/stream failure, or the classic WAV boundary. Reserve
16 MiB phone storage. Format sizes are about 40.32 MB/hour private and
115.2 MB/hour exported WAV, with about 37.28 hours per WAV file; these are
calculations, not endurance results.

Keep explicit initiation, presentation-before-capture, volatile-only badge
audio, and visibility/fault stops. Firmware/app audio State v2 must match;
old clients fail the version check.
[Audio behavior](../audio-prototype.md) · [Protocol](../audio-protocol.md)
