# 0019: Flash-backed artwork and fixed app workspaces

Status: Accepted by explicit owner instruction, 2026-10-16. Amends 0012 and
0018's storage allocation/frame bounds. Prototype remains nRF5340 DK; nRF52840
fit and Rev A hardware scanning are separate port/acceptance work.

The owner accepted the proposed 128-frame dedicated artwork store and requested
fixed main pages inspired by their Glyph Museum screenshot. Repeated headings,
long helper copy and stacked cards give way to a canvas, compact tools and inline
link actions. Libraries open separately and may scroll. Text inputs, horizontal
frame strips and focused settings dialogs retain their normal interaction.
Light/dark still follows the phone. No upstream code or assets are copied.

Support 128 frames and 131718 encoded bytes: 128 raw 1024-byte gray frames plus
six animation-header and five per-frame-header bytes. Compression is optional
for fitting an ordinary full 128-frame clip. Keep V1 binary/V2 gray encoding,
individual durations and loop/once/reverse/ping-pong. Local library bound becomes
16 MiB for up to 64 snapshots; no cloud storage or account is introduced.

On the DK, carve 264 KiB from the previously inert network-update reserve into
two 132 KiB artwork banks. Keep application/MCUboot slots and the 32 KiB settings
partition at their existing addresses. 24 KiB remains reserved. This removes the
previous reservation for a complete network update; network DFU was not
implemented. The later single-core nRF52840 layout must budget code/controller,
bootloader, update strategy and assets together; this DK layout is not its port.

Stage sequential fragments into the inactive bank, with four-byte aligned writes,
lazy 4 KiB erase and readback. A dedicated worker performs flash operations;
Bluetooth callbacks hand off bounded fragments. Publish the CRC32/length/bank
manifest last in NVS `badge/art-meta`, format3. Retain the old active bank until
that commit. If the manifest write acknowledgement fails, block further artwork
uploads until reboot/reload establishes which bank is durable. Legacy inline and format1 chunk artwork remain loadable on
upgrade.
Unknown/corrupt manifests fail closed. Older firmware rejects format3, so a
supported downgrade requires the matching previous settings backup; it must not
silently revive an old URL/artwork schema. No factory reset is required to upgrade.

Settings/modes/commands hold immutable lifetime-owned artwork views, not full
animation copies. Playback reads mapped flash using a bounded frame cache and
sequential delta decoding; independent earlier frames are skipped. Public typed
inputs must have stable backing. Boot legacy data lives in a static 8 KiB cache.

Extension bit6 advertises wide artwork transport. Use 32-bit lengths/cursors and
MTU-sized chunks, preserving authenticated Apply and exact complete readback.
A length/CRC metadata query avoids rereading unchanged assets for scalar changes;
verify the payload CRC as well as stable metadata across a full read. After
artwork Apply, invalidate the cache and verify full bytes. Closed links cancel
paged reads before another cursor write. Older capabilities retain their bounds.
New upload gates block recording starts, pause local PDM
visualization before flash work and hold through Apply. Existing recording and NFC
updates reject overlapping upload work. Inactivity/disconnect releases staging.

Software tests cover maximum raw clips, bank/manifest interruption, legacy
migration, corruption, cross-64-KiB transfer, bounded workspaces, layout/keyboard/
large-type/library regressions and Android media. Physical flash latency, combined
load, optical gray steps/refresh and recording-view visibility still need bench
evidence. No compile result closes hardware risks.
