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
  - the companion app is subscribed to the audio stream and has proved the
    app key (below) on this connection; a start from the app also carries a
    valid app-key authenticator of its own;
  - the LED rail is on (not shed for low battery) and the matrix driver is
    running.

  Otherwise the badge does not record, and after a long-press the matrix, if
  it is on, briefly shows that recording is unavailable.
- Recording is its own display mode: the matrix shows REC and a live level bar
  for the whole recording. REC uses a fixed recording brightness, whatever
  the brightness setting, set by the visibility test in validation section 8
  (the badge has no ambient light sensor). The mode cannot run behind another
  mode.
- Capture for a recording starts only after the matrix driver confirms the
  REC frame is being shown. Samples captured before that, including any from
  a visualization mode, are discarded and never encoded or sent.
- Audio is streamed live, compressed with IMA ADPCM at 16 kHz (about
  64 kbit/s), over the encrypted, bonded BLE link to the paired app.
- The app keeps recordings only in its private, encrypted storage, excluded
  from cloud and device backups, never in shared media or file storage. A
  recording leaves the app only when the user explicitly exports or shares
  it.
- A Bluetooth bond trusts the phone, not one app on it, so the companion app
  is authenticated as well, with an app key:
  - Enrollment is confirmed on the badge. The key is set up only while
    pairing mode, started on the badge, is open: the app and the badge run a
    key exchange, both show a short code derived from it, and the user accepts
    with a button press on the badge only if the codes match. The badge takes
    one enrollment per pairing window and cancels the window if a second app
    tries. The badge keeps the key with its Bluetooth bond keys, the app in
    the phone's secure key store; pairing again or a factory reset replaces
    it.
  - Every recording-control command from the app (start, stop, recording
    settings) carries an authenticator computed with the app key over the
    command and a fresh counter, so a command from another app on the same
    connection, or a replayed one, is refused.
  - Each recording is encrypted and authenticated with a key used for that
    recording only, so another app on the phone cannot read it.
  - No key is ever logged. The protocol is defined with the audio GATT
    service.
- The user stops a recording with a button press or from the app. It also
  stops when the link drops or the app unsubscribes, when the display is
  turned off or blanked, when the LED rail is shed for low battery or the
  matrix driver faults, or at the maximum length set in the settings.
  Before REC leaves the matrix or the LED rail turns off, capture stops and
  any audio not yet sent is discarded. If the matrix driver faults, capture
  stops at once and all queued audio is discarded, not sent.
- Audio data, raw samples and the encoded ADPCM stream alike (before and
  after encryption), exists on the badge only in volatile buffers (capture,
  encoding and Bluetooth transmit queues) and is discarded when the recording
  ends; none reaches flash, NVS/settings, logs, telemetry or crash dumps.

This supersedes the "no stored microphone audio" part of 0001. Audio
visualization remains as decided there.

## Consequences

The firmware needs a recording mode, an audio GATT service with ADPCM
encoding, app-key enrollment with confirmation on the badge, authenticated
control commands, per-recording encryption, and the stop conditions above;
the app needs the matching key handling, private storage for recordings, and
recording and playback screens.
Recording is new scope and must not delay the minimum viable demo. The audio
privacy review (validation section 8) now covers encoded audio and
transmission: no audio is captured for a recording before REC is shown or
after it is removed, REC is readable in the intended wear conditions, a
recording cannot start without the authenticated companion app or while the
indicator cannot be shown, no other app can enroll, control or read a
recording, the mode cannot be left or hidden while recording, and the manual
stop and every automatic stop condition work.

## References

- [Firmware: audio privacy requirement](../firmware.md#audio-privacy-requirement)
- [Validation: microphone and audio processing](../validation.md#8-microphone-and-audio-processing)
