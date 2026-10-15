// SPDX-License-Identifier: MIT
import 'dart:async';
import 'package:flutter/services.dart';
import 'package:flutter/foundation.dart'
    show defaultTargetPlatform, TargetPlatform, kIsWeb;
import 'package:flutter/foundation.dart' show debugPrint;
import 'package:universal_ble/universal_ble.dart';
import 'badge_controller.dart';
import 'protocol.dart';
import 'audio_controller.dart';

class UniversalBadgeLink implements BadgeLink, AudioTransport {
  UniversalBadgeLink._(this.device, this.result, this.capabilities);
  final BleDevice device;
  final BleCharacteristic result;
  final BadgeCapabilities capabilities;
  BleCharacteristic? _audio;
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

  static Future<UniversalBadgeLink> connect(BleDevice device) async {
    try {
      await device.connect();
      await device.discoverServices();
      final info = await (await device.getCharacteristic(
        infoUuid,
        service: serviceUuid,
      )).read();
      final capabilities = BadgeCapabilities.decode(info);
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

  @override
  Future<Uint8List> read(String uuid) async {
    if (uuid == BadgeField.artwork.uuid) {
      final characteristic = await device.getCharacteristic(
        uuid,
        service: serviceUuid,
      );
      return readArtworkPages(
        (cursor) => characteristic.write(cursor, withResponse: true),
        characteristic.read,
      );
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
  Future<void> close() async {
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
