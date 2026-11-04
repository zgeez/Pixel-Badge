# Sensor and audio tests

Note firmware and app versions, configuration, instruments, where the raw data
is, and the result.

## IMU

Check identity, units and axes, data-ready interrupts and sample freshness.
Use the [motion checks](../motion-prototype.md) for tilt, flat and diagonal
poses, manual rotation, the QR orientation lock, saved settings and sensor
loss. Measure processing time, stack use and the shared I²C bus with BLE,
audio and NFC running. A registered interrupt doesn't prove it fires.

## Microphone and audio

- Check the PDM clock and rate, the start-up discard, level and FFT response
  to known signals, and that capture stops outside audio modes.
- The recording view must be on screen before the first sample goes out.
- Recording must be refused without an authenticated link, session proof and
  subscription, a fresh button accept, or a healthy display and LED rail.
- Try wrong MACs, replayed counters, old sessions, packets from another
  recording, repeated handshakes and malformed data.
- Stop it every way: buttons, the app, Home or lock, unsubscribing, link
  loss, low phone storage, the WAV size limit, a display fault, load shedding.
- Play, seek, export and delete; interrupt a file; record past five minutes.
  Known waveforms show rate, order and gaps.
- Trace samples and keys through normal, error and debug paths. None may
  reach badge flash, settings or logs. Use synthetic audio for leak tests.

The view shows REC above a level bar and stays lit in silence. Check that
someone who doesn't know the badge can tell it's recording from 1 and 2 m,
head-on, 45° to the side and 30° above, indoors, in shade and in sun. The
proposed rule is 10 of 10 within 2 s per condition. Recording stays off by
default until that passes.

[Audio bench](../audio-prototype.md), [wire format](../audio-protocol.md).
