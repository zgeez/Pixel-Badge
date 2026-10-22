# Sensor and audio tests

Record firmware/app versions, configuration, instruments, raw-data location,
and pass/fail. Sensor and recording behavior still need Rev A measurements.

## 7. IMU

Verify identity, sample units/orientation, data-ready interrupts, and freshness.
Use [motion checks](../motion-prototype.md) for four-way tilt, flat/diagonal
poses, manual rotation, QR entry lock, settings persistence, and sensor loss.
Measure fetch/processing time, stack peaks, and shared-I²C behavior under
BLE/audio/NFC load. A registered interrupt callback does not prove delivery.

## 8. Microphone and audio processing

- Check PDM clock/rate/lane, startup discard, level/FFT response with known
  signals, and capture stopping outside audio modes. Record gain/noise limits.
- Confirm the recording view is presented before the first sent sample; prior
  visualization samples must be discarded. Compare frame/capture timestamps.
- Refuse recording without authenticated LESC, session proof/subscription,
  a fresh button-start acceptance, or a healthy display/LED rail.
- Test wrong MACs, replayed counters, old sessions, cross-recording packets,
  repeat handshakes, sequence limits, and malformed data. Bond trust does not
  isolate one app from every other app on the same phone.
- Exercise button/app stop, Home/lock, unsubscribe, heartbeat/link failure,
  low storage, WAV boundary, display blank/fault, rail shedding, and DMA/transport
  failure. Queued samples must be discarded before the view disappears.
- Test playback/seek/export/delete, interrupted files, and a recording beyond
  five minutes. Use known waveforms to check rate, order, continuity, and loss.
- Trace raw/encoded samples and keys through normal/error/debug paths. No audio
  or keys may reach badge flash, settings, logs, telemetry, or persistent dumps.
  Phone files stay encrypted/private and excluded from backups; export is explicit.
  Use synthetic samples for leakage tests, not retained microphone recordings.

The current view is spectrum/level plus lit corners in silence
([ADR 0018](../decisions/0018-grayscale-studio-recording-view.md)). Test whether
an uninformed observer can identify recording at 1/2 m, head-on, 45° sideways,
and 30° above, indoors, shade, and direct sun. Record illuminance. Proposed
rule: 10/10 correct within 2 s per condition; this is not yet an accepted limit.
Any failed condition keeps default recording disabled.

[Audio bench](../audio-prototype.md) · [Wire contract](../audio-protocol.md)
