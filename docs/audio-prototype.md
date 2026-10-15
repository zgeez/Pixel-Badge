# P3 audio prototype

Firmware and Android app software are implemented on the nRF5340 DK/HUB75
fixture. Hardware/phone acceptance remains pending. Default P3 firmware permits
local Level/Spectrum visualization but **disables recording**. Only the explicit
recording bench fragment enables it. The nRF52840 port remains after prototype
firmware work.

## Behavior and boundaries

Mode short cycles QR, Text, available Drawing/Animation, Level and Spectrum.
Capture is nominal32kHz stereo with a selected lane, four static20ms DMA blocks,
200ms startup discard, DC-removed RMS and a256-point Q15 Hann FFT. PDM stops and
buffers are cleared outside audio modes. Actual sample/PDM clock and lane must
be measured; these meters are not calibrated sound-pressure measurements.

Action (Button2) long opens the normal Bluetooth pairing window. Once paired,
reconnect in Pixel Badge: audio setup is automatic and the Audio page shows
Ready to record. There is no separate enrollment or recording-length selector.
[ADR0016](decisions/0016-simple-audio-controls.md) records the owner amendment.
After a failed settings Apply, Refresh before selecting a visualization.

Recording starts from the app, or Mode (Button1) long requests a fresh accept
from the foreground subscribed app within3s. Otherwise NO REC appears. Either
badge button or app Stop stops recording. REC is a steady large label with a
live level bar, at fixed DK brightness255 (candidate, not visibility acceptance).
Capture starts only after the matrix driver confirms a REC buffer swap and full
scan. Prior visualization DMA is stopped/erased. A31-tap anti-alias FIR precedes
2:1 decimation to nominal16kHz; independent10ms IMA ADPCM blocks are encrypted
and streamed, never stored on the badge. A gentle30Hz DC/rumble filter,
automatic gain (up to24dB) and peak limiting run before compression so playback
and exports both benefit. Quiet ambience is not gated. The raw local level/FFT
measurements remain uncalibrated; recording gain is not acoustic sensitivity.
New settings need the same-position/distance listening comparison on hardware.

Controls require LESC/MITM level4 plus automatic session proof and replay protection. Start
also needs subscription, MTU>=108, healthy display and the explicit bench gate.
Link loss, unsubscribe,1.5s app-heartbeat timeout, display/scan fault stop/discard capture. Recording continues until Stop, phone storage is low
(16MiB reserve), or the WAV file boundary (about37.28hours). Private recordings
use about40.32MB/hour; exported WAV uses115.2MB/hour. These are format sizes,
not validated recording endurance. Stop
waits boundedly for DMA release and zero-rate disable before erasing the pool;
unconfirmed stop resets instead of claiming cleanup. No audio or keys in logs,
telemetry or dumps. Settings cannot replace REC while recording.

Android wraps one fresh recording-file key with Keystore before capture, then
uses local chunk encryption in noBackupFilesDir (older files remain readable);
backups are disabled. Plaintext/keys never cross Flutter's channel. Playback
processes one block at a time. Delete removes the private file. Explicit Export
WAV uses the document picker; the user may share that export. Leaving/locking
the app stops recording; no automatic resume exists.

**The externally powered HUB75 fixture has no panel power-good feedback.**
Software blank/scan faults are detectable; unplugging its separate supply cannot
be inferred from GPIO scanning. The later Rev A port must wire LED-rail and
low-battery permission/stop handling. Recording remains release-disabled until
that integration and ADR0011 visibility/privacy/security acceptance pass.

## Build/install

Use the existing [bring-up wiring](bringup.md), PDM CLK=P1.07, DATA=P1.09,
correct supply/GND and selected lane. Standalone audio-screening.conf remains
available: Button1 short meter/FFT, long pause; Button2 short brightness, long
lane. Screening starts local capture on boot and has no BLE recording.

```powershell
Set-Location E:\Embedded\Nordic\smart-led-badge\build\prototype-audio
# Default P3, recording disabled:
.\scripts\dev.ps1 build -BuildDir build/p3-normal -SysbuildConfFile firmware/configs/ble-sysbuild.conf
# Explicit development recording bench:
.\scripts\dev.ps1 build -BuildDir build/p3-recording -SysbuildConfFile firmware/configs/ble-sysbuild.conf -ExtraConfFile firmware/configs/audio-recording-bench.conf
west flash -d build/p3-recording --runner jlink --no-erase
.\scripts\app\dev.ps1 build
adb install -r app/build/app/outputs/flutter-apk/app-debug.apk
```

Boot with both buttons released. Preserve working bonds/NFC receipts with
--no-erase; do not factory reset for this bench. Missing .env leaves contact
presets empty; scripts import only four validated public link defines.

## Owner tests, in order

1. **Regression/local audio:** reconnect and Refresh. QR/Text/Drawing/Animation
   still work. Audio -> Level: speak/clap. Spectrum: use a known tone. Switch
   to QR. Expected response/tone band; PDM clock stops outside audio. If silent,
   use screening to test the other lane and report the clock.
2. **Automatic setup:** reconnect using the existing Bluetooth bond. Audio
   shows Ready to record without an enrollment button, extra code or duration
   selector. Reconnect again and confirm it prepares automatically.
3. **Recording/playback:** Record, speak5-10s, Stop. Expected
   steady REC+level throughout; prior mode returns after cleanup; a private
   recording appears and Play sounds right. Export WAV to a chosen location,
   verify playback, Delete the private copy. Nothing automatically in Media.
   In app0.4.0+, pause, drag the seek slider forward/backward, resume and use
   the ten-second skip controls. Progress follows played samples; seeking
   authenticates each selected encrypted block, including legacy files. Also
   seek to the last instant and resume: the tail should finish, not stay playing.
   After a dropped link, reconnect, then repeat Home/reopen to check the service.
4. **Button start:** keep Audio foreground/authenticated; Mode long about1s
   requests a fresh recording. Either button stops. Disconnected app: NO REC.
   Normal firmware refuses both app and button recording.
5. **Automatic stops:** separate recordings for Home, lock, Bluetooth off,
   and disconnect. Expected capture/REC ends; reopening never resumes.
   Home/lock retain BLE; recording and playback stop/pause. Reopen and Refresh
   without reconnecting, and check Back/reopen retains the link too. Bluetooth
   off or badge power-off must end the connection/service. Force-stop still
   ends the process. Play the saved recording; no automatic recording restart.
6. **Sustained transport:** record past five minutes, listen for speed errors,
   gaps/distortion, and check for resets/overruns. Report duration, phone/image
   and non-secret status. Never capture audio/key data in RTT or dumps.
7. **Visibility:** normal wear distance/angles, indoor, shade, bright outdoor
   and direct sun. Record pass/fail/photos. Any failure keeps release recording
   disabled; brightness255 is not proof.

Engineering gates also exercise wrong-MAC/replay, cross-recording packets,
repeat handshakes, low storage/WAV boundary, unsubscribe, software
blank/scan faults, DMA/transport failures and persistence inspection. Use
controlled fault injection, not an unmonitored HUB75 power unplug. Rail shedding
and combined fluid/audio/refresh load need later hardware integration. Owner
checks do not substitute for those gates.

See [wire contract](audio-protocol.md), [ADR0011](decisions/0011-visible-voice-recording.md)
and [software evidence](../validation/results/2026-10-14-p3-audio-start/README.md).
