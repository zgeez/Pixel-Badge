# nRF5340 prototype

This is the existing DK + HUB75 + external NTAG/IMU/microphone application,
renamed explicitly to distinguish it from the E73 nRF52840 Rev A board. Shared
logic lives in `../common`; both application and network-core images remain.

This is the Prototype program ([firmware targets](../README.md)): the BLE build
with the network-core controller.

```powershell
./scripts/dev.ps1 build -Image Prototype -Pristine
./scripts/dev.ps1 build -Image Prototype -BuildDir build/prototype-recording -ExtraConfFile firmware/nrf5340_prototype/configs/audio-recording-bench.conf -Pristine

# Program the DK: network core, then application core, then a pin reset.
./scripts/dev.ps1 flash -Image Prototype
./scripts/dev.ps1 flash -Image Prototype -BuildDir build/prototype-recording
```

`flash` refuses unless the DK's debugger identifies the nRF5340 (with a board
on the debug-out cable it would target that board instead) and the build is an
nrf5340dk build. It erases only the ranges the images touch and refuses an
application image that reaches past the primary slot (0x60000), so the artwork,
network-update and settings partitions, including the bond, are never erased.
It does not back up settings; make a backup first when the stored state matters.
`dev.ps1 rtt -Image Prototype` logs RTT from the application core.

The Prototype program builds to `build/prototype`. `-Target Prototype` without
`-Image` still builds the application without BLE to `build/nrf5340dk`. The inner application image directory is
now `nrf5340_prototype/zephyr`, replacing `smart_led_badge/zephyr`; provisioning
ELF paths and CI artifacts follow that name. Rebuild pristine after the move.
Existing flash offsets, artwork/settings formats and bond policy are retained.

`boards/smart_led_badge` is the earlier nRF5340 custom-board design, retained
for compile regression only. It does not describe current Rev A hardware.
Never flash an nRF52840 image to this DK.

The merged app 0.10.3/build 31 and matching pre-split prototype recording image
were installed with settings/bond retention checks. Later port edits have only
been built/tested, not flashed. Final functional checks are deferred to Rev A
bring-up during the port; this does not mark them passed.
