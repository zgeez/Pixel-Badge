// SPDX-License-Identifier: MIT
import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

const audioServiceUuid = 'f3b00101-6e9b-4f4c-a8f8-731a9d46e1c0';
const audioStateUuid = 'f3b00102-6e9b-4f4c-a8f8-731a9d46e1c0';
const audioControlUuid = 'f3b00103-6e9b-4f4c-a8f8-731a9d46e1c0';
const audioStreamUuid = 'f3b00104-6e9b-4f4c-a8f8-731a9d46e1c0';

abstract interface class AudioTransport {
  String get audioDeviceId;
  Future<Uint8List> readAudioState();
  Future<void> writeAudioControl(Uint8List value);
  Future<void> subscribeAudio();
  Future<void> unsubscribeAudio();
  Stream<Uint8List> get audioPackets;
}

class AudioState {
  AudioState(
    this.flags,
    this.error,
    this.nonce,
    this.recordingNonce,
    this.challenge,
    this.publicKey,
  );
  final int flags, error;
  final Uint8List nonce, recordingNonce, challenge, publicKey;
  bool get keyed => flags & 1 != 0;
  bool get proved => flags & 2 != 0;
  bool get recording => flags & 8 != 0;
  bool get buttonRequest => flags & 16 != 0;
  bool get recordingEnabled => flags & 32 != 0;
  factory AudioState.decode(Uint8List value) {
    if (value.length != 125 || value[0] != 2 || value[1] & ~59 != 0) {
      throw const FormatException('Unsupported audio service.');
    }
    final bytes = ByteData.sublistView(value);
    if (value.sublist(2, 8).any((byte) => byte != 0)) {
      throw const FormatException('Malformed audio status.');
    }
    return AudioState(
      value[1],
      bytes.getInt32(8, Endian.little),
      value.sublist(12, 28),
      value.sublist(28, 44),
      value.sublist(44, 60),
      value.sublist(60, 125),
    );
  }
}

List<Uint8List> audioFragments(Uint8List message) {
  if (message.isEmpty || message.length > 96) {
    throw const FormatException('Invalid audio command.');
  }
  return [
    for (var i = 0; i < message.length; i += 19)
      Uint8List.fromList([
        i,
        ...message.sublist(i, (i + 19).clamp(0, message.length)),
      ]),
    Uint8List.fromList([255]),
  ];
}

Uint8List audioControlBody(int opcode, int counter, int value) {
  if (opcode < 2 ||
      opcode > 7 ||
      opcode == 6 ||
      counter < 1 ||
      counter > 0xffffffff ||
      value < 0 ||
      value > 65535) {
    throw const FormatException('Invalid audio command.');
  }
  return (ByteData(7)
        ..setUint8(0, opcode)
        ..setUint32(1, counter, Endian.little)
        ..setUint16(5, value, Endian.little))
      .buffer
      .asUint8List();
}

class PlaybackState {
  const PlaybackState({
    this.id,
    this.positionMs = 0,
    this.durationMs = 0,
    this.playing = false,
    this.error,
  });
  final String? id, error;
  final int positionMs, durationMs;
  final bool playing;
  factory PlaybackState.decode(Map<Object?, Object?> value) => PlaybackState(
    id: value['id'] as String?,
    positionMs: (value['positionMs'] as num? ?? 0).toInt(),
    durationMs: (value['durationMs'] as num? ?? 0).toInt(),
    playing: value['playing'] == true,
    error: value['error'] as String?,
  );
}

class AudioController extends ChangeNotifier {
  AudioController({MethodChannel? channel})
    : native = channel ?? const MethodChannel('com.ziadg.badge_app/audio');
  final MethodChannel native;
  AudioTransport? transport;
  AudioState? state;
  Timer? timer, mediaTimer;
  PlaybackState playback = const PlaybackState();
  final recordingWatch = Stopwatch();
  int playbackEpoch = 0;
  bool playbackPolling = false;
  void mediaClock() {
    mediaTimer ??= Timer.periodic(const Duration(milliseconds: 200), (_) {
      if (disposed) return;
      if (playback.playing) unawaited(pollPlayback());
      if (receiving) changed();
      if (!receiving && !playback.playing) {
        mediaTimer?.cancel();
        mediaTimer = null;
      }
    });
  }

  Future<void> pollPlayback() async {
    if (playbackPolling || disposed) return;
    playbackPolling = true;
    final epoch = playbackEpoch;
    try {
      final value = await native.invokeMapMethod<Object?, Object?>(
        'playbackState',
      );
      if (value != null && epoch == playbackEpoch && !disposed) {
        playback = PlaybackState.decode(value);
        changed();
      }
    } catch (_) {
    } finally {
      playbackPolling = false;
    }
  }

  Future<void> playbackAction(
    String method, [
    Map<String, Object?>? arguments,
  ]) async {
    final epoch = ++playbackEpoch;
    try {
      final value = await native.invokeMapMethod<Object?, Object?>(
        method,
        arguments,
      );
      if (epoch != playbackEpoch || disposed) return;
      playback = value == null
          ? const PlaybackState()
          : PlaybackState.decode(value);
      mediaClock();
      changed();
    } catch (_) {
      message = 'Could not play this recording.';
      changed();
    }
  }

  Future<void> play(String id) async {
    if (!busy && !receiving) await playbackAction('play', {'id': id});
  }

  Future<void> togglePlayback() =>
      playbackAction(playback.playing ? 'pausePlayback' : 'resumePlayback');
  Future<void> seek(int ms) => playbackAction('seekPlayback', {
    'positionMs': ms.clamp(0, playback.durationMs),
  });
  Future<void> stopPlayback() => playbackAction('stopPlayback');
  StreamSubscription<Uint8List>? subscription;
  Future<void> controls = Future.value(), packets = Future.value();
  int counter = 0, queued = 0, generation = 0, operationEpoch = 0;
  bool busy = false,
      receiving = false,
      polling = false,
      foreground = true,
      disposed = false;
  String message = 'Connect a badge with P3 firmware to use audio.';
  int? _lastDiagnosticFlags, _lastDiagnosticError;
  void updateState(AudioState value) {
    state = value;
    assert(() {
      if (_lastDiagnosticFlags != value.flags ||
          _lastDiagnosticError != value.error) {
        debugPrint(
          'Pixel Badge audio: flags=${value.flags}, result=${value.error}',
        );
        _lastDiagnosticFlags = value.flags;
        _lastDiagnosticError = value.error;
      }
      return true;
    }());
  }

  String? deviceStopReason(int error) =>
      error == 0 ? null : 'The badge stopped recording (status $error).';
  List<Map<Object?, Object?>> recordings = [];
  bool get available => transport != null && state != null;
  bool get authenticated => state?.proved == true;
  bool get recording => receiving;
  bool get android =>
      defaultTargetPlatform == TargetPlatform.android && !kIsWeb;
  void changed() {
    if (!disposed) notifyListeners();
  }

  Future<void> refreshLibrary() async {
    // Directory scans share native capture I/O. Never queue one during capture.
    if (!android || receiving) return;
    final list = await native.invokeListMethod<Object?>('list') ?? [];
    recordings = list
        .map((value) => Map<Object?, Object?>.from(value as Map))
        .toList();
    changed();
  }

  Future<void> attach(AudioTransport? link) async {
    await detach();
    if (link == null || !android) return;
    final session = ++generation;
    transport = link;
    counter = 0;
    busy = true;
    changed();
    try {
      updateState(AudioState.decode(await link.readAudioState()));
      if (session != generation) return;
      final pub = await native.invokeMethod<Uint8List>('beginSession');
      if (session != generation || pub == null) return;
      await send(Uint8List.fromList([1, ...pub]));
      final reply = AudioState.decode(await link.readAudioState());
      if (session != generation) return;
      if (!reply.keyed) throw StateError('Reconnect the badge.');
      updateState(reply);
      await native.invokeMethod<void>('deriveSession', {
        'public': reply.publicKey,
        'nonce': reply.nonce,
      });
      if (session != generation) return;
      await control(2);
      if (session != generation) return;
      if (!authenticated) throw StateError('Reconnect the badge.');
      await link.subscribeAudio();
      message = 'Ready to record.';
      timer = Timer.periodic(
        const Duration(milliseconds: 400),
        (_) => unawaited(poll()),
      );
    } catch (_) {
      message = 'Audio connection failed. Reconnect the badge.';
    } finally {
      busy = false;
      changed();
    }
  }

  Future<void> send(Uint8List value) async {
    final link = transport ?? (throw StateError('Reconnect the badge.'));
    for (final fragment in audioFragments(value)) {
      await link.writeAudioControl(fragment);
    }
  }

  Future<void> control(int opcode, {int value = 0, int? expectedEpoch}) {
    final link = transport, snapshot = state, session = generation;
    if (link == null || snapshot == null) {
      return Future.error(StateError('Reconnect the badge.'));
    }
    final task = controls.catchError((Object _) {}).then((_) async {
      if (expectedEpoch != null) {
        checkStart(link, session, expectedEpoch);
      }
      if (session != generation || counter == 0xffffffff) {
        throw StateError('Reconnect the badge.');
      }
      final signed = await native.invokeMethod<Uint8List>('control', {
        'device': link.audioDeviceId,
        'body': audioControlBody(opcode, ++counter, value),
        'nonce': snapshot.nonce,
        if (opcode == 5) 'challenge': snapshot.challenge,
      });
      if (signed == null || session != generation) {
        throw StateError('Audio session ended.');
      }
      if (expectedEpoch != null) {
        checkStart(link, session, expectedEpoch);
      }
      await send(signed);
      final result = AudioState.decode(await link.readAudioState());
      if (session == generation) updateState(result);
    });
    controls = task;
    return task;
  }

  Future<void> poll() async {
    if (polling || busy || stopping || transport == null || !foreground) return;
    polling = true;
    final session = generation, epoch = operationEpoch;
    try {
      final result = AudioState.decode(await transport!.readAudioState());
      if (session != generation ||
          epoch != operationEpoch ||
          busy ||
          stopping) {
        return;
      }
      updateState(result);
      if (receiving) {
        if (!result.recording) {
          await stop(reason: deviceStopReason(result.error));
        } else {
          await control(7);
        }
      } else if (result.buttonRequest &&
          result.proved &&
          !busy &&
          result.recordingEnabled) {
        await start(fromButton: true);
      }
    } catch (_) {
      if (session == generation) {
        await stop(reason: 'Audio connection failed; recording stopped.');
      }
    } finally {
      polling = false;
      changed();
    }
  }

  Future<void> start({bool fromButton = false}) async {
    if (busy ||
        receiving ||
        stopping ||
        _detaching != null ||
        !authenticated ||
        !foreground) {
      return;
    }
    final epoch = ++operationEpoch;
    busy = true;
    changed();
    final session = generation;
    final link = transport!;
    final setup = _start(link, session, epoch, fromButton: fromButton);
    _starting = setup;
    try {
      await setup;
    } catch (error) {
      // A stop/detach already owns cleanup for a cancelled operation. It waits
      // for setup to settle before closing the native file and subscription.
      if (session == generation && epoch == operationEpoch) {
        final reason = error is StateError
            ? error.message
            : error is PlatformException && error.code == 'capacity'
            ? error.message
            : 'Recording could not start. Reconnect.';
        await stop(reason: reason);
      }
    } finally {
      if (identical(_starting, setup)) _starting = null;
      busy = false;
      changed();
    }
  }

  Future<void>? _starting;
  void checkStart(AudioTransport link, int session, int epoch) {
    if (disposed ||
        session != generation ||
        epoch != operationEpoch ||
        !identical(link, transport) ||
        !foreground ||
        stopping ||
        _detaching != null) {
      throw StateError('Recording cancelled.');
    }
  }

  Future<void> _start(
    AudioTransport link,
    int session,
    int epoch, {
    required bool fromButton,
  }) async {
    if (state?.recordingEnabled != true) {
      throw StateError(
        'Recording is disabled until REC visibility is accepted.',
      );
    }
    await stopPlayback();
    checkStart(link, session, epoch);
    await link.subscribeAudio();
    checkStart(link, session, epoch);
    // File/Keystore setup must finish before asking the badge to capture.
    await native.invokeMethod<void>('prepareRecording');
    checkStart(link, session, epoch);
    // Listen before requesting start. The bounded queue covers phone scheduling;
    // firmware also discards the first 200 ms of this fresh capture.
    receiving = true;
    recordingWatch
      ..reset()
      ..start();
    mediaClock();
    queued = 0;
    packets = Future.value();
    final opened = Completer<void>();
    subscription = link.audioPackets.listen(
      (packet) {
        if (!receiving || session != generation) return;
        if (++queued > 32) {
          queued--;
          unawaited(
            stop(reason: 'Phone could not keep up; recording stopped.'),
          );
          return;
        }
        packets = packets
            .then((_) async {
              await opened.future;
              if (receiving && session == generation) {
                await native.invokeMethod<void>('packet', packet);
              }
            })
            .catchError((Object error) {
              unawaited(
                stop(
                  reason: error is PlatformException && error.code == 'capacity'
                      ? error.message ??
                            'Phone storage is low; recording stopped.'
                      : 'Audio stream or storage failed; recording stopped.',
                ),
              );
            })
            .whenComplete(() => queued--);
      },
      onError: (Object _) =>
          unawaited(stop(reason: 'Audio stream failed; recording stopped.')),
    );
    try {
      await control(fromButton ? 5 : 3, expectedEpoch: epoch);
      checkStart(link, session, epoch);
      await native.invokeMethod<void>('beginRecording', {
        'device': link.audioDeviceId,
        'nonce': state!.recordingNonce,
      });
      checkStart(link, session, epoch);
      opened.complete();
    } catch (_) {
      opened.complete();
      rethrow;
    }
    message = 'Recording — REC must remain visible on the badge.';
  }

  Future<void> finishLocal({String? reason}) async {
    receiving = false;
    recordingWatch.stop();
    await subscription?.cancel();
    subscription = null;
    await packets;
    final saved = await native.invokeMethod<String>('finishRecording');
    try {
      await transport?.unsubscribeAudio();
    } catch (_) {}
    if (foreground &&
        !disposed &&
        _detaching == null &&
        authenticated &&
        transport != null) {
      try {
        await transport!.subscribeAudio();
      } catch (_) {}
    }
    message =
        reason ??
        (saved != null
            ? 'Recording stopped. Saved privately on this phone.'
            : 'Recording stopped. No recording was saved.');
    await refreshLibrary();
  }

  Future<void>? _stopping;
  bool get stopping => _stopping != null;
  Future<void> stop({String? reason}) =>
      _stopping ??= _stop(reason: reason).whenComplete(() {
        _stopping = null;
        changed();
      });
  Future<void> _stop({String? reason}) async {
    operationEpoch++;
    try {
      if (transport != null && authenticated) {
        try {
          await control(4);
        } catch (_) {}
      }
      // Native preparation may still be opening a file. Stop capture promptly,
      // then wait so no file or receiver can appear after finalization.
      try {
        await _starting;
      } catch (_) {}
      await finishLocal(reason: reason);
    } catch (_) {
      message = 'Recording stopped; the phone could not save it.';
    }
  }

  Future<void> libraryAction(String method, String id) async {
    if (busy || receiving) return;
    if (method == 'play') {
      await play(id);
      return;
    }
    busy = true;
    changed();
    try {
      await stopPlayback();
      await native.invokeMethod<void>(method, {'id': id});
      await refreshLibrary();
    } catch (_) {
      message = 'Could not $method this recording.';
    } finally {
      busy = false;
      changed();
    }
  }

  Future<void>? _detaching;
  Future<void> detach() =>
      _detaching ??= _detach().whenComplete(() => _detaching = null);
  Future<void> _detach() async {
    timer?.cancel();
    timer = null;
    // Drain an idle resubscribe before stop removes the old link's listener.
    await _resuming;
    await stop();
    generation++;
    transport = null;
    state = null;
    try {
      await native.invokeMethod<void>('endSession');
    } catch (_) {}
    changed();
  }

  void background() {
    foreground = false;
    unawaited(playbackAction('pausePlayback'));
    unawaited(stop());
  }

  Future<void>? _resuming;
  Future<void> resume() {
    foreground = true;
    return _resuming ??= _resume().whenComplete(() => _resuming = null);
  }

  Future<void> _resume() async {
    final link = transport, session = generation;
    await _stopping;
    if (disposed ||
        !foreground ||
        session != generation ||
        !identical(link, transport) ||
        link == null ||
        !authenticated ||
        _detaching != null) {
      return;
    }
    try {
      await link.subscribeAudio();
    } catch (_) {
      if (session == generation && foreground && !disposed) {
        message = 'Audio connection failed. Reconnect the badge.';
        changed();
      }
    }
  }

  @override
  void dispose() {
    disposed = true;
    timer?.cancel();
    mediaTimer?.cancel();
    unawaited(stopPlayback());
    unawaited(detach());
    super.dispose();
  }
}
