# 0019: Flash-backed artwork and fixed app workspaces

Status: Accepted

Date: 2026-10-16

## Decision

Support 128 frames/131718 encoded bytes and compact fixed app workspaces.
Libraries open separately; drafts save automatically. Local libraries hold
at most 64 snapshots within 16 MiB.

Reserve two 132 KiB flash banks, keeping MCUboot/image slots and the 32 KiB
settings offsets. This consumes the old network-update reserve, leaving 24 KiB;
a complete network update no longer fits that reservation.

A flash worker stages aligned fragments in the inactive bank, lazily erases,
and reads back. Commit the CRC/length/bank manifest last. If its acknowledgement
fails, block uploads until restart/reload resolves which bank is durable.
Keep legacy artwork loading; unknown/corrupt manifests fail closed.

Use immutable flash views and a bounded playback cache, not animation-sized
RAM copies. Extension bit 6 adds 32-bit cursors and MTU-sized fragments.
Always verify complete applied content; invalidate metadata caches after Apply.
Uploads exclude recording/NFC work and pause local PDM before flash operations.

This preserves capacity at the cost of flash space and longer transfers.
Flash latency, optical timing, and combined-load behavior still need measurement.
[Storage](../rev-a-firmware.md#flash-and-ram) · [Wire contract](../ble-protocol.md)
