# 0001: Rev A product scope and engineering priorities

Status: Accepted; recording amended by [0011](0011-visible-voice-recording.md)

Date: 2026-10-04

## Decision

Build a reusable badge with QR/name display, motion-driven fluid, audio
visualization, BLE configuration, passive NFC, and a battery-powered custom PCB.
It must work without a phone after configuration. Keep a DK/breakout fallback,
but do not count that as completed Rev A hardware.

Priorities are QR reliability, electrical correctness, display timing,
firmware recovery, power, connectivity, wearability, then polish. Keep debug
access and avoid stretch work that delays those features.

The original scope excluded stored microphone audio; [0011](0011-visible-voice-recording.md)
later allowed explicit phone recordings while keeping audio off badge storage.
[Architecture](../architecture.md) · [Tests](../validation.md)
