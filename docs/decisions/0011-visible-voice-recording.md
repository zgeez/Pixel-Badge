# 0011: Visible voice recording streamed to the phone app

Status: Accepted; enrollment/duration amended by [0016](0016-simple-audio-controls.md), display by [0018](0018-grayscale-studio-recording-view.md)

Date: 2026-10-11

## Decision

Allow explicit voice recording by streaming to the phone, keeping audio off
badge flash, settings, logs, and dumps. A live compressed GATT stream fits the
existing BLE app better than adding LE Audio or on-badge recording storage.

Capture requires authenticated LESC pairing, an authorized subscribed app,
healthy LED rail/display, and confirmed presentation of the recording view.
Discard samples captured before that confirmation. The recording view owns
the display until capture/cleanup ends.

Use 16 kHz IMA ADPCM, replay-protected controls, and a fresh authenticated
encryption key for each recording. The phone keeps encrypted private files,
excluded from backups; WAV export/share is explicit. Keys remain out of logs
and dumps.

Stop on a button/app request, lost link/subscription, app failure, or display/
rail fault. Stop capture and discard queued data before removing the view.
Keep recording disabled by default until visibility and stop/security tests pass.

The original design used separate app-key enrollment, REC lettering, and a
duration cap. The linked amendments replace those details while retaining
explicit initiation, visible recording, and volatile-only badge audio.
[Audio protocol](../audio-protocol.md) · [Tests](../testing/sensors-audio.md)
