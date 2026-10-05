# 0001: Rev A product scope and engineering priorities

- Status: Proposed
- Date: 2026-10-04
- Deciders: Project owner

## Context

Rev A is a reusable wearable identity/networking badge and a debuggable,
reworkable engineering prototype. A clear feature baseline is needed to keep
implementation and validation focused. No product features or measurements
exist yet.

## Options considered

1. Minimal QR/name demo: less integration work, but omits the selected
   interactive and contact-sharing scope.
2. Complete Rev A baseline with a bounded fallback demo: preserves the product
   scope while allowing an explicit fallback if the custom hardware slips.

## Decision

Choose the complete Rev A baseline: QR and name/identity, IMU-driven Flip/fluid
simulation, audio visualization with no stored microphone audio, companion-app
BLE configuration, passive NFC sharing the QR URL, and a battery-powered custom
PCB. The badge remains usable without a phone after configuration. Stretch
features must not delay Rev A. A fallback demo is not completion of Rev A.

Prioritize QR reliability, electrical correctness, matrix timing/brightness,
firmware reliability, battery/power behavior, BLE/NFC usability, wearability,
and visual polish, in that order. Preserve debug and recovery access.

## Consequences

Fluid and audio are required Rev A features. Firmware/app development can
proceed on the DK and breakouts while hardware is prepared. Integration,
recovery, battery safety, and privacy require explicit verification. Scope
selection does not establish performance or validate the prototype.

## References

- [Architecture](../architecture.md)
- [Validation and audio privacy requirements](../validation.md)
- [PR #2](https://github.com/zgeez/smart-led-badge/pull/2)
