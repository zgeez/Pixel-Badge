# 0018: Grayscale Studio and recording visualizers

Status: Accepted; frame/storage bounds superseded by [0019](0019-large-artwork-fixed-workspaces.md)

Date: 2026-10-16

## Decision

Add lossless 0–255 per-pixel intensity, animation timing/direction, media tools,
automatic drafts, and library rename. Retain binary artwork compatibility.
The initial bound was 32 frames/8192 encoded bytes with verified chunk banks
and a manifest committed last; 0019 replaces that capacity/storage scheme.

Use eight weighted planes for all DK content, including QR/text. Hardware OE
edges and one-shot slots extend blanking when work is late. The 250 ns base
pulse is an implementation value; 256 encoded levels do not prove 256 optical
shades.

Replace REC text with spectrum/level at brightness 255 and lit corners in
silence. Retain presentation-before-capture and failure stops; repeat visibility
testing for this view before enabling recording by default.

Implement editor workflows independently from documented Glyph tools, without
copying their source/assets. [App](../../app/README.md) ·
[BLE formats](../ble-protocol.md#grayscale-artwork-extension-adr0018)
