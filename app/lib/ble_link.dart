// SPDX-License-Identifier: MIT
import 'dart:async';
import 'dart:typed_data' show Endian;
import 'package:flutter/services.dart';
import 'package:flutter/foundation.dart'
    show defaultTargetPlatform, TargetPlatform, kIsWeb;
import 'package:flutter/foundation.dart' show debugPrint;
import 'package:universal_ble/universal_ble.dart';
import 'badge_controller.dart';
import 'protocol.dart';
import 'artwork_codec.dart';
import 'audio_controller.dart';

class UniversalBadgeLink
    implements
        BadgeLink,
        AudioTransport,
        BulkArtworkLink,
        ArtworkTransferLink,
        ArtworkReadProgressLink {
  UniversalBadgeLink._(this.device, this.result, this.capabilities);
  final BleDevice device;
  final BleCharacteristic result;
  final BadgeCapabilities capabilities;
  BleCharacteristic? _audio;
  Uint8List? _artworkBytes, _artworkKey;
  bool _closed = false;
  Future<void>? _closing;
  Future<int>? _artworkMtu;
  void Function(double)? _readProgress;
  @override
  void setArtworkReadProgress(void Function(double)? listener) {
    _readProgress = listener;
  }

  void _checkOpen() {
    if (_closed) throw StateError("Badge disconnected.");
  }

  @override
  void invalidateArtworkCache() {
    _artworkBytes = null;
    _artworkKey = null;
  }

  StreamSubscription<bool>? _connection;
  static int _nextOwner = 0;
  final int _owner = ++_nextOwner;
  static const _background = MethodChannel('com.ziadg.badge_app/connection');
  static bool get _android =>
      !kIsWeb && defaultTargetPlatform == TargetPlatform.android;
  Future<void> keepConnected() async {
    if (!_android) return;
    _connection = device.connectionStream.listen((connected) {
      if (!connected) unawaited(_stopBackground());
    });
    await _background.invokeMethod<void>('start', {'owner': _owner});
    if (!await device.isConnected) {
      await _stopBackground();
      throw StateError('Badge disconnected.');
    }
  }

  Future<void> _stopBackground() async {
    if (_android) {
      try {
        await _background.invokeMethod<void>('stop', {'owner': _owner});
      } catch (_) {}
    }
  }

  @override
  String get audioDeviceId => device.deviceId;
  @override
  Future<Uint8List> readAudioState() async => (await device.getCharacteristic(
    audioStateUuid,
    service: audioServiceUuid,
  )).read();
  @override
  Future<void> writeAudioControl(Uint8List data) async =>
      (await device.getCharacteristic(
        audioControlUuid,
        service: audioServiceUuid,
      )).write(data, withResponse: true);
  @override
  Future<void> subscribeAudio() async {
    final mtu = await device.requestMtu(247);
    if (mtu < 108) {
      throw StateError('This connection cannot carry audio. Reconnect.');
    }
    _audio = await device.getCharacteristic(
      audioStreamUuid,
      service: audioServiceUuid,
    );
    await _audio!.notifications.subscribe();
  }

  @override
  Stream<Uint8List> get audioPackets => _audio!.onValueReceived;
  @override
  Future<void> unsubscribeAudio() async {
    await _audio?.unsubscribe();
    _audio = null;
  }

  static Future<UniversalBadgeLink> connect(
    BleDevice device, {
    void Function(bool? paired)? onBondState,
  }) async {
    try {
      await device.connect();
      await device.discoverServices();
      final info = await (await device.getCharacteristic(
        infoUuid,
        service: serviceUuid,
      )).read();
      final capabilities = BadgeCapabilities.decode(info);
      // OS bond state is a UI hint only. Always perform the existing pairing
      // and authenticated read, regardless of this result.
      bool? paired;
      if (onBondState != null && BleCapabilities.hasSystemPairingApi) {
        try {
          paired = await device.isPaired();
        } catch (_) {}
      }
      onBondState?.call(paired);
      await device.pair(
        pairingCommand: BleCommand(
          service: serviceUuid,
          characteristic: statusUuid,
        ),
        timeout: const Duration(seconds: 60),
      );
      final result = await device.getCharacteristic(
        statusUuid,
        service: serviceUuid,
      );
      await result
          .read(); // Firmware requires authenticated LESC, beyond OS bond status.
      await result.notifications.subscribe();
      final link = UniversalBadgeLink._(device, result, capabilities);
      try {
        await link.keepConnected();
      } catch (_) {
        await link.close();
        rethrow;
      }
      return link;
    } catch (_) {
      try {
        await device.disconnect();
      } catch (_) {
        /* Keep the original connection error. */
      }
      rethrow;
    }
  }

  int _priorityUsers = 0;
  @override
  Future<void> transferPriority(bool active) async {
    if (active) {
      _priorityUsers++;
    } else {
      _priorityUsers = (_priorityUsers - 1).clamp(0, 100);
    }
    if (kIsWeb || defaultTargetPlatform != TargetPlatform.android || _closed) {
      return;
    }
    if ((active && _priorityUsers == 1) || (!active && _priorityUsers == 0)) {
      try {
        await UniversalBle.requestConnectionPriority(
          device.deviceId,
          active
              ? BleConnectionPriority.highPerformance
              : BleConnectionPriority.balanced,
        );
      } catch (_) {
        /* Priority is an optional OS hint; transfers still work. */
      }
    }
  }

  @override
  Future<int> artworkPayloadSize() => _artworkMtu ??= _negotiateArtworkMtu();
  Future<int> _negotiateArtworkMtu() async {
    try {
      final mtu = await device.requestMtu(247);
      assert(() {
        debugPrint('Pixel Badge artwork MTU: $mtu');
        return true;
      }());
      return (mtu - 3).clamp(20, 244);
    } catch (_) {
      return 20;
    }
  }

  @override
  Future<Uint8List> read(String uuid) async {
    _checkOpen();
    if (uuid == BadgeField.artwork.uuid) {
      if (capabilities.hasLargeArtwork) {
        await artworkPayloadSize();
        _checkOpen();
      }
      final characteristic = await device.getCharacteristic(
        uuid,
        service: serviceUuid,
      );
      _checkOpen();
      Future<void> writeCursor(Uint8List value) async {
        _checkOpen();
        await characteristic.write(value, withResponse: true);
        _checkOpen();
      }

      Future<Uint8List> readPage() async {
        _checkOpen();
        final value = await characteristic.read();
        _checkOpen();
        return value;
      }

      Uint8List? key;
      if (capabilities.hasLargeArtwork) {
        await writeCursor(Uint8List.fromList([3]));
        key = await readPage();
        if (key.length != 8) {
          throw const FormatException('Malformed artwork metadata.');
        }
        if (_artworkBytes != null &&
            _artworkKey != null &&
            sameBytes(key, _artworkKey!)) {
          _readProgress?.call(1);
          return _artworkBytes!;
        }
      }
      await transferPriority(true);
      try {
        final readWatch = Stopwatch()..start();
        final bytes = await readArtworkPages(
          writeCursor,
          readPage,
          wide: capabilities.hasLargeArtwork,
          sequential: capabilities.hasEditorV2,
          onProgress: (value) => _readProgress?.call(value),
        );
        assert(() {
          debugPrint(
            'Pixel Badge artwork read: ${bytes.length} bytes / ${readWatch.elapsedMilliseconds} ms',
          );
          return true;
        }());
        if (key != null) {
          final metadata = ByteData.sublistView(key);
          if (metadata.getUint32(0, Endian.little) != bytes.length ||
              metadata.getUint32(4, Endian.little) != artworkCrc32(bytes)) {
            throw const FormatException(
              "Artwork checksum mismatch. Refresh again.",
            );
          }
          await writeCursor(Uint8List.fromList([3]));
          final after = await readPage();
          if (!sameBytes(key, after)) {
            throw const FormatException(
              'Artwork changed during readback. Refresh again.',
            );
          }
          _artworkBytes = bytes;
          _artworkKey = key;
        }
        return bytes;
      } finally {
        await transferPriority(false);
      }
    }
    final bytes = await (await device.getCharacteristic(
      uuid,
      service: serviceUuid,
    )).read();
    assert(() {
      if (uuid == statusUuid) {
        debugPrint('Pixel Badge status read: ${bytes.join(',')}');
      }
      return true;
    }());
    return bytes;
  }

  @override
  Future<void> write(String uuid, Uint8List data) async =>
      (await device.getCharacteristic(
        uuid,
        service: serviceUuid,
      )).write(data, withResponse: true);
  @override
  Stream<Uint8List> get statuses => result.onValueReceived;
  @override
  Stream<bool> get connections => device.connectionStream;
  @override
  Future<void> close() {
    _closed = true;
    return _closing ??= _close();
  }

  Future<void> _close() async {
    await _connection?.cancel();
    await _stopBackground();
    try {
      await unsubscribeAudio();
      await result.unsubscribe();
    } catch (_) {
      // The OS may already have removed the disconnected GATT client.
    } finally {
      try {
        await device.disconnect();
      } catch (_) {
        /* Already disconnected. */
      }
    }
  }
}
