# BLE protocol v2

The GATT interface between the app and the badge. Recording has its own
[audio protocol](audio-protocol.md). There's no firmware-update command.
All UUIDs end in `-6e9b-4f4c-a8f8-731a9d46e1c0`; integers are little-endian.

| UUID | Name | Access | Content |
|---|---|---|---|
| f3b00001 | Service | | |
| f3b00002 | Info | Read | 8 bytes, below |
| f3b00003 | Name | Read, write | 1–32 printable ASCII |
| f3b00004 | URL | Read, write | 1–25 bytes of HTTP(S), must fit QR Version 1 |
| f3b00005 | Brightness | Read, write | 13, 64, 128 or 255 (16 accepted as 13) |
| f3b00006 | Mode | Read, write | QR 0, text 1, drawing 2, animation 3, level 4, spectrum 5, fluid 6 |
| f3b00007 | Apply | Write with response | `[2, request_id:u32, field:u8]` |
| f3b00008 | Status | Read, notify | 12 bytes, below |
| f3b00009 | Text | Read, write | 6–102 bytes |
| f3b0000a | Artwork | Read, write | Up to 131,718 bytes, paged |
| f3b0000b | NFC sync | Read, write | `[1, ready, busy, result:i32]` / stage `[1]` |
| f3b0000c | Motion | Read, write | 8 bytes |
| f3b0000d | Motion status | Read | 16 bytes |
| f3b0000e | Battery | Read | 6 bytes |

Info is `[version=2, url_max=25, name_max=32, brightness_count=4, modes, flags, max_frames=128, extensions]`.
`modes` is 4, 6 (with audio) or 7 (with audio and motion). Extension bits:
text 1, artwork 2, NFC resync 4, audio 8, motion 16, grayscale 32, large
artwork 64, editor 128. Current firmware reports 231, 239 or 255.

## Security

Info is public. Everything else, including notifications, needs LE Secure
Connections at security level 4. Just Works and legacy pairing are refused.

Pairing only opens on a long Action press, with a healthy display, for 60
seconds. The badge shows a random six-digit passkey as two rows of three and
never logs it. Bonded phones reconnect without a new window. There's one
connection and one bond slot, and an old bond is never overwritten. Factory
reset clears keys and settings but not the NFC tag.

## Stage, then apply

Writes stage a value in the current connection; reads return what's
applied. Prepared writes, ATT offsets and write-without-response are refused.
Everything fits the default ATT MTU of 23.

Name, and URLs over 20 bytes, are written as `[offset:u8, total:u8, data...]`
with 1–18 data bytes per write. Offset 0 starts over; later chunks must be
contiguous with the same total. A short URL can also be written raw (its
first byte is a letter, so it can't be mistaken for a header).

Apply fields: URL 1, name 2, brightness 3, mode 4, text 5, artwork 6, motion 7,
NFC resync 127. Each Apply commits one field. Request IDs are nonzero and
increase within a connection; a repeated or older ID is refused. Only one
Apply runs at a time.

The ATT response means accepted, not done. The work runs on a worker thread.
A URL Apply only completes once the QR code is shown and the NFC tag agrees.
Other fields go to the display before saving and roll back on failure.

If the link drops, staging is discarded. An Apply that hadn't started is
cancelled; one already running may finish. The app marks it unknown,
refreshes after reconnecting, and never replays it.

## Status

| Offset | Size | Meaning |
|---|---|---|
| 0 | 1 | Version 2 |
| 1 | 4 | Request ID |
| 5 | 1 | Field, 0 when idle |
| 6 | 1 | Phase: idle 0, pending 1, complete 2 |
| 7 | 4 | Result, signed errno: pending −115, success 0 |
| 11 | 1 | Flags: bit 0 writes available, bit 1 NFC in sync, bit 2 pairing window open |

Pending is always −115, whatever the C library's `EINPROGRESS` is. Common
errors: invalid −22, access −13, busy −16, storage −5, timeout −110,
cancelled −125. A client checks version, ID, field, phase complete and result
0, then reads back the exact bytes. A URL also needs the NFC-in-sync flag.

Apply URL with ID `12345678h`: `02 78 56 34 12 01`.
URL ID 1 completed with access denied, writes available: `02 01 00 00 00 01 02 F3 FF FF FF 01`.

## Text

`[1, layout, scale, alignment, interval:u16, text...]`, up to 96 printable
ASCII characters or LF. Layout: auto fit 0, word wrap 1, scroll 2, bounce
lines 3. Scale: auto 0, or 1–3. Alignment: left 0, center 1, right 2.
Interval is 50–500 ms per scroll pixel; pages hold for 20 intervals. Empty
text shows the name. Bounce lines move every overflowing line in one shared
back-and-forth cycle. The app preview uses the same font and rules.

Staged as `[offset:u16, total:u16, 1–16 bytes]`. Long reads return it whole.

## Artwork

Binary (V1): `[1, count, interval:u16, frames...]`, 1–128 frames of 128 bytes,
interval 50–2000 ms, row-major, MSB first: pixel (x, y) is bit `7 - x%8` of
byte `y*4 + x/8`.

Grayscale (V2): `[2, count, interval:u16, playback, 0]`, then per frame
`[encoding, length:u16, duration_ms:u16, payload]`. Up to 128 frames, 50–2000
ms each. Playback: loop 0, once 1, reverse 2, ping-pong 3. Encodings: packed
binary 0 (128 bytes), raw gray 1 (1024 bytes), RLE 2, and XOR-delta RLE 3
against the previous frame (not allowed on frame 0). An RLE run is `[run:u16, value]` and runs must
total 1024. Anything malformed, including trailing bytes, is refused.

Drawing mode shows frame 0. Animation plays by elapsed time and may skip
frames when busy. Gray levels scale the global brightness.

### Upload

`[2, offset:u32, total:u32, payload]`, 1–235 payload bytes, 244 bytes at most.
Ask for MTU 247; at MTU 23 the payload is 11 bytes. Offsets must be
contiguous with a constant total. The first fragment restarts the inactive
bank, and a failed upload never replaces what's showing. The older
`[1, offset:u16, total:u16, 1–15 bytes]` form works below 64 KiB.

### Readback

Write a selector, then read:

| Selector | Read returns |
|---|---|
| `[4]` sequential | `[offset:u32, total:u32, payload]`, advancing after each complete read |
| `[2, offset:u32]` | `[offset:u32, total:u32, payload]` at that offset |
| `[0, offset:u16]` | `[offset:u16, total:u16, up to 16 bytes]` (older form) |
| `[3]` metadata | `[length:u32, crc32:u32]` |

Responses are at most 244 bytes, kept below the ATT read size so clients
never issue Read Blob. Sequential reads carry 236 content bytes at MTU 247.
If a page fails, start again from `[4]`. Clients compare the metadata before
and after a full read. Measured on the DK: 3,551 bytes in 786 ms at MTU 247.

While an upload is active (10 s after its last fragment, and through Apply),
recording can't start. The flash worker, not the Bluetooth callback, writes
flash. Android asks for high connection priority only during transfers.

## Motion

`[1, auto, right, down, response, particles, rotation, 0]`. `auto` is 0 or 1.
Axes: +X 0, −X 1, +Y 2, −Y 3, +Z 4, −Z 5, and right and down must differ.
Response 1–5, particles 16–192, rotation 0–3 quarter turns for manual mode.
Default `01 01 00 02 03 60 00 00`. Rev A ignores the axis bytes and uses its
fixed mount (+X right, −Y down).

Motion status, a snapshot:

| Offset | Size | Meaning |
|---|---|---|
| 0 | 1 | Version 1 |
| 1 | 1 | Flags: ready 1, fresh (≤ 250 ms) 2, interrupt registered 4 |
| 2 | 1 | Current rotation, 0–3 |
| 3 | 1 | 0 |
| 4 | 2 | Sample age in ms, 65535 if unknown |
| 6 | 2 | Filtered down vector X, mm/s², signed |
| 8 | 2 | Filtered down vector Y, mm/s², signed |
| 10 | 2 | 0 |
| 12 | 4 | Last sensor errno |

## NFC resync

Stage `[1]` on NFC sync, then Apply field 127. The badge rewrites the tag from
the current link through the guarded NFC worker. Keep phones away while it
runs. Reading returns `[1, ready, busy, result:i32]`.

## Battery

`[1, level, flags, 0, mv:u16]`. Level: unknown 0, almost empty 1, low 2, about
half 3, mostly full 4, full 5. Flags: bit 0 USB, bit 1 charging, bit 2 shed
(level then reads almost empty). `mv` is the near-rest voltage behind the
level. The level is a rough voltage estimate ([power](power.md#battery-measurement)).
The DK reports 0. No Info flag advertises it; clients just look for it.

## QR links

Only QR Version 1 fits 32 × 32 with the full quiet zone (21 + 8 = 29). The
encoder picks the strongest error correction that fits. Alphanumeric/byte
capacity: H 10/7, Q 16/11, M 20/14, L 25/17, including the scheme. The
alphanumeric set is digits, uppercase letters and `$%*+-./:`, so uppercase
links fit better. Lowercase or other punctuation uses byte mode. Fitting a
code doesn't mean phones will read it.

## Storage

Settings use schema v1 in NVS, with optional text, artwork and `badge/motion`
records. Artwork lives in two 132 KiB flash banks at `0xb0000`, with a
manifest `badge/art-meta` = `[3, bank, 0, 0, length:u32, crc32:u32]`. The
inactive bank is written and read back before the manifest switches. If a
manifest write reports an error, both banks are locked until restart, since
it may have committed anyway.

## Older firmware

The app still talks to older badges using what they advertise: v1 Info
`[1,20,32,4,2,flags,0,0]` for basic controls, 20-byte URLs, 16-frame binary
artwork, 32-frame grayscale with an 8192-byte limit, and the 6 % brightness
preset. Firmware only accepts v2 Apply. Re-discover services after flashing,
since phones cache the GATT table.

[DK BLE results](../validation/results/2026-10-13-dk-p2-settings/README.md),
[NFC policy](nfc.md), [Zephyr GATT](https://docs.zephyrproject.org/latest/services/connectivity/bluetooth/api/gatt.html).
