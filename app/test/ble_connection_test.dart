// SPDX-License-Identifier: MIT
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:universal_ble/universal_ble.dart';
import 'package:badge_app/ble_link.dart';
import 'package:badge_app/badge_controller.dart';
import 'package:badge_app/protocol.dart';

class Adapter extends UniversalBlePlatform {
  final online = <String, bool>{};
  @override
  dynamic noSuchMethod(Invocation invocation) => super.noSuchMethod(invocation);
  @override
  Future<void> connect(
    String id, {
    Duration? connectionTimeout,
    bool autoConnect = false,
    ConnectionPlatformConfig? platformConfig,
  }) async {
    online[id] = true;
    updateConnection(id, true);
  }

  void drop(String id) {
    online[id] = false;
    updateConnection(id, false);
  }

  @override
  Future<void> disconnect(String id) async {
    drop(id);
  }

  @override
  Future<BleConnectionState> getConnectionState(String id) async =>
      online[id] == true
      ? BleConnectionState.connected
      : BleConnectionState.disconnected;
  @override
  Future<bool> pair(String id) async => true;
  @override
  Future<bool> isPaired(String id) async => true;
  @override
  Future<AvailabilityState> getBluetoothAvailabilityState() async =>
      AvailabilityState.poweredOn;
  @override
  Future<void> setNotifiable(
    String id,
    String service,
    String characteristic,
    BleInputProperty property,
  ) async {}
  @override
  Future<List<BleService>> discoverServices(
    String id,
    bool descriptors,
  ) async => [
    BleService(serviceUuid, [
      for (final uuid in [
        infoUuid,
        statusUuid,
        ...legacyFields.map((field) => field.uuid),
      ])
        BleCharacteristic.withMetaData(
          deviceId: id,
          serviceId: serviceUuid,
          uuid: uuid,
          properties: [
            CharacteristicProperty.read,
            CharacteristicProperty.notify,
          ],
          descriptors: [],
        ),
    ]),
  ];
  @override
  Future<Uint8List> readValue(
    String id,
    String service,
    String characteristic, {
    Duration? timeout,
  }) async {
    if (characteristic == infoUuid) {
      return Uint8List.fromList([1, 20, 32, 4, 2, 3, 0, 0]);
    }
    if (characteristic == statusUuid) {
      return (ByteData(12)
            ..setUint8(0, 1)
            ..setUint8(11, 3))
          .buffer
          .asUint8List();
    }
    if (characteristic == BadgeField.url.uuid) {
      return Uint8List.fromList('HTTPS://A.B/X'.codeUnits);
    }
    if (characteristic == BadgeField.name.uuid) {
      return Uint8List.fromList('BADGE'.codeUnits);
    }
    return Uint8List.fromList([
      characteristic == BadgeField.mode.uuid ? 0 : 128,
    ]);
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  test(
    'old link cleanup preserves the replacement connection service',
    () async {
      debugDefaultTargetPlatformOverride = TargetPlatform.android;
      final adapter = Adapter();
      UniversalBle.setInstance(adapter);
      bool service = false;
      int? owner;
      final calls = <String>[];
      const channel = MethodChannel('com.ziadg.badge_app/connection');
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(channel, (call) async {
            final token = (call.arguments as Map)['owner'] as int;
            calls.add('${call.method}:$token');
            if (call.method == 'start') {
              service = true;
              owner = token;
            } else if (owner == token) {
              service = false;
              owner = null;
            }
            return null;
          });
      final controller = BadgeController();
      final first = await UniversalBadgeLink.connect(
        BleDevice(name: 'Badge', deviceId: 'AA:BB:CC:DD:EE:01'),
      );
      await controller.attach(first);
      expect(service, true);
      adapter.drop('AA:BB:CC:DD:EE:01');
      await Future<void>.delayed(Duration.zero);
      expect(controller.connected, false);
      final second = await UniversalBadgeLink.connect(
        BleDevice(name: 'Badge', deviceId: 'AA:BB:CC:DD:EE:02'),
      );
      expect(service, true);
      await controller.attach(second);
      expect(controller.connected, true);
      expect(adapter.online['AA:BB:CC:DD:EE:02'], true);
      expect(service, true, reason: 'Old close must not stop the new lease');
      adapter.drop('AA:BB:CC:DD:EE:02');
      await Future<void>.delayed(Duration.zero);
      expect(
        service,
        false,
        reason: 'Actual loss of the current link releases its lease',
      );
      expect(calls.where((call) => call.startsWith('start:')).length, 2);
      await controller.disconnect();
      controller.dispose();
      debugDefaultTargetPlatformOverride = null;
    },
  );
}
