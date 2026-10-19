// SPDX-License-Identifier: MIT
import 'dart:typed_data';
import 'package:badge_app/badge_controller.dart';
import 'package:badge_app/motion_model.dart';
import 'package:badge_app/motion_panel.dart';
import 'package:badge_app/protocol.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'studio_controller_test.dart' show StudioLink;
import 'screen_test.dart' show showApp, page, visible;

class MotionLink extends StudioLink {
  MotionLink() {
    values[BadgeField.motion.uuid] = MotionDesign().encode();
  }
  @override
  Future<Uint8List> read(String uuid) async => uuid == motionStatusUuid
      ? (Uint8List(16)
          ..[0] = 1
          ..[1] = 7)
      : super.read(uuid);
}

void main() {
  testWidgets('motion controls omit mounting and survive returning to Badge', (
    tester,
  ) async {
    final badge = BadgeController(), link = MotionLink();
    await badge.attach(
      link,
      capabilities: const BadgeCapabilities(2, hasAudio: true, hasMotion: true),
    );
    await showApp(tester, controller: badge);
    await tester.tap(find.byTooltip('Motion settings'));
    await tester.pumpAndSettle();
    await visible(tester, find.text('Fluid amount'));
    expect(find.text('Sensor mounting'), findsNothing);
    expect(find.text('Panel right'), findsNothing);
    expect(find.text('Create'), findsNothing);
    await tester.tapAt(const Offset(5, 5));
    await tester.pumpAndSettle();
    await page(tester, 'Links');
    await page(tester, 'Badge');
    await tester.tap(find.byTooltip('Motion settings'));
    await tester.pumpAndSettle();
    await visible(tester, find.text('Apply motion'));
    expect(tester.takeException(), isNull);
    expect(find.byType(DropdownButtonFormField<int>), findsNothing);
    await tester.pumpWidget(const SizedBox());
    badge.dispose();
    await link.dispose();
  });
  test('motion capability and payload keep old badges compatible', () {
    final caps = BadgeCapabilities.decode([2, 20, 32, 4, 7, 3, 16, 31]);
    expect(caps.hasMotion && caps.hasAudio && caps.hasStudio, true);
    expect(caps.modeCount, 7);
    expect(MotionDesign(right: -256).encode, throwsFormatException);
    expect(caps.settings.last, BadgeField.motion);
    expect(BadgeCapabilities.studio.hasMotion, false);
    expect(
      () => BadgeCapabilities.decode([2, 20, 32, 4, 7, 3, 16, 15]),
      throwsFormatException,
    );
    for (var x = 0; x < 6; x++) {
      for (var y = 0; y < 6; y++) {
        final design = MotionDesign(right: x, down: y);
        if (x ~/ 2 == y ~/ 2) {
          expect(design.encode, throwsFormatException);
        } else {
          expect(MotionDesign.decode(design.encode()).down, y);
        }
      }
    }
    expect(
      () => validateValue(BadgeField.motion, [1, 1, 0, 2, 3, 193, 0, 0]),
      throwsFormatException,
    );
    expect(BadgeStatus.decode([2, 1, 0, 0, 0, 7, 2, 0, 0, 0, 0, 3]).field, 7);
    expect(
      stagePackets(BadgeField.motion, MotionDesign().encode()).single.length,
      8,
    );
  });
  test('motion readback confirms exact settings and sensor status', () async {
    final badge = BadgeController(), link = MotionLink();
    await badge.attach(
      link,
      capabilities: const BadgeCapabilities(2, hasAudio: true, hasMotion: true),
    );
    expect(badge.motionStatus!.fresh, true);
    final config = MotionDesign(right: 5, down: 0, particles: 144).encode();
    await badge.apply(BadgeField.motion, config);
    expect(badge.values[BadgeField.motion], config);
    expect(badge.confirmed, true);
    await badge.apply(BadgeField.mode, [6]);
    expect(badge.mode, 6);
    await badge.disconnect();
    expect(badge.motionStatus, null);
    badge.dispose();
    await link.dispose();
  });
  test('motion status rejects malformed flags, length and rotation', () {
    for (final bytes in [
      Uint8List(15),
      Uint8List(16)..[0] = 2,
      Uint8List(16)
        ..[0] = 1
        ..[1] = 8,
      Uint8List(16)
        ..[0] = 1
        ..[2] = 4,
    ]) {
      expect(() => MotionStatus.decode(bytes), throwsFormatException);
    }
    final data = ByteData(16)
      ..setUint8(0, 1)
      ..setUint8(1, 7)
      ..setInt16(6, -9807, Endian.little)
      ..setInt32(12, -5, Endian.little);
    final state = MotionStatus.decode(data.buffer.asUint8List());
    expect(state.x, -9807);
    expect(state.error, -5);
    expect(state.interrupt, true);
  });
  testWidgets('motion controls support manual rotation and bounded response', (
    tester,
  ) async {
    List<int>? sent;
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: SingleChildScrollView(
            child: MotionPanel(
              value: MotionDesign(right: 5, down: 0).encode(),
              status: const MotionStatus(7, 0, 0, 0, 9807, 0),
              enabled: true,
              apply: (value) async => sent = value,
              refresh: () async {},
            ),
          ),
        ),
      ),
    );
    expect(find.text('IMU ready'), findsOneWidget);
    await tester.tap(find.byType(Switch));
    await tester.pumpAndSettle();
    await tester.tap(find.text('90°'));
    await tester.pumpAndSettle();
    final slider = tester.widgetList<Slider>(find.byType(Slider)).first;
    slider.onChanged!(5);
    tester.widgetList<Slider>(find.byType(Slider)).last.onChanged!(144);
    await tester.pump();
    await tester.ensureVisible(find.text('Apply motion'));
    await tester.tap(find.text('Apply motion'));
    await tester.pump();
    expect(sent, [1, 0, 5, 0, 5, 144, 1, 0]);
    await tester.pumpWidget(const SizedBox());
  });
}
