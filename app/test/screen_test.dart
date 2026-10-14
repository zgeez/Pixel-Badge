// SPDX-License-Identifier: MIT
import 'dart:async';
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
import 'studio_controller_test.dart' show StudioLink;

class DelayedDraftStore implements DraftStore {
  final result = Completer<StudioDraft>();
  @override
  Future<StudioDraft> load() => result.future;
  @override
  Future<void> save(StudioDraft draft) async {}
}

Future<void> showApp(
  WidgetTester tester, {
  BadgeController? controller,
  DraftStore? store,
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
    final label = List.filled(32, '\u{1f469}\u200d\u{1f4bb}').join();
    await tester.enterText(find.byType(TextFormField).first, label);
    await tester.enterText(find.byType(TextFormField).last, 'https://A.B/x');
    await tester.tap(find.text('Save link'));
    await tester.pump(const Duration(seconds: 1));
    expect((await store.load()).links.single.url, 'HTTPS://A.B/x');
    expect((await store.load()).links.single.label, label);
    expect(find.text('Active'), findsNothing);
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets('text size follows delayed draft load and badge replacement', (
    tester,
  ) async {
    final store = DelayedDraftStore();
    final link = StudioLink();
    link.values[BadgeField.text.uuid] = const TextDesign(
      text: 'X',
      scale: 3,
    ).encode();
    final controller = BadgeController();
    await controller.attach(link, capabilities: BadgeCapabilities.studio);
    await showApp(tester, store: store, controller: controller);
    await page(tester, 'Text');
    expect(
      tester
          .state<FormFieldState<int>>(find.byType(DropdownButtonFormField<int>))
          .value,
      0,
    );
    store.result.complete(
      StudioDraft(text: const TextDesign(text: 'Hi', scale: 2)),
    );
    await tester.pump();
    expect(
      tester
          .state<FormFieldState<int>>(find.byType(DropdownButtonFormField<int>))
          .value,
      2,
    );
    await visible(tester, find.text('Load from badge'));
    await tester.tap(find.text('Load from badge'));
    await tester.pump();
    await tester.tap(find.text('Load draft'));
    await tester.pump();
    expect(
      tester
          .state<FormFieldState<int>>(find.byType(DropdownButtonFormField<int>))
          .value,
      3,
    );
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
    controller.dispose();
    await link.dispose();
  });

  testWidgets(
    'badge text can replace invalid local text without an async error',
    (tester) async {
      final link = StudioLink();
      final content = const TextDesign(text: 'From badge', scale: 2);
      link.values[BadgeField.text.uuid] = content.encode();
      final controller = BadgeController();
      await controller.attach(link, capabilities: BadgeCapabilities.studio);
      final store = MemoryDraftStore();
      await showApp(tester, controller: controller, store: store);
      await page(tester, 'Text');
      await tester.enterText(find.byType(TextFormField).last, '\u{1f600}');
      await tester.pump();
      await visible(tester, find.text('Load from badge'));
      await tester.tap(find.text('Load from badge'));
      await tester.pump();
      expect(find.text('Replace this draft?'), findsOneWidget);
      await tester.tap(find.text('Load draft'));
      await tester.pump(const Duration(seconds: 1));
      expect((await store.load()).text.encode(), content.encode());
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
      controller.dispose();
      await link.dispose();
    },
  );

  testWidgets(
    'short multiline and scaled text preview advances through pages',
    (tester) async {
      for (final design in [
        const TextDesign(
          text: 'A\nb\nc\nD\nE',
          layout: 1,
          scale: 1,
          interval: 100,
        ),
        const TextDesign(text: 'ABCDEF', layout: 1, scale: 2, interval: 100),
        const TextDesign(text: 'AB', layout: 1, scale: 3, interval: 100),
      ]) {
        final store = MemoryDraftStore();
        await store.save(StudioDraft(text: design));
        await showApp(tester, store: store);
        await page(tester, 'Text');
        await visible(tester, find.byType(MatrixPreview));
        final clock =
            tester
                    .widget<ValueListenableBuilder<int>>(
                      find.ancestor(
                        of: find.byType(MatrixPreview),
                        matching: find.byType(ValueListenableBuilder<int>),
                      ),
                    )
                    .valueListenable
                as ValueNotifier<int>;
        clock.value = 0;
        await tester.pump();
        final before = List<int>.from(
          tester.widget<MatrixPreview>(find.byType(MatrixPreview)).frame,
        );
        clock.value = 2000;
        await tester.pump();
        final after = tester
            .widget<MatrixPreview>(find.byType(MatrixPreview))
            .frame;
        expect(after, isNot(orderedEquals(before)));
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      }
    },
  );

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
