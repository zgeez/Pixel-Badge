# P3 audio GATT v2

Separate service f3b00101-6e9b-4f4c-a8f8-731a9d46e1c0; State0102, Control0103,
Audio0104 use the same suffix. All require LESC/MITM level4. Configuration stays
v2, with six modes/extension flags15 when P3 is present; old Studio uses four/7.
REC is never a persistent mode.

## Automatic session and State

Audio State v2 requires the normal authenticated LESC bond. Pixel Badge sends
`01 || phone_public[65]` automatically after connecting, ephemeral P-256 SEC1
uncompressed. Only one successful exchange is accepted per connection; a new
exchange requires reconnecting. No physical enrollment or extra code appears.
Invalid points fail closed. RFC5869 HKDF-SHA256:

```
PRK = HMAC-SHA256(connection_nonce[16], ECDH_x[32])
session_key = HMAC-SHA256(PRK, "PB3E" || phone_public || badge_public || 01)
```

Keys are RAM-only, cleared on disconnect, and proved using opcode2. Trust is in
the bonded phone; this no longer authenticates a distinct companion app against
other apps on that phone. See [ADR0016](decisions/0016-simple-audio-controls.md).
Old persistent audio keys/durations are ignored. Existing recordings remain
readable using their unchanged phone storage key.

State125bytes (all integers little-endian):

| Offset | Value |
|---|---|
| 0 | Version2 |
| 1 | Flags: session key1, proof2, REC pending/active8, button challenge16, bench recording enabled32 |
| 2..7 | Reserved, zero |
| 8 | Non-secret result, int32 |
| 12 | Fresh random connection nonce,16bytes |
| 28 | Recording nonce,16bytes |
| 44 | Button challenge,16bytes |
| 60 | Badge public key,65bytes after automatic exchange |

## Control

`opcode:u8 || counter:u32le || value:u16le || tag[16]`. Opcodes2 prove,3 start,
4 stop,5 button accept,7 heartbeat. Opcode6 is removed and refused. Counter strictly increases
per connection; fresh random connection nonce defeats reconnect/reboot replays.
Verify before advancing counter:

```
tag = HMAC-SHA256(session_key, "PB3C" || connection_nonce || command[0:7]
                 || (button_challenge if opcode==5 else empty))[0:16]
```

Each write: offset byte plus<=19message bytes, contiguous, accumulator<=96bytes.
Separate FF write executes/clears. Malformed fragments/disconnect clear staging.
No automatic replay. Start checks proof, subscription, MTU>=108, healthy display
and recording bench gate. Button accept3s, heartbeat1.5s. No elapsed-time cap.

## Audio and storage

Packet105bytes: `01 || sequence:u32le || ciphertext[84] || GCM_tag[16]`.
Sequence starts0, contiguous. Loss/duplicate/bad tag/overflow stops the receiver.
No plaintext or retransmission fallback:

```
recording_key = HMAC-SHA256(session_key, "PB3R" || random_recording_nonce[16])
GCM_nonce = recording_nonce[0:8] || sequence:u32le
AAD = "PB3A" || recording_nonce || packet[0:5]
```

AES-256-GCM, full128-bit tag, fresh recording key. The badge stops before
sequence wrap; the phone storage/WAV boundary is reached much earlier. Badge queues a bounded pair of 10ms notifications per20ms capture block, with
a40ms completion bound for the pair; failure stops. Phone queue<=32packets (320ms), with file/key preparation completed before Start. Plaintext IMA
block: signed16LE initial predictor, index0, reserved0,80packed bytes low-nibble
first. All160samples, including sample0, are encoded relative to the predictor.
Playback nominal16kHz mono; nominal32kHz input has a31-tap anti-alias FIR before
decimation. Before ADPCM, a fixed-point recording processor applies a gentle
30Hz first-order DC/rumble filter, RMS-based automatic gain and a peak limiter.
Initial gain8x, range0.25x-16x (up to24dB boost), RMS target2600 PCM counts
(about-22dBFS). Gain reduction moves halfway toward its target per20ms block;
increase uses1/50 per block. Gain freezes below24RMS counts, without muting
samples. Per-block peak lookahead limits output to+/-30000 before compression;
interpolated gain reduces block-boundary steps. State/scratch reset at capture
start/stop. No voice gate or speech-only suppression. These are implementation
settings, not calibrated acoustic levels. Actual DK clocks remain a measurement gate.

Android wraps a fresh random32-byte file key once using its Keystore AES-GCM
master key before requesting capture. V2 files have a64-byte header:
`"PBA2" || wrapping_IV[12] || encrypted_file_key[32] || tag[16]`.
Header AAD is `"PB3K" || filename`. Packet encryption uses the RAM file key,
not per-packet Keystore operations. Each validated84-byte block becomes112bytes:
`IV[12] || ciphertext[84] || tag[16]`. V2 IV is8zero bytes plus sequence:u32le,
unique under a fresh file key. Chunk AAD is `"PB3F" || filename || sequence:u32le`.
The file key is cleared on stop and unwrapped once per playback/export. Existing
V1 files (no header, directly encrypted with the master key) remain readable;
file length modulo112 distinguishes V1(0) from V2(64), followed by header/tag
validation. Incomplete final writes are truncated to complete authenticated chunks.

Files are private and excluded from backups, bounded by available phone storage
(16MiB reserve) and classic WAV size: at most13,421,772blocks, about37.28hours.
Private storage uses40.32MB/hour plus64bytes per V2 file; WAV uses115.2MB/hour.
Library listing excludes malformed/empty/open files individually and never
clears a connection key or finishes capture on a read-only failure. The phone
does not queue library scans during capture on the shared native I/O executor.
Stop/detach await shared cleanup before a replacement session is installed.
Stop invalidates pending capture setup, checks cancellation after every async
setup step and before a queued Start is sent, and waits for setup before local
file finalization. Resume waits for stop cleanup and restores the authenticated
idle subscription without starting capture. A detached link is not restored.
No five-minute limit. Playback/export authenticate every block. Only explicit
export creates PCM/WAV outside private storage. Secret/plaintext arrays are
cleared on setup/storage failure and stop; none cross Flutter's channel.

Uses [PSA Crypto](https://arm-software.github.io/psa-api/crypto/1.2/), RFC5869,
Java cryptography and [Android Keystore](https://developer.android.com/privacy-and-security/keystore).
Reference vectors test software operations; bond/session security, visibility,
RF throughput, DMA stop timing and rail integration still need bench acceptance.
