# 0002: nRF5340 module and NCS/Zephyr platform

Status: Accepted; module superseded by [0015](0015-nrf52840-e73-module.md)

Date: 2026-10-04

## Decision

Start with the nRF5340 DK and a Raytac MDBT53-P1M module, using NCS 3.4.1,
Zephyr, C++17 application logic, and C interfaces where needed. A module avoids
a bare-SoC RF design while retaining BLE, USB, and hardware display timing.

Use Zephyr synchronization, no exceptions, and bounded application storage.
RTTI/dynamic allocation need a specific justification. Pin mappings belong in
board/devicetree files. Plan an external 32.768 kHz crystal and follow the
module's power, footprint, and antenna guidance.

Availability later drove the nRF52840 module change. The SDK/language policy
and separate DK development target remain.
[Firmware](../firmware.md) · [Module sources](../references.md)
