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

- A recording starts only from the paired app or a button long-press, and
  only when all of these hold:
  - the phone is connected with an authenticated bond: LE Secure Connections
    pairing with MITM protection (the passkey shown on the matrix). The audio
    and recording-control characteristics refuse links whose bond is
    unauthenticated, such as Just Works;
  - the app has proved, on this connection, that it holds the app key (below)
    and is subscribed to the audio stream;
  - the LED rail is on (not shed for low battery) and the matrix driver is
    running.

  Otherwise the badge does not record, and after a long-press the matrix, if
  it is on, briefly shows that recording is unavailable.
- Recording is its own display mode: the matrix shows REC and a live level bar
  for the whole recording, at a minimum visible brightness whatever the
  brightness setting. Audio is sent only once REC is on the matrix. The mode
  cannot run behind another mode.
- Audio is streamed live, compressed with IMA ADPCM at 16 kHz (about
  64 kbit/s), over the encrypted, bonded BLE link to the paired app, which
  stores the recording.
- A Bluetooth bond trusts the phone, not one app on it, so the companion app
  is authenticated as well. At pairing the app and the badge set up a shared
  app key; the badge keeps it with its Bluetooth bond keys, the app in the
  phone's secure key store. Recording control needs a fresh challenge and
  response with that key on every connection, and the audio is encrypted and
  authenticated with a key used for that recording only, so another app on the
  same phone can neither start a recording nor read one. Pairing again or a
  factory reset replaces the app key. No key is ever logged. The protocol is
  defined with the audio GATT service.
- The user stops a recording with a button press or from the app. It also
  stops when the link drops or the app unsubscribes, when the display is
  turned off or blanked, when the LED rail is shed for low battery or the
  matrix driver faults, or at the maximum length set in the settings.
- Audio data, raw samples and the encoded ADPCM stream alike (before and
  after encryption), exists on the badge only in volatile buffers (capture, encoding and Bluetooth transmit
  queues) and is discarded when the recording ends; none reaches flash,
  NVS/settings, logs, telemetry or crash dumps.

This supersedes the "no stored microphone audio" part of 0001. Audio
visualization remains as decided there.

## Consequences

The firmware needs a recording mode, an audio GATT service with ADPCM
encoding, the app key with its challenge and per-recording encryption, and the
stop conditions above; the app needs the matching key handling and recording
and playback screens. Recording is new scope and must not delay the minimum viable demo.
The audio privacy review (validation section 8) now covers encoded audio and
transmission: no audio is sent without the recording mode shown, a recording
cannot start without an authenticated app holding the app key or while the
indicator cannot be shown, the mode cannot be left or hidden while recording,
and the manual stop and every automatic stop condition work.

## References

- [Firmware: audio privacy requirement](../firmware.md#audio-privacy-requirement)
- [Validation: microphone and audio processing](../validation.md#8-microphone-and-audio-processing)
