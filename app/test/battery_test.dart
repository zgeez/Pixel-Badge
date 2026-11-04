// SPDX-License-Identifier: MIT
import 'dart:typed_data';
import 'package:badge_app/badge_controller.dart';
import 'package:badge_app/battery_model.dart';
import 'package:badge_app/protocol.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'controller_test.dart' show FakeLink;

Uint8List battery(int level, int flags, int mv) =>
    Uint8List.fromList([1, level, flags, 0, mv & 0xff, mv >> 8]);

void main() {
  test('decodes the battery characteristic', () {
    final status = BatteryStatus.decode(battery(4, 3, 3986));
    expect(status.level, BatteryStatus.high);
    expect(status.usbPower, isTrue);
    expect(status.charging, isTrue);
    expect(status.shed, isFalse);
    expect(status.millivolts, 3986);
    expect(status.label, 'Mostly full');
    expect(status.source, 'Charging over USB.');
  });

  test('rejects malformed battery packets', () {
    for (final bytes in [
      Uint8List.fromList([1, 4, 0, 0, 0]),
      Uint8List.fromList([2, 4, 0, 0, 0, 0]),
      battery(6, 0, 4000),
      battery(3, 8, 3800),
      Uint8List.fromList([1, 3, 0, 1, 0, 0]),
    ]) {
      expect(() => BatteryStatus.decode(bytes), throwsFormatException);
    }
  });

  test('describes the power source', () {
    expect(
      BatteryStatus.decode(battery(5, 1, 4190)).source,
      'Charged and on USB power.',
    );
    expect(
      BatteryStatus.decode(battery(3, 1, 3800)).source,
      'On USB power, not charging.',
    );
    expect(
      BatteryStatus.decode(battery(2, 0, 3650)).source,
      'Running on battery.',
    );
    expect(BatteryStatus.decode(battery(0, 0, 0)).known, isFalse);
  });

  test('a full reading while charging is nearly full', () {
    final charging = BatteryStatus.decode(battery(5, 3, 4116));
    expect(charging.label, 'Nearly full');
    expect(charging.icon, Icons.battery_6_bar);
    final done = BatteryStatus.decode(battery(5, 1, 4180));
    expect(done.label, 'Full');
    expect(done.icon, Icons.battery_full);
    expect(done.source, 'Charged and on USB power.');
  });

  test('charging keeps the level word and shows the level with a bolt', () {
    final half = BatteryStatus.decode(battery(3, 3, 3850));
    expect(half.label, 'About half');
    expect(half.icon, Icons.battery_3_bar);
    expect(BatteryStatus.decode(battery(1, 3, 3550)).icon, Icons.battery_1_bar);
  });

  testWidgets('charging indicator shows the level word and a bolt', (
    tester,
  ) async {
    Widget bar(Uint8List bytes) => MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          actions: [BatteryIndicator(BatteryStatus.decode(bytes))],
        ),
      ),
    );
    await tester.pumpWidget(bar(battery(3, 3, 3850)));
    expect(find.text('About half'), findsOneWidget);
    expect(find.text('Charging'), findsNothing);
    expect(find.byIcon(Icons.battery_3_bar), findsOneWidget);
    expect(find.byIcon(Icons.bolt), findsOneWidget);
    await tester.pumpWidget(bar(battery(3, 0, 3850)));
    expect(find.byIcon(Icons.bolt), findsNothing);
  });

  test('controller reads the battery after connecting', () async {
    final link = FakeLink();
    link.values[batteryUuid] = battery(3, 0, 3820);
    final controller = BadgeController();
    await controller.attach(link);
    expect(controller.battery?.label, 'About half');
    await controller.disconnect();
    expect(controller.battery, isNull);
  });

  test(
    'older firmware without the battery characteristic still connects',
    () async {
      final controller = BadgeController();
      await controller.attach(FakeLink());
      expect(controller.confirmed, isTrue);
      expect(controller.battery, isNull);
      await controller.disconnect();
    },
  );
}
