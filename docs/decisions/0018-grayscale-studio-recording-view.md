# 0018: Grayscale Studio and recording visualizers

Status: Accepted by explicit owner instruction, 2026-10-16. Implementation and
software checks are separate from panel timing, visibility and power acceptance.
Amends 0003, 0011 and 0016. The nRF52840 Rev A port remains later work.

The owner requested 0-255 pixel brightness, more animation frames where bounded,
library renaming, automatic draft persistence, richer media editing, and removal
of REC lettering in favor of full spectrum and level views while recording.

Keep 32x32 single-color pixels. Encode 8-bit intensity losslessly; retain binary
V1 compatibility. Support up to 32 frames, individual 50-2000 ms durations and
loop, once, reverse or ping-pong playback. The badge accepts at most 8192 encoded
bytes. Raw, binary, run-length and XOR-delta frames keep simple designs compact.
The editor/library may retain a larger local design; transfer must reject excess
before changing the badge. This is a format/storage bound, not a phone limit.

Store artwork in two banks of 1024-byte NVS chunks with readback, CRC32 and a
manifest committed last. Keep the existing 32 KiB partition and legacy record
loading. No settings erase, bond reset or partition relocation is part of this
update. Owner-only settings workspaces move from stacks to bounded static RAM.

On the DK HUB75 backend, all content uses the same eight weighted planes and
hardware timer/DPPI OE edges (2026-10-17 correction). Removing the single-plane
optimization avoids a whole-frame brightness jump when any intermediate shade
appears. Binary QR/text now share the existing gray duty budget; fresh optical
and combined-load checks remain required. One-shot
slots extend blanking under interrupt delay rather than extending an LED pulse.
The current 250 ns base pulse and resulting optical shades need scope/panel and
combined audio/BLE/motion acceptance. Encoded 256 values do not prove 256
perceptually distinct shades. No Rev A refresh, memory or battery claim follows.

During pending, active and draining recording, use a full spectrum plus level
meter at global brightness 255. Fixed lit corner pixels remain visible in
silence. Remove REC lettering. Keep presentation-before-capture, stop on display
failure, authenticated live transport, foreground-only start and existing stop
paths. Release recording remains disabled pending hardware acceptance. Visibility
of this changed recording view must be assessed again; prior REC-label evidence
does not validate it.

Implement the editor independently using documented workflows from
[GlyphMatrixEditor](https://github.com/pauwma/GlyphMatrixEditor) and
[Glyph Museum](https://app.glyphmuseum.com/editor?remix=6748): brush intensity,
shapes, selection, transforms, onion skin, timeline and media conversion. No GPL
source or assets are copied. Use Flutter/native system codecs with explicit
resource bounds. Android media work runs separately from recording packet I/O.
