// SPDX-License-Identifier: MIT
import 'dart:async';
import 'dart:typed_data';
import 'package:flutter/foundation.dart' show debugPrint;
import 'package:universal_ble/universal_ble.dart';
import 'badge_controller.dart';
import 'protocol.dart';

class UniversalBadgeLink implements BadgeLink {
  UniversalBadgeLink._(this.device, this.result);
  final BleDevice device;
  final BleCharacteristic result;
  static Future<UniversalBadgeLink> connect(BleDevice device) async {
    try {
      await device.connect();
      await device.discoverServices();
      final info = await (await device.getCharacteristic(
        infoUuid,
        service: serviceUuid,
      )).read();
      if (info.length != 8 ||
          info[0] != 1 ||
          info[1] != 20 ||
          info[2] != 32 ||
          info[3] != 4 ||
          info[4] != 2 ||
          info[6] != 0 ||
          info[7] != 0) {
        throw const FormatException(
          'Badge firmware uses an unsupported configuration protocol.',
        );
      }
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
      return UniversalBadgeLink._(device, result);
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
    try {
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
