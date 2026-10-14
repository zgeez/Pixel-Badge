// SPDX-License-Identifier: MIT
import 'dart:convert';
import 'dart:io';
import 'dart:ui' as ui;
import 'package:badge_app/badge_controller.dart';
import 'package:badge_app/draft_store.dart';
import 'package:badge_app/main.dart';
import 'package:badge_app/matrix_widgets.dart';
import 'package:badge_app/protocol.dart';
import 'package:badge_app/studio_model.dart';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'controller_test.dart' show FakeLink;

Future<void> showApp(
  WidgetTester tester, {
  BadgeController? controller,
  MemoryDraftStore? store,
  Brightness brightness = Brightness.light,
  double width = 390,
}) async {
  tester.view.physicalSize = Size(width, 844);
  tester.view.devicePixelRatio = 1;
  addTearDown(tester.view.resetPhysicalSize);
  addTearDown(tester.view.resetDevicePixelRatio);
  final themes = SmartBadgeApp();
  await tester.pumpWidget(
    RepaintBoundary(
      key: const Key('capture'),
      child: MaterialApp(
        debugShowCheckedModeBanner: false,
        theme: themes.theme(brightness),
        home: BadgeHome(
          controller: controller,
          draftStore: store ?? MemoryDraftStore(),
          scanResults: const Stream.empty(),
        ),
      ),
    ),
  );
  await tester.pump(const Duration(milliseconds: 200));
}

Future<void> page(WidgetTester tester, String label) async {
  await tester.tap(
    find
        .descendant(of: find.byType(NavigationBar), matching: find.text(label))
        .last,
  );
  await tester.pump();
  await tester.pump(const Duration(milliseconds: 800));
}

Future<void> visible(WidgetTester tester, Finder target) async {
  await tester.scrollUntilVisible(
    target,
    200,
    scrollable: find.byType(Scrollable).first,
  );
  await tester.pump(const Duration(milliseconds: 200));
}

Future<void> capture(WidgetTester tester, String name) async {
  if (!const bool.fromEnvironment('CAPTURE_UI')) return;
  final boundary = tester.renderObject<RenderRepaintBoundary>(
    find.byKey(const Key('capture')),
  );
  await tester.runAsync(() async {
    final image = await boundary.toImage(pixelRatio: 1);
    final data = await image.toByteData(format: ui.ImageByteFormat.png);
    final file = File('build/ui/$name.png');
    await file.parent.create(recursive: true);
    await file.writeAsBytes(data!.buffer.asUint8List());
    image.dispose();
  });
}

void main() {
  setUpAll(() async {
    const fontRoot = String.fromEnvironment('UI_FONTS');
    if (fontRoot.isNotEmpty) {
      for (final entry in {
        'Roboto': 'roboto-regular.ttf',
        'MaterialIcons': 'materialicons-regular.otf',
      }.entries) {
        final loader = FontLoader(entry.key)
          ..addFont(
            File(
              '$fontRoot/${entry.value}',
            ).readAsBytes().then((bytes) => ByteData.sublistView(bytes)),
          );
        await loader.load();
      }
    }
  });
  testWidgets('missing NFC disables links while identity remains available', (
    tester,
  ) async {
    final link = FakeLink()..nfcVerified = false;
    final controller = BadgeController();
    await controller.attach(link);
    final store = MemoryDraftStore();
    await store.save(
      StudioDraft(links: [SavedLink('My site', 'HTTPS://A.B/X')]),
    );
    await showApp(tester, controller: controller, store: store);
    await page(tester, 'Links');
    await visible(tester, find.text('Apply link'));
    expect(
      tester
          .widget<FilledButton>(
            find.ancestor(
              of: find.text('Apply link'),
              matching: find.byWidgetPredicate((w) => w is FilledButton),
            ),
          )
          .onPressed,
      isNull,
    );
    await page(tester, 'Text');
    await visible(tester, find.text('Apply name'));
    expect(
      tester
          .widget<FilledButton>(
            find.ancestor(
              of: find.text('Apply name'),
              matching: find.byWidgetPredicate((w) => w is FilledButton),
            ),
          )
          .onPressed,
      isNotNull,
    );
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
    controller.dispose();
    await link.dispose();
  });
  testWidgets('refreshed identity is what Apply sends', (tester) async {
    final link = FakeLink();
    final controller = BadgeController();
    await controller.attach(link);
    await showApp(tester, controller: controller);
    link.values[BadgeField.name.uuid] = Uint8List.fromList(
      ascii.encode('UPDATED'),
    );
    await controller.refresh();
    await tester.pump();
    await page(tester, 'Text');
    await visible(tester, find.text('Apply name'));
    expect(
      tester
          .widget<TextFormField>(find.byType(TextFormField).first)
          .controller!
          .text,
      'UPDATED',
    );
    await tester.tap(find.text('Apply name'));
    await tester.pump(const Duration(seconds: 1));
    expect(controller.name, 'UPDATED');
    expect(link.applies, 1);
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
    controller.dispose();
    await link.dispose();
  });
  testWidgets('offline custom links save locally and never appear active', (
    tester,
  ) async {
    final store = MemoryDraftStore();
    await showApp(tester, store: store);
    await page(tester, 'Links');
    await visible(tester, find.text('Add custom link'));
    await tester.tap(find.text('Add custom link'));
    await tester.pump(const Duration(milliseconds: 300));
    await tester.enterText(find.byType(TextFormField).first, 'My site');
    await tester.enterText(find.byType(TextFormField).last, 'https://A.B/x');
    await tester.tap(find.text('Save link'));
    await tester.pump(const Duration(seconds: 1));
    expect((await store.load()).links.single.url, 'HTTPS://A.B/x');
    expect(find.text('Active'), findsNothing);
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
  });
  for (final brightness in Brightness.values) {
    testWidgets('four pages fit a narrow ${brightness.name} phone', (
      tester,
    ) async {
      final store = MemoryDraftStore();
      final matrix = MatrixDesign();
      matrix.rectangle(9, 9, 22, 22);
      matrix.add(duplicate: true);
      await store.save(
        StudioDraft(
          links: [SavedLink('My site', 'HTTPS://A.B/X')],
          text: TextDesign(text: 'Hi!'),
          matrix: matrix,
        ),
      );
      await showApp(tester, store: store, brightness: brightness);
      await capture(tester, 'badge-${brightness.name}');
      for (final label in ['Links', 'Text', 'Studio']) {
        await page(tester, label);
        if (label == 'Studio') {
          expect(
            pixel(
              tester
                  .widget<MatrixEditor>(find.byType(MatrixEditor))
                  .design
                  .current,
              9,
              9,
            ),
            isTrue,
          );
        }
        await capture(tester, '${label.toLowerCase()}-${brightness.name}');
        expect(tester.takeException(), isNull);
      }
      await tester.pumpWidget(const SizedBox());
    });
  }
  testWidgets('large text and tablet navigation remain usable', (tester) async {
    tester.platformDispatcher.textScaleFactorTestValue = 1.6;
    addTearDown(tester.platformDispatcher.clearTextScaleFactorTestValue);
    await showApp(tester, width: 1024);
    expect(find.byType(NavigationRail), findsOneWidget);
    await tester.tap(
      find.descendant(
        of: find.byType(NavigationRail),
        matching: find.text('Studio'),
      ),
    );
    await tester.pump(const Duration(milliseconds: 200));
    await capture(tester, 'studio-tablet');
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets('pairing guidance includes direct bonded reconnect', (
    tester,
  ) async {
    await showApp(tester);
    await tester.tap(find.text('Find badge'));
    await tester.pump(const Duration(milliseconds: 400));
    expect(
      find.textContaining('A paired phone can reconnect directly'),
      findsOneWidget,
    );
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets('vertical canvas strokes stay on the canvas and undo together', (
    tester,
  ) async {
    final design = MatrixDesign();
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: ListView(
            children: [
              MatrixEditor(
                design: design,
                tool: PixelTool.pen,
                onChanged: () {},
              ),
              const SizedBox(height: 1000),
            ],
          ),
        ),
      ),
    );
    final origin = tester.getTopLeft(find.byType(MatrixEditor));
    final width = tester.getSize(find.byType(MatrixEditor)).width;
    final cell = width / 34;
    final gesture = await tester.startGesture(
      origin + Offset(10.5 * cell, 5.5 * cell),
    );
    await gesture.moveBy(Offset(0, 10 * cell));
    await gesture.up();
    await tester.pump();
    expect(pixel(design.current, 9, 14), isTrue);
    design.undo();
    expect(design.current.every((v) => v == 0), isTrue);
    expect(design.canUndo, isFalse);
    await tester.pumpWidget(const SizedBox());
  });
}
