// SPDX-License-Identifier: MIT
import 'dart:convert';
import 'dart:typed_data';
import 'package:badge_app/badge_controller.dart';
import 'package:badge_app/main.dart';
import 'package:badge_app/protocol.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'controller_test.dart' show FakeLink;

void main() {
  testWidgets('missing NFC explains link gate and keeps identity available', (
    tester,
  ) async {
    final link = FakeLink()..nfcVerified = false;
    final controller = BadgeController();
    await controller.attach(link);
    await tester.pumpWidget(
      MaterialApp(
        home: BadgeHome(
          controller: controller,
          scanResults: const Stream.empty(),
        ),
      ),
    );
    await tester.scrollUntilVisible(
      find.text('Apply link'),
      150,
      scrollable: find.byType(Scrollable).first,
    );
    expect(find.text(BadgeController.nfcRequiredMessage), findsOneWidget);
    expect(
      tester
          .widget<FilledButton>(find.widgetWithText(FilledButton, 'Apply link'))
          .onPressed,
      isNull,
    );
    await tester.scrollUntilVisible(
      find.text('Apply name'),
      150,
      scrollable: find.byType(Scrollable).first,
    );
    expect(
      tester
          .widget<FilledButton>(find.widgetWithText(FilledButton, 'Apply name'))
          .onPressed,
      isNotNull,
    );
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
    controller.dispose();
    await link.dispose();
  });
  testWidgets('refreshed name matches the editable value sent by Apply', (
    tester,
  ) async {
    final link = FakeLink();
    final controller = BadgeController();
    await controller.attach(link);
    await tester.pumpWidget(
      MaterialApp(
        home: BadgeHome(
          controller: controller,
          scanResults: const Stream.empty(),
        ),
      ),
    );
    link.values[BadgeField.name.uuid] = Uint8List.fromList(
      ascii.encode('UPDATED'),
    );
    await controller.refresh();
    await tester.pump();
    await tester.scrollUntilVisible(
      find.byType(TextFormField),
      250,
      scrollable: find.byType(Scrollable).first,
    );
    expect(
      tester.widget<TextFormField>(find.byType(TextFormField)).controller!.text,
      'UPDATED',
    );
    await tester.ensureVisible(find.text('Apply name'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('Apply name'));
    await tester.pumpAndSettle();
    expect(controller.name, 'UPDATED');
    expect(link.applies, 1);
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
    controller.dispose();
    await link.dispose();
  });
  testWidgets('connection instructions fit a narrow phone', (tester) async {
    tester.view.physicalSize = const Size(390, 844);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    await tester.pumpWidget(
      const MaterialApp(home: BadgeHome(scanResults: Stream.empty())),
    );
    expect(find.text('Find badge'), findsOneWidget);
    expect(find.textContaining('Hold Action'), findsOneWidget);
    expect(find.text('Apply link'), findsNothing);
    expect(tester.takeException(), isNull);
  });
  testWidgets('readback labels become unconfirmed on a disconnected phone', (
    tester,
  ) async {
    final link = FakeLink();
    final controller = BadgeController();
    await controller.attach(link);
    await tester.pumpWidget(
      MaterialApp(
        home: BadgeHome(
          controller: controller,
          scanResults: const Stream.empty(),
        ),
      ),
    );
    await tester.pump();
    expect(find.text('Active link'), findsOneWidget);
    expect(find.text('HTTPS://A.B/X'), findsOneWidget);
    link.online.add(false);
    await tester.pump();
    expect(find.text('Active link'), findsNothing);
    expect(find.text('Find badge'), findsOneWidget);
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
    controller.dispose();
    await link.dispose();
  });
}
