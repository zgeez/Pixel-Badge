# 0002: nRF5340 module and NCS/Zephyr platform

- Status: Proposed
- Date: 2026-10-04
- Deciders: Project owner

## Context

The badge needs BLE, hardware-timed display control, sensors, and USB data,
with a reproducible firmware environment. Development currently uses the
nRF5340 DK; custom-board firmware and hardware have not been validated.

## Options considered

1. Bare nRF5340: permits a custom RF layout but adds RF/layout and assembly
   work beyond the selected module architecture.
2. nRF5340 radio module with NCS/Zephyr: retains the DK platform and vendor
   module reference guidance; introduces footprint, keep-out, and sourcing
   constraints.

## Decision

Use the nRF5340 via a radio module, with Raytac MDBT53-P1M as the preferred
badge module and the nRF5340 DK for development. Use nRF Connect SDK v3.4.1 /
Zephyr and C++17 for application/domain logic, with C-compatible Zephyr
interfaces where needed. Use Zephyr synchronization, disable exceptions, and
require justification for RTTI or runtime dynamic allocation. Plan an external
32.768 kHz crystal; select the part/load configuration from primary guidance.

## Consequences

Module procurement is open; it must be checked before hardware release. Board
pins and peripherals belong in devicetree/board definitions, not DK assumptions.
Follow Raytac Ver.E and its 241021 guide for power, USB, land pattern, clocks,
and RF placement. Circuit values, pin allocation, timing, and clock behavior
still require schematic review and target measurements. The build/test
environment is pinned; compilation does not prove hardware correctness.

## References

- [Firmware platform and policies](../firmware.md)
- [Hardware preparation](../hardware.md)
- [Versioned Nordic and Raytac specifications](../references.md)
