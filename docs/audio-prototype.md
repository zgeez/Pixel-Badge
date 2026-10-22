# Audio development

Level and Spectrum modes process microphone input locally. Recording is off in
normal builds; the explicit bench configuration streams encrypted audio to the
connected Android app.

## Signal path

The DK captures at nominal 32 kHz, computes DC-removed RMS and a 256-point FFT,
and decimates recording audio to 16 kHz. Rev A captures natively at 16 kHz.
Recording uses independent 10 ms IMA ADPCM blocks, a rumble filter, gain control,
and peak limiting. These meters are not calibrated sound-pressure measurements.

Recording requires an authenticated bond, app subscription/session proof, and
a confirmed display frame. The recording view shows spectrum/level with lit
corners in silence. Audio stays in volatile badge buffers; the phone stores
encrypted files outside backups. WAV export is explicit.

## Build

Run from the repository root. Use the [DK wiring](prototype-wiring.md), including
PDM CLK P1.07 and DATA P1.09.

```powershell
.\scripts\dev.ps1 build -Image Prototype -BuildDir build/prototype-recording -ExtraConfFile firmware/nrf5340_prototype/configs/audio-recording-bench.conf -Pristine
.\scripts\dev.ps1 flash -Image Prototype -BuildDir build/prototype-recording
.\scripts\app\dev.ps1 build
adb install -r app/build/app/outputs/flutter-apk/app-debug.apk
```

Back up settings first. Do not factory reset for this test.

## Bench checks

| Check | Expected result |
|---|---|
| Level/Spectrum, then QR | Sound changes the meters; PDM stops outside audio modes |
| Reconnect the bonded phone | Audio prepares automatically; no extra enrollment |
| Record 5–10 s, stop, play, seek, export | Complete recording, correct speed, usable WAV, no automatic media export |
| Hold Mode with the app ready | Fresh recording request; either button stops it |
| Repeat with normal firmware | App/button recording is refused |
| Home, lock, unsubscribe, link loss | Recording stops; returning never resumes it automatically |
| Return after cleanup, then hold Mode | A new button recording still works |
| Record beyond five minutes | No legacy duration cap, resets, gaps, or overruns |
| Check the recording view in sun/shade/indoors | Recording is visibly identifiable throughout |

Log firmware/app versions, duration, and non-secret status. Never capture audio
or keys in RTT/dumps. Test replay/MAC failures, display faults, DMA stop failure,
low storage, and rail shedding using the [audio test plan](testing/sensors-audio.md#8-microphone-and-audio-processing).

The external HUB75 panel has no power-good feedback. Scan counters cannot detect
its supply being unplugged. Recording remains disabled by default until display
visibility, security, stop paths, and endurance pass on the intended hardware.
[Wire format](audio-protocol.md) ·
[Software record](../validation/results/2026-10-14-p3-audio-start/README.md)
