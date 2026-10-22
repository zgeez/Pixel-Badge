# BLE configuration protocol v2

Authenticated GATT configuration for Pixel Badge. The DK uses an IPC radio
core; Rev A runs host/controller on one core. Audio has a separate
[wire format](audio-protocol.md). Recording is disabled in normal builds;
there is no firmware-update command. Older capability sets remain supported.

UUIDs use the suffix `-6e9b-4f4c-a8f8-731a9d46e1c0` below.

| UUID prefix | Purpose | Properties | Encoding |
|---|---|---|---|
| f3b00001 | Service | — | Custom 128-bit UUID |
| f3b00002 | Info | Read | 8 bytes: version=2, URL max=25 (older firmware: 20), name max=32, brightness count=4, modes=4/6/7, flags, max frames=128 (older: 16/32), extension flags=231/239/255 (older: 7/15/31, 39/47/63 or 103/111/127) |
| f3b00003 | Name | Read / Write | Read applied ASCII; writes use fragments below |
| f3b00004 | URL | Read / Write | Exact ASCII HTTP(S), 1–25 bytes, V1 QR fit required |
| f3b00005 | Brightness | Read / Write | One byte: 13, 64, 128 or 255; legacy 16 also accepted; see editor extension |
| f3b00006 | Mode | Read / Write | QR=0, text=1, drawing=2, animation=3, level=4, spectrum=5, fluid=6; only advertised modes accepted; Apply also stores the boot default |
| f3b00007 | Apply | Write with response | `[2, request_id:u32le, field:u8]`, six bytes |
| f3b00008 | Status | Read / Notify | 12 bytes, layout below |
| f3b00009 | Text | Read / Write | Applied content / staged fragments; 6–102 bytes |
| f3b0000a | Artwork | Read / Write | Paged applied content / staged fragments; 14–131718 bytes (legacy V1 minimum 132) |
| f3b0000b | NFC sync | Read / Write | Seven-byte NFC state / staged `[1]` action |
| f3b0000c | Motion | Read / Write | Applied/staged eight-byte motion setting |
| f3b0000d | Motion status | Read | 16-byte sensor/orientation snapshot |

Info is public; all other reads, writes and CCC subscriptions require
authenticated LE Secure Connections, security level 4. Just Works and legacy
pairing are refused. New pairing requires an Action/Button 2 long press,
healthy display and a 60-second window. The six-digit random passkey is shown
in two rows of three digits, with leading zeroes, never logged. Bonded
reconnects do not require a new window. One connection and one bond slot;
no oldest-bond overwrite. Factory reset clears Bluetooth keys and settings; external tag configuration
is retained and NFC policy/URL checks run again without a phone verifier.
The window is an implementation timeout, not a measured acceptance criterion.
Button 1 keeps mode cycling; short button actions are held during Apply or
passkey display.
Button 1 skips empty artwork and animations with fewer than two frames.

## Stage and apply

Setting writes stage in the current connection; reads return applied state.
Name is 1–32 printable ASCII bytes. Each name write is
`[offset:u8, total:u8, ASCII...]`, with 1–18 data bytes. Offset 0 starts/replaces
the name; later chunks must be contiguous with the same total. Incomplete
names cannot be applied. Other writes contain raw values. Prepared writes,
nonzero ATT offsets and write-without-response are rejected. Write/status
packets fit ATT MTU 23; ordinary long reads handle names longer than 20 bytes.

Apply fields: URL=1, name=2, brightness=3, mode=4, text=5, artwork=6, motion=7,
NFC resync=127 (ephemeral action). Apply changes **one staged
field**, with no multi-field transaction. IDs are nonzero, strictly increasing
within a connection. Duplicate/older IDs are rejected without replay; read
Status to choose the next ID. Reconnect at ID exhaustion. Staging is consumed
on acceptance. Only one Apply is outstanding globally, including across reconnects.

The ATT response means acceptance, not completion. Work is queued; Bluetooth
callbacks never perform flash or I2C operations. Main owns settings and display.
URL uses P1 preview/guarded NFC/persistence/rollback; success requires QR
submission and tag agreement. Other fields submit to the display before saving,
and restore prior state on submission/storage failure. Mode reads describe
the active mode, including Button 1 changes. Stored settings work offline.

## Status

| Offset | Size | Meaning |
|---|---|---|
| 0 | 1 | Version=2 |
| 1 | 4 | Unsigned request ID, little endian |
| 5 | 1 | Field; 0 only while idle |
| 6 | 1 | Phase: idle=0, pending=1, complete=2 |
| 7 | 4 | Signed errno, little endian: pending=-115, success=0, failures negative |
| 11 | 1 | Flags: bit 0 writes available; bit 1 NFC synchronization verified; bit 2 pairing window; others zero |

The pending value is a fixed protocol constant, independent of the C library's
EINPROGRESS number (DK picolibc uses119; Linux host uses115). Firmware normalizes
this value; the app continues to reject a noncanonical pending packet.

Idle has ID=0, field=0, result=0. Representative errors: invalid=-22,
provisioning/access=-13, busy=-16, storage/transport=-5, timeout=-110,
cancelled-before-execution=-125. ATT rejects malformed/unauthenticated/unstaged
or busy requests before acceptance. Read Status if a notification is dropped.
Readback is published before completion. Match version, ID, field, complete
phase and result zero; then read the exact applied bytes. URL activation also
requires the NFC-verified flag. Normalize only the HTTP(S) scheme, never a path.

Disconnect discards staging/status. Queued work not yet begun is cancelled;
an already-running URL request may finish after disconnect. Its completion
cannot attach to a new session. The client marks this indeterminate and
refreshes after reconnect, **never automatically replays Apply**. Failed or
uncertain readback also requires Refresh before another Apply. Separate
settings/tag storage does not provide physical power-loss atomicity.

Golden Apply URL ID `12345678h`: `02 78 56 34 12 01`.
Completed URL ID 1, access denied, writes available:
`02 01 00 00 00 01 02 F3 FF FF FF 01`.

## Studio content and NFC action

Info extension flags are text=1, artwork=2 and NFC resync=4. This version
requires all three. Text/artwork write chunks fit the default 20-byte ATT
payload. Offset 0 replaces a staged object; later chunks must be contiguous
with an unchanged total. Apply rejects incomplete or semantically invalid
objects. No EEPROM/NVS write occurs during staging.

Text payload: `[1, layout, scale, alignment, interval:u16le, ASCII...]`.
Maximum 96 printable ASCII bytes or LF, minimum six-byte header. Layouts are
auto fit=0, word wrap=1, scrolling=2 and bounce lines=3 (editor extension).
Scale is auto=0 or 1/2/3; alignment
is left=0, center=1, right=2. Interval is 50–500 ms per scroll pixel;
paginated layouts hold each page for interval × 20 ms. Auto fit with automatic
scale chooses the largest size whose wrapped text fits one page, falling back
to small paginated text. Word wrap with automatic scale uses small text.
Empty text uses the stored name. The case-sensitive 5 × 8 font is shared with
the app. Text staging is `[offset:u16le, total:u16le, 1–16 payload bytes]`.
Ordinary GATT long reads return the complete applied text (at most 102 bytes).

Artwork payload: `[1, count, interval:u16le, frames...]`. Count is 1–32 (older badges: 16),
interval 50–2000 ms. Each frame is exactly 128 bytes, row-major, MSB first:
pixel `(x,y)` is bit `7-(x%8)` of byte `y*4+x/8`. Drawing displays frame 0;
animation selects `(elapsed/interval)%count`. The runtime uses elapsed time
and may skip frames when busy. Nominal playback interval is not a measured
frame-rate guarantee. Staging is
`[1, offset:u16le, total:u16le, 1–15 payload bytes]`.

Artwork is larger than GATT's 512-byte attribute limit, so readback is paged.
Write `[0, offset:u16le]` to select an applied read cursor, then read
`[offset:u16le, total:u16le, up to 16 payload bytes]`. The cursor is specific to
the single connection and resets on connect. The client checks contiguous
offsets, stable total and exact complete bytes, with Status before/after the
read. Cursor selection does not stage content or persist anything.

NFC read: `[1, ready, busy, result:i32le]`, where ready/busy are 0 or 1 and
result is non-secret diagnostic status. Stage `[1]`, then Apply field127 to
reconcile the current QR URL through the existing guarded NFC worker. This
does not change URL, credentials or provisioning policy and does not persist
a field127 setting. Successful completion additionally requires NFC-ready
readback. Keep phones away during synchronization. No saved RF-proof receipt
or phone verifier is required; see ADR 0017 below.

Settings schema remains v1 with optional `text` and `artwork` records. Older
core settings survive the upgrade; missing/invalid extensions default to
name-backed text and one blank frame. Schema recovery writes all defaults
before activating the schema. NVS stores bounded artwork in verified chunks with an atomic manifest; see below.
Sending content then selecting its mode is two separate confirmed Applies,
so interrupted clients must refresh rather than assume both completed.

The updated app reads v1 Info `[1,20,32,4,2,flags,0,0]` for basic controls and
v2 Info for Studio. Firmware accepts v2 Apply only. Old v1 apps need updating.
Re-discover services after flashing; OS GATT caches can retain the old table.
Extensions are appended after the original v1 attributes to preserve their
order. Service discovery must still find the newly added characteristics.

## Bench

Use the [BLE checks](ble-bench.md) with the Prototype image and Pixel Badge app.
The [dated settings/pairing record](../validation/results/2026-10-13-dk-p2-settings/README.md)
separates captured results from manual observations and unfinished checks.

### NFC setup and synchronization

Boot, Apply link, and Sync again verify readable tag policy and exact NDEF URL.
Success needs the synchronized flag and readback, not an RF-proof receipt.
Fresh/misconfigured tags use [explicit wired setup](nfc-prototype.md).
Existing tags retain their credentials; normal operations never personalize them.

### Restore NFC verification (historical optional RF diagnostic)

The old two-session proof is optional diagnostics only. It no longer gates
boot or Apply. See [NFC tools](../scripts/nfc/README.md) and
[ADR 0017](decisions/0017-local-nfc-synchronization.md).

## Software verification (2026-10-12)

Historical build/test/deployment counts are in the
[validation records](validation.md). Use their firmware versions when interpreting
results; later protocol extensions need their own tests.

## Motion extension

Info pairs are modes=4/extensions=7 for Studio, 6/15 for audio and 7/31 for
audio plus motion. Extension bit 4 advertises Motion and its status snapshot.
The matching app accepts older capability sets; older clients can reject the
new set and should be updated with the firmware. Existing characteristic and
field IDs are unchanged. Motion uses field 7; NFC action 127 has an independent
staging slot. The eight-byte value is validated and saved as one setting,
`badge/motion`, under additive schema v1. Missing old records use defaults.

Motion encoding: `[1, auto, right, down, response, particles, rotation, 0]`.
`auto` is 0/1; signed axes are +X=0, -X=1, +Y=2, -Y=3, +Z=4, -Z=5.
Right and down must name different physical axes. Response is 1–5, particles
16–192, and rotation 0–3 represents 0/90/180/270 degrees for manual mode.
Default bytes: `01 01 00 02 03 60 00 00`. The mount is not yet bench-validated.

Motion status is a read-only authenticated snapshot, not a sample stream:

| Offset | Size | Meaning |
|---|---|---|
| 0 | 1 | Version=1 |
| 1 | 1 | Flags: device ready=1, sample fresh=2, trigger registered=4 |
| 2 | 1 | Current content rotation, quarter turns 0–3 |
| 3 | 1 | Reserved zero |
| 4 | 2 | Sample age in ms, unsigned LE; 65535 means unknown or saturated |
| 6 | 2 | Filtered projected down X, signed LE mm/s² |
| 8 | 2 | Filtered projected down Y, signed LE mm/s² |
| 10 | 2 | Reserved zero |
| 12 | 4 | Last sensor-read errno, signed LE |

Fresh means no more than 250 ms old. Trigger registration is not evidence of
observed interrupts. Current rotation can differ from a QR's locked entry
rotation. See [motion-prototype.md](motion-prototype.md) for behavior and tests.

References: [Zephyr GATT](https://docs.zephyrproject.org/latest/services/connectivity/bluetooth/api/gatt.html),
[Zephyr LE host](https://docs.zephyrproject.org/latest/services/connectivity/bluetooth/bluetooth-le-host.html),
[Universal BLE](https://pub.dev/packages/universal_ble), [NFC policy](nfc.md).

## Extended QR links and staging

Info byte 1 advertises 25 on this firmware, or 20 on older badges. The app
retains the older V1-M limits when connected to an older badge. Web links only:
QR and NFC share the exact same HTTP(S) destination, preserving path case.

Only Model 2 Version 1 fits 32 by 32 with the full four-module quiet zone:
21 modules plus eight is 29; Version 2 needs 33. Encoding automatically selects
the strongest correction that fits, H then Q then M then L. Alphanumeric/ASCII
capacities are H10/7, Q16/11, M20/14 and L25/17, including the URL scheme.
The alphanumeric alphabet is digits, uppercase letters, space and `$%*+-./:`;
URL syntax still rejects spaces. Other accepted URL punctuation or lowercase
uses ASCII byte mode. Larger or cropped QR symbols and Micro QR are not emitted.
Fitting a symbol is not physical scan acceptance, particularly at level L.

Raw URL writes remain compatible. For a URL over 20 bytes, each ATT write is
`offset:u8, total:u8, data[1..18]`, exactly the existing name envelope. The first
offset is zero; total is at most 25; fragments are contiguous and URL validation
runs only when complete. All writes fit the default ATT MTU. Fragment headers
cannot be confused with raw HTTP(S), whose first byte is a printable letter.
Staging remains independent for every field, using field-sized bounded buffers.


## Grayscale artwork extension (ADR0018)

Info extension bit 5 (`32`) advertises lossless 8-bit artwork; byte 6 is 32.
The historical 0.7 grayscale contract below uses mode/extension pairs 4/39,
6/47 or 7/63. ADR0019 supersedes its capacity/storage/transport bounds; existing
older badges retain those bounds. The matching app retains support for
older 16-frame binary badges and refuses unsupported gray/large transfers.
The encoded badge budget is 8192 bytes, including all headers. Local drafts can
retain up to 32 raw gray frames without being sendable; validate before staging.

Binary V1 remains `[1,count,interval:u16le,128-byte packed frames...]`.
V2 starts `[2,count,interval:u16le,playback,reserved=0]`; each frame starts
`[encoding,payload_length:u16le,duration_ms:u16le]`. Counts are 1..32 and all
durations/default interval are 50..2000 ms. Playback is loop=0, once=1 (hold
final), reverse=2, ping-pong=3 (endpoints once per cycle). Drawing uses frame 0.

Encoding 0 is 128-byte packed binary, 1 is 1024 row-major intensity bytes,
2 is RLE and 3 is XOR-delta RLE against the previous decoded frame (not frame 0).
Each RLE triplet is `[run:u16le,value]`; positive runs must total exactly 1024.
Delta values XOR the corresponding previous pixels. Reject reserved bits,
unknown encodings, invalid sizes/runs/durations and trailing bytes. Transfer
fragments/read cursors are unchanged. Global brightness still accepts only
16/64/128/255; per-pixel intensities multiply this global LED on-time budget.

Storage preserves schema v1 and the existing 32 KiB partition. Legacy `artwork`
loads when no new manifest exists. `art0/0..7` and `art1/0..7` are inactive/active
1024-byte chunk banks. `art-meta` is `[1,bank,chunk_count,0,length:u32le,CRC32:u32le]`;
CRC32 uses IEEE polynomial. Write/read back inactive chunks before committing
the manifest last. Missing/corrupt selected chunks fail closed. Schema recovery
stores the complete default set before activating the schema. No partition move
or factory reset is needed; older firmware cannot interpret a newer manifest.


## Large-artwork transport (ADR0019, extension bit 6)

Bit6 (`64`) requires bit5 (gray); Info byte6 is128. Mode/extension pairs are
4/103,6/111,7/127. A complete 128-frame raw gray animation is131718 bytes,
including headers. The app still enforces16/32-frame and2052/8192-byte limits
on older advertised capabilities. Apply/Status version remains2.

Wide artwork stage: `[2,offset:u32le,total:u32le,payload...]`,9-byte header,
1..235 payload bytes, total packet at most244. Request ATT MTU247 where supported;
fall back to20-byte packets with11-byte payloads. Require contiguous offsets,
constant bounded total, authenticated current connection, complete valid content.
The first fragment restarts only the inactive upload; failure never commits it.
The callback waits for a dedicated flash worker; it does not program flash itself.

Wide read cursor: write `[2,offset:u32le]`, then read
`[offset:u32le,total:u32le,payload...]`, at most244 response bytes. Offsets/total
must match exactly on every page. Legacy `[0,offset:u16le]` cursors and selector1
uploads remain bounded-compatible below65536 bytes. A metadata cursor `[3]`
returns `[length:u32le,CRC32:u32le]`. Compare before/after full reads; cache only
unchanged metadata within the current link and a matching payload CRC. Publish
metadata from the same locked settings snapshot as paged data. Closing a link
cancels its outstanding reads before another cursor request. Always invalidate after artwork Apply
and verify all applied bytes before confirmation. No authentication is relaxed.

Artwork is now an immutable mapped view, not an embedded Settings/command array.
Two132KiB banks occupy0xb0000..0xf2000 on the DK; settings stay0xf8000..0x100000.
Manifest format3 in `badge/art-meta` is `[3,bank,0,0,length:u32le,CRC32:u32le]`.
Write/read back inactive data before committing the NVS manifest. Format1 chunks
and the legacy single record load on upgrade. Old firmware fails closed on the
new manifest; use the matching backup for downgrade. No automatic provisioning,
bond reset or settings erase occurs on upgrade. A failed manifest-write
acknowledgement pins both banks until restart/reload, because the manifest may
have committed despite its error result. The app requests a restart before retry.
The remaining 24 KiB reserve does not hold a full network update.
The [Rev A layout](rev-a-firmware.md#flash-and-ram) budgets the controller and app together.

An upload lease lasts10s after its latest fragment and holds through Apply.
Recording starts are refused while it is held; uploads refuse active/requested/
draining recording or an NFC operation. Local visualization DMA is stopped by
the main thread before the worker can erase/write (bounded500ms handoff), then
resumes when the lease ends. These are implementation bounds, not measured
combined-load acceptance. Library/read-only metadata work remains available.

## Editor and sequential-read extension (bit 7)

Info extension bit7 (`128`) requires large-artwork bit6. Mode/extension pairs
are4/231,6/239,7/255. It advertises bounce-lines text, the5% brightness preset
and sequential artwork reads. Clients retain the old paths when bit7 is absent.

Brightness presets are13/64/128/255 (approximately5/25/50/100% of the global
on-time budget). Legacy16 is still accepted; settings load/application normalizes
it to13 without an eager storage write. Older connected firmware keeps the
app's16-based6% preset. These values are not per-pixel gray levels or measured
optical brightness; artwork intensities remain0..255.

Text layout3 preserves explicit LF lines, including empty lines. With the0.10.1
renderer, overflowing lines share one triangular phase and reverse together.
Each travels from left=+3 to left=-(overflow+3), with no deliberate endpoint hold;
shorter overflowing lines move more slowly over the same half-cycle. Lines that
fit retain their alignment. Auto scale uses1x. Half-cycle duration is the longest
overflow plus6 pixels, multiplied by the50..500ms interval. Position uses elapsed
milliseconds and nearest-pixel rounding. Extra rows page after a complete cycle,
or40 intervals when none scroll. The app preview uses the same rules. This refines
the earlier0.10.0 endpoint-pause behavior without changing the payload format.

Write artwork selector `[4]` once to reset the connection's sequential cursor.
Each read returns `[offset:u32le,total:u32le,payload...]`, advancing only after a
complete successful response. Responses are capped at244 bytes and kept strictly
shorter than the ATT read-response capacity so clients do not issue Read Blob.
At MTU247 this carries236 content bytes; MTU23 carries13. Nonzero ATT read offsets
are rejected in sequential mode. Explicit offset selectors and metadata selector
`[3]` disable sequential mode. A failed/uncertain page aborts the full read; a new
read starts with `[4]`, never by blindly repeating a potentially advanced page.

Contiguous offset/total checks, payload validation, stable before/after metadata,
CRC and exact post-Apply readback still apply. One cursor write per page is
removed; upload staging remains acknowledged, with235 content bytes per write
at MTU247. Android requests high-performance connection priority only during
actual artwork transfers and restores balanced priority afterwards. This is an
optional OS hint, not a guarantee of connection interval or throughput.

[Local measurement and deployment](../validation/results/2026-10-17-editor-transfer/README.md):
3551 content bytes read in786ms at MTU247, about4.5kB/s. This is one small-asset
read, not a maximum-size upload benchmark or Rev A acceptance.
