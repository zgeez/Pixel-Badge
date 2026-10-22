# 0020: Separate prototype and Rev A firmware applications

Status: Accepted 2026-10-17

Date: 2026-10-18

## Decision

Keep separate Zephyr/sysbuild roots for `firmware/nrf5340_prototype` and
`firmware/rev_a_nrf52840`. Share services, codecs, and drivers in `common/`,
with shared host tests. Board/configuration/partition files stay target-specific.

The DK keeps its network-core controller and HUB75 driver. Rev A combines
application/controller and uses its own matrix backend through the same display
interface. Both use the same app protocol; the old nRF5340 custom board remains
a compile-regression fixture.

This avoids copied service implementations. Rebuild pristine after changing
roots. Rev A needs separate resource/timing evidence; its initial outputs and
charging stay disarmed until staged bring-up.
[Firmware targets](../../firmware/README.md) · [Rev A implementation](../rev-a-firmware.md)
