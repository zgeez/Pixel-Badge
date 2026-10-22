# 0003: 32×32 monochrome display and QR-first validation

Status: Accepted; grayscale added by [0018](0018-grayscale-studio-recording-view.md)

Date: 2026-10-04

## Decision

Use 32 × 32 white SMD LEDs, initially binary with global brightness control.
Test QR on the HUB75 fixture, then repeat on the custom white-LED board.
Pitch, current, and brightness require optical/electrical evidence.

Version 1 is the standard QR geometry: 21 modules plus a four-module border
occupies 29 × 29 pixels. Version 2 needs 33 × 33 and does not fit. Reject
oversized payloads instead of clipping the quiet zone.

The RGB prototype cannot establish final LED brightness, camera behavior,
or thermals. Grayscale changes require fresh timing/optical tests.
[Display tests](../testing/display.md) · [QR sources](../references.md#standards)
