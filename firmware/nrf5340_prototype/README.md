# nRF5340 prototype

This is the existing DK + HUB75 + external NTAG/IMU/microphone application,
renamed explicitly to distinguish it from the E73 nRF52840 Rev A board. Shared
logic lives in `../common`; both application and network-core images remain.

```powershell
./scripts/dev.ps1 build -Target Prototype -Pristine
./scripts/dev.ps1 build -Target Prototype -BuildDir build/prototype-recording -ExtraConfFile firmware/nrf5340_prototype/configs/audio-recording-bench.conf -Pristine
```

Default output is `build/nrf5340dk`. The inner application image directory is
now `nrf5340_prototype/zephyr`, replacing `smart_led_badge/zephyr`; provisioning
ELF paths and CI artifacts follow that name. Rebuild pristine after the move.
Existing flash offsets, artwork/settings formats and bond policy are retained.

`boards/smart_led_badge` is the earlier nRF5340 custom-board design, retained
for compile regression only. It does not describe current Rev A hardware.
Never flash an nRF52840 image to this DK.

The merged app 0.10.3/build 31 and matching pre-split prototype recording image
were installed with settings/bond retention checks. Later port edits have only
been built/tested, not flashed. Final functional checks are deferred to Rev A
bring-up at the owner's request; this does not mark them passed.
