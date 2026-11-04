# Audio protocol v2

A separate GATT service, `f3b00101-6e9b-4f4c-a8f8-731a9d46e1c0`, with State
(`f3b00102`), Control (`f3b00103`) and Audio (`f3b00104`) on the same suffix.
All three need LE Secure Connections at security level 4. Recording is never
a saved mode. Integers are little-endian.

## Session

After connecting, the app writes `01 || phone_public[65]` to State: an
ephemeral P-256 key, SEC1 uncompressed. One exchange per connection; a new
one needs a reconnect. Invalid points fail. Keys are derived with HKDF-SHA256
(RFC 5869):

```
PRK = HMAC-SHA256(connection_nonce[16], ECDH_x[32])
session_key = HMAC-SHA256(PRK, "PB3E" || phone_public || badge_public || 01)
```

Keys live in RAM and are cleared on disconnect. The app proves it holds the
key with opcode 2. Trust rests on the bonded phone; it doesn't tell this app
apart from other apps on the same phone ([ADR 0016](decisions/0016-simple-audio-controls.md)).

State is 125 bytes:

| Offset | Content |
|---|---|
| 0 | Version 2 |
| 1 | Flags: session key 1, proof 2, recording pending or active 8, button challenge 16, recording built in 32 |
| 2–7 | Zero |
| 8 | Result, int32, not secret |
| 12 | Connection nonce, 16 bytes, fresh per connection |
| 28 | Recording nonce, 16 bytes |
| 44 | Button challenge, 16 bytes |
| 60 | Badge public key, 65 bytes, after the exchange |

## Control

A command is `opcode:u8 || counter:u32 || value:u16 || tag[16]`. Opcodes:
prove 2, start 3, stop 4, accept button 5, heartbeat 7 (6 is retired). The
counter must increase within a connection, and the fresh connection nonce
stops replays across reconnects. The tag is checked before the counter moves:

```
tag = HMAC-SHA256(session_key, "PB3C" || connection_nonce || command[0:7]
                 || (button_challenge if opcode==5 else empty))[0:16]
```

Commands are written in fragments: an offset byte plus up to 19 bytes,
contiguous, 96 bytes at most, then a single `FF` write runs it. Bad fragments
or a disconnect clear it. Start needs the proof, a subscription, MTU ≥ 108, a
healthy display and a recording-enabled build. A button accept times out after
3 s and heartbeats after 1.5 s. There's no overall time limit.

## Audio packets

Each notification is 105 bytes: `01 || sequence:u32 || ciphertext[84] || tag[16]`.
Sequence starts at 0 with no gaps. A lost, repeated or bad packet stops the
receiver; there's no plaintext fallback or retransmission.

```
recording_key = HMAC-SHA256(session_key, "PB3R" || random_recording_nonce[16])
GCM_nonce = recording_nonce[0:8] || sequence:u32le
AAD = "PB3A" || recording_nonce || packet[0:5]
```

AES-256-GCM with the full 128-bit tag and a fresh key per recording. The badge
stops before the sequence wraps.

The 84 plaintext bytes are one 10 ms IMA ADPCM block at 16 kHz mono: initial
predictor (int16), index, a reserved 0, then 80 packed bytes, low nibble first.
All 160 samples are coded relative to the predictor. The badge sends two
blocks per 20 ms of capture, within 40 ms, or stops.

Before encoding, a fixed-point stage applies a 30 Hz high-pass, automatic
gain (start 8×, range 0.25–16×, target RMS 2600 counts, about −22 dBFS) and a
limiter at ±30000. Gain drops quickly and rises slowly, and freezes in near
silence instead of muting. These are tuning values, not calibrated levels.

## Phone storage

Android makes a random 32-byte file key per recording and wraps it with an
Android Keystore AES-GCM key. A V2 file starts with a 64-byte header,
`"PBA2" || wrapping_IV[12] || encrypted_file_key[32] || tag[16]`, with AAD
`"PB3K" || filename`. Each 84-byte block is stored as
`IV[12] || ciphertext[84] || tag[16]`, where IV is 8 zero bytes plus the
sequence, and AAD is `"PB3F" || filename || sequence:u32le`. V1 files (no
header, encrypted with the Keystore key directly) still open; file length
mod 112 tells them apart. An unfinished last write is trimmed to whole chunks.

Files are private, excluded from backups, and capped by free space (16 MiB
kept free) and the WAV size limit, about 37 hours. Storage costs about 40 MB
per hour; WAV export about 115 MB per hour. Playback and export check every
block, and only an explicit export writes plain audio outside private
storage. Secrets are cleared on stop and on errors, and never cross the
Flutter channel.

Built on [PSA Crypto](https://arm-software.github.io/psa-api/crypto/1.2/),
RFC 5869, Java cryptography and [Android Keystore](https://developer.android.com/privacy-and-security/keystore).
Test vectors cover the crypto. Bond security, the recording view, throughput
and stop timing still need checking on hardware.
