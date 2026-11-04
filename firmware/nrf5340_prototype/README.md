# nRF5340 prototype

The DK bench: nRF5340 DK, HUB75 panel, and breakouts for the NFC tag, IMU and
microphone. Shared code is in `../common`; the DK keeps its separate network
core image for the radio.

```powershell
./scripts/dev.ps1 build -Image Prototype -Pristine
./scripts/dev.ps1 flash -Image Prototype     # network core, app core, then reset
./scripts/dev.ps1 rtt -Image Prototype
```

Add `configs/audio-recording-bench.conf` with its own `-BuildDir` for
recording ([audio](../../docs/audio-prototype.md)).

`flash` only runs when the debugger sees the DK's own nRF5340, and only
erases what the images cover. Artwork, settings and the bond survive. It
doesn't back them up, so do that first if they matter. Never flash an
nRF52840 image to the DK.

`boards/smart_led_badge` is an earlier custom-board design, kept only for
compile checks. [Wiring](../../docs/prototype-wiring.md).
