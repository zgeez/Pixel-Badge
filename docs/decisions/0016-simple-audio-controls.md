# 0016: Record and stop over the paired Bluetooth connection

- Status: Accepted
- Date: 2026-10-14
- Deciders: Project owner

## Context

The owner requests removing the separate audio enrollment action and selectable
recording duration. Normal secure Bluetooth pairing remains the authorization
step. This amends the enrollment and duration requirements in ADR0011.

## Decision

The Audio page exposes Record/Stop, with no matching-code enrollment or duration
selector. After the normal authenticated LESC bond connects, Pixel Badge
prepares an ephemeral encrypted audio session automatically. No separate app
identity or persistent app key is required. Authorization trusts the bonded
phone; the earlier guarantee of excluding other apps on that phone is removed.
An automatic P-256/HKDF session exchange, replay counters and per-recording
AES-GCM remain, without a second physical confirmation. Session keys are RAM
only and cleared on disconnect. Existing private recording encryption and
explicit WAV export remain.

There is no user-selected or five-minute recording timeout. Recordings continue
until Stop, low phone storage, a stream/app/link failure, or the single-file WAV
format boundary. Reserve16MiB of free phone storage and stop before exhausting
it. At the current112-byte chunk/10ms rate, private storage is40.32MB per hour;
exported16kHz mono16-bit PCM is115.2MB per hour. Classic RIFF/WAV's unsigned
32-bit length restricts one recording to13,421,772blocks, about37.28hours. These
are format calculations, not measured battery or sustained recording results.
The badge also refuses sequence wrap. Legacy persisted durations/app keys are
ignored; factory reset clears them with other settings.

REC visibility, explicit user initiation, volatile-only badge audio, disconnect/
unsubscribe/foreground/heartbeat stops, display failure handling and default
release recording gate remain as in ADR0011. No new hardware acceptance is
implied. nRF52840 port remains after prototype firmware.

## Consequences

One Bluetooth pairing establishes trust; reconnecting needs no audio enrollment.
Firmware and app upgrade together to audio State v2. Old clients fail the audio
version check. Normal settings GATT remains v2. Long recordings require a live
foreground app, reliable BLE and battery power throughout. WAV export needs
additional destination space and is always explicit.
