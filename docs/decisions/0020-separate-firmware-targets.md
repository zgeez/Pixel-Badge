# 0020: Separate prototype and Rev A firmware applications

- Status: Accepted by owner 2026-10-17; hardware acceptance pending
- Date: 2026-10-18
- Related: [0015](0015-nrf52840-e73-module.md), [0019](0019-large-artwork-fixed-workspaces.md)

## Context

The completed prototype uses an nRF5340 DK, HUB75 panel and external sensor/tag
modules. Rev A instead uses E73/nRF52840 with a discrete LED scan chain and
board-specific power circuitry. The owner requested retaining the prototype
in a clearly named firmware space and starting separate Rev A firmware.

## Decision

Use two Zephyr application/sysbuild roots: `firmware/nrf5340_prototype` and
`firmware/rev_a_nrf52840`. Share portable app/services, codecs and drivers under
`firmware/common`; retain shared tests under `firmware/tests`. Board/config/
partition files belong to the corresponding application. Developer commands
explicitly select a target, defaulting to the prototype for existing workflows.

The prototype retains its network-core radio build and storage offsets.
Rev A uses a single application/controller image and a distinct matrix backend
behind the same display interface. Both preserve the BLE settings/content
protocol. The earlier nRF5340 custom-board definition remains a regression
fixture, clearly labeled as superseded hardware.

## Consequences

Shared features do not diverge into copied applications. Builds and artifacts
need the new source/image paths; perform a pristine build after moving roots.
The Rev A port must establish its own resource, peripheral and timing evidence;
prototype validation does not transfer automatically. The initial Rev A image
keeps matrix output disarmed and charging disabled. Power policy and physical
acceptance remain separate follow-up work. Owner deferral of final physical
tests to bring-up is recorded as pending, not as a pass or release approval.
