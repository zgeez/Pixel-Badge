# 0011: Visible voice recording streamed to the phone app

- Status: Accepted
- Date: 2026-10-11
- Deciders: Project owner

## Context

[0001](0001-rev-a-product-scope.md) set audio as visualization only, with no
stored microphone audio. The owner now wants the badge to record voice for
playback in the companion app. Recording near other people raises privacy
concerns, so it has to be obvious when it happens, and the badge's own storage
should stay free of audio. The nRF5340's internal flash (1 MB, shared with the
firmware) would hold only seconds of raw audio anyway.

## Options considered

1. Where audio is held:
   - Stream live to the phone app, which stores the recording; nothing kept on
     the badge. Keeps flash, settings, logs and dumps audio-free.
   - Record on the badge, then transfer. Needs flash space the badge does not
     have and puts audio in persistent storage.
2. Transport:
   - A custom GATT stream over the encrypted, bonded link, compressed. Works
     with any BLE phone and keeps the app simple.
   - Bluetooth LE Audio (LC3), which the nRF Connect SDK supports on the
     nRF5340. Needs an LE Audio capable phone and more app work.

## Decision

Voice recording is allowed, visibly and without storing audio on the badge:

- A recording starts only from the paired app or a button long-press.
- Recording is its own display mode: the matrix shows REC and a live level bar
  for the whole recording. It cannot run behind another mode.
- Audio is streamed live, compressed with IMA ADPCM at 16 kHz (about
  64 kbit/s), over the encrypted, bonded BLE link to the paired app, which
  stores the recording.
- A recording stops when the link drops, when the display is turned off or
  blanked, or at the maximum length set in the settings.
- Raw samples exist on the badge only in volatile buffers; none reach flash,
  NVS/settings, logs or crash dumps.

This supersedes the "no stored microphone audio" part of 0001. Audio
visualization remains as decided there.

## Consequences

The firmware needs a recording mode, an audio GATT service with ADPCM
encoding, and the stop conditions above; the app needs recording and playback
screens. Recording is new scope and must not delay the minimum viable demo.
The audio privacy review (validation section 8) now covers transmission: no
audio is sent without the recording mode shown, the mode cannot be left or
hidden while recording, and every stop condition works.

## References

- [Firmware: audio privacy requirement](../firmware.md#audio-privacy-requirement)
- [Validation: microphone and audio processing](../validation.md#8-microphone-and-audio-processing)
