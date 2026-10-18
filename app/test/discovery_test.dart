// SPDX-License-Identifier: MIT
import 'dart:async';
import 'package:badge_app/badge_controller.dart';
import 'package:badge_app/studio_home.dart';
import 'package:badge_app/draft_store.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:universal_ble/universal_ble.dart';
import 'ble_connection_test.dart' show Adapter;

class ScanAdapter extends Adapter {
  ScanAdapter({this.bonded = true, this.pairingGate});
  final bool bonded;
  final Completer<void>? pairingGate;
  @override
  Future<bool> isPaired(String id) async => bonded;
  @override
  Future<bool> pair(String id) async {
    await pairingGate?.future;
    return true;
  }

  @override
  Future<void> startScan({
    ScanFilter? scanFilter,
    PlatformConfig? platformConfig,
  }) async {}
  @override
  Future<void> stopScan() async {}
}

void main() {
  for (final count in [1, 2]) {
    testWidgets('$count discovered badges: auto-connect only for one', (
      tester,
    ) async {
      debugDefaultTargetPlatformOverride = TargetPlatform.linux;
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(
            const MethodChannel('com.ziadg.badge_app/audio'),
            (_) async => null,
          );
      final adapter = ScanAdapter();
      UniversalBle.setInstance(adapter);
      final found = StreamController<BleDevice>.broadcast(sync: true);
      final badge = BadgeController();
      await tester.pumpWidget(
        MaterialApp(
          home: BadgeHome(
            controller: badge,
            draftStore: MemoryDraftStore(),
            scanResults: found.stream,
          ),
        ),
      );
      await tester.pump(const Duration(milliseconds: 300));
      await tester.tap(find.text('Find badge'));
      await tester.pump(const Duration(milliseconds: 200));
      for (var i = 0; i < count; i++) {
        found.add(
          BleDevice(deviceId: 'AA:BB:CC:DD:EE:0${i + 1}', name: 'Badge $i'),
        );
      }
      await tester.pump(const Duration(seconds: 4));
      for (var i = 0; i < 12; i++) {
        await tester.pump(const Duration(milliseconds: 200));
      }
      if (count == 1) {
        expect(
          badge.connected,
          isTrue,
          reason: '${adapter.online} / ${badge.message}',
        );
        expect(adapter.online['AA:BB:CC:DD:EE:01'], isTrue);
        expect(find.text('Find your badge'), findsNothing);
      } else {
        expect(badge.connected, isFalse);
        expect(adapter.online, isEmpty);
        expect(find.text('Badge 0'), findsOneWidget);
        expect(find.text('Badge 1'), findsOneWidget);
      }
      await tester.pumpWidget(const SizedBox());
      badge.dispose();
      await found.close();
      debugDefaultTargetPlatformOverride = null;
      expect(tester.takeException(), isNull);
    });
  }
  for (final bonded in [true, false]) {
    testWidgets('connection guidance matches OS bond state $bonded', (
      tester,
    ) async {
      debugDefaultTargetPlatformOverride = TargetPlatform.linux;
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(
            const MethodChannel('com.ziadg.badge_app/audio'),
            (_) async => null,
          );
      final gate = Completer<void>();
      final adapter = ScanAdapter(bonded: bonded, pairingGate: gate);
      UniversalBle.setInstance(adapter);
      final found = StreamController<BleDevice>.broadcast(sync: true);
      final badge = BadgeController();
      await tester.pumpWidget(
        MaterialApp(
          home: BadgeHome(
            controller: badge,
            draftStore: MemoryDraftStore(),
            scanResults: found.stream,
          ),
        ),
      );
      await tester.pump(const Duration(milliseconds: 300));
      await tester.tap(find.text('Find badge'));
      await tester.pump(const Duration(milliseconds: 200));
      found.add(BleDevice(deviceId: 'AA:BB:CC:DD:EE:01', name: 'Badge'));
      await tester.pump(const Duration(seconds: 4));
      for (var i = 0; i < 12; i++) {
        await tester.pump(const Duration(milliseconds: 100));
      }
      if (bonded) {
        expect(find.text('Reconnecting'), findsOneWidget);
        expect(
          find.text(
            'Enter the code on your badge in the phone pairing dialog.',
          ),
          findsNothing,
        );
      } else {
        expect(
          find.text(
            'Enter the code on your badge in the phone pairing dialog.',
          ),
          findsOneWidget,
        );
      }
      gate.complete();
      for (var i = 0; i < 12; i++) {
        await tester.pump(const Duration(milliseconds: 100));
      }
      expect(badge.connected, isTrue);
      expect(find.text('Create'), findsNothing);
      await tester.pumpWidget(const SizedBox());
      badge.dispose();
      await found.close();
      debugDefaultTargetPlatformOverride = null;
      expect(tester.takeException(), isNull);
    });
  }
}
