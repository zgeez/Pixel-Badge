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
import 'package:badge_app/selection_control.dart';
import 'studio_controller_test.dart' show StudioLink;

class DelayedDraftStore implements DraftStore {
  final result = Completer<StudioDraft>();
  final saved = <StudioDraft>[];
  @override
  Future<StudioDraft> load() => result.future;
  @override
  Future<void> save(StudioDraft draft) async {
    saved.add(StudioDraft.decode(draft.encode()));
  }
}

Future<void> showApp(
  WidgetTester tester, {
  BadgeController? controller,
  DraftStore? store,
  Brightness brightness = Brightness.light,
  double width = 390,
  double height = 844,
}) async {
  tester.view.physicalSize = Size(width, height);
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
  final index = ['Badge', 'Links', 'Text', 'Studio', 'Audio'].indexOf(label);
  if (find.byType(NavigationBar).evaluate().isNotEmpty) {
    await tester.tap(find.byType(NavigationDestination).at(index));
  } else {
    await tester.tap(
      find.descendant(
        of: find.byType(NavigationRail),
        matching: find.text(label),
      ),
    );
  }
  await tester.pump();
  await tester.pump(const Duration(milliseconds: 300));
}

Future<void> visible(WidgetTester tester, Finder target) async {
  if (target.evaluate().isNotEmpty) {
    await tester.ensureVisible(target);
  } else {
    await tester.scrollUntilVisible(
      target,
      150,
      scrollable: find.byType(Scrollable).first,
    );
  }
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

Future<void> loadUiFonts() async {
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
}

void main() {
  setUpAll(loadUiFonts);
  testWidgets('background retains secure link without resuming capture', (
    tester,
  ) async {
    final link = FakeLink(), badge = BadgeController();
    await badge.attach(link);
    await showApp(tester, controller: badge);
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.paused);
    await tester.pump();
    expect(badge.connected, isTrue);
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.resumed);
    await tester.pump();
    expect(badge.connected, isTrue);
    await tester.pumpWidget(const SizedBox());
    badge.dispose();
    await link.dispose();
  });
  testWidgets('unverified NFC allows guarded retry and text send', (
    tester,
  ) async {
    final link = StudioLink()..nfcVerified = false;
    final badge = BadgeController();
    await badge.attach(link, capabilities: BadgeCapabilities.studio);
    final store = MemoryDraftStore();
    await store.save(
      StudioDraft(links: [SavedLink('My site', 'HTTPS://A.B/X')]),
    );
    await showApp(tester, controller: badge, store: store);
    await page(tester, 'Links');
    expect(
      tester
          .widget<IconButton>(
            find.byWidgetPredicate(
              (w) => w is IconButton && w.tooltip == 'Apply My site',
            ),
          )
          .onPressed,
      isNotNull,
    );
    expect(
      tester
          .widget<IconButton>(
            find.byWidgetPredicate(
              (w) => w is IconButton && w.tooltip == 'Sync again',
            ),
          )
          .onPressed,
      isNotNull,
    );
    await page(tester, 'Text');
    expect(
      tester
          .widget<FilledButton>(
            find.ancestor(
              of: find.text('Send text'),
              matching: find.byWidgetPredicate((w) => w is FilledButton),
            ),
          )
          .onPressed,
      isNotNull,
    );
    await tester.pumpWidget(const SizedBox());
    badge.dispose();
    await link.dispose();
  });
  testWidgets('NFC details retain the actual setup diagnostic', (tester) async {
    final link = StudioLink()
      ..nfcVerified = false
      ..nfcError = -13;
    final badge = BadgeController();
    await badge.attach(link, capabilities: BadgeCapabilities.studio);
    await showApp(tester, controller: badge);
    await page(tester, 'Links');
    await tester.tap(find.byTooltip('NFC details'));
    await tester.pumpAndSettle();
    expect(find.text(BadgeController.nfcSetupMessage), findsOneWidget);
    expect(find.text(BadgeController.nfcRequiredMessage), findsNothing);
    await tester.pumpWidget(const SizedBox());
    badge.dispose();
    await link.dispose();
  });
  testWidgets('composer sends text and mode without an identity editor', (
    tester,
  ) async {
    final link = StudioLink(), badge = BadgeController();
    await badge.attach(link, capabilities: BadgeCapabilities.studio);
    await showApp(tester, controller: badge);
    await page(tester, 'Text');
    expect(find.text('Identity'), findsNothing);
    expect(find.text('Apply name'), findsNothing);
    await tester.enterText(
      find.byKey(const Key('text-message')),
      'Hello badge',
    );
    FocusManager.instance.primaryFocus?.unfocus();
    await tester.pump();
    await tester.tap(find.text('Send text'));
    await tester.pump(const Duration(seconds: 1));
    expect(
      TextDesign.decode(link.values[BadgeField.text.uuid]!).text,
      'Hello badge',
    );
    expect(badge.mode, 1);
    expect(link.applies, 2);
    await tester.pumpWidget(const SizedBox());
    badge.dispose();
    await link.dispose();
  });
  testWidgets('empty message preview and send use the confirmed badge name', (
    tester,
  ) async {
    final link = StudioLink();
    link.values[BadgeField.name.uuid] = Uint8List.fromList(
      ascii.encode('SAVED'),
    );
    final badge = BadgeController();
    await badge.attach(link, capabilities: BadgeCapabilities.studio);
    await showApp(tester, controller: badge);
    await page(tester, 'Text');
    final ticker =
        tester
                .widget<ValueListenableBuilder<int>>(
                  find.ancestor(
                    of: find.byType(MatrixPreview),
                    matching: find.byType(ValueListenableBuilder<int>),
                  ),
                )
                .valueListenable
            as ValueNotifier<int>;
    ticker.value = 0;
    await tester.pump();
    expect(
      tester.widget<MatrixPreview>(find.byType(MatrixPreview)).frame,
      const TextDesign().render(0, fallback: 'SAVED'),
    );
    await tester.tap(find.text('Send text'));
    await tester.pump(const Duration(seconds: 1));
    expect(badge.name, 'SAVED');
    expect(TextDesign.decode(link.values[BadgeField.text.uuid]!).text, isEmpty);
    await tester.pumpWidget(const SizedBox());
    badge.dispose();
    await link.dispose();
  });
  testWidgets('late draft load cannot replace a newer edit or save', (
    tester,
  ) async {
    final store = DelayedDraftStore();
    await showApp(tester, store: store);
    await page(tester, 'Text');
    final input = tester.widget<TextField>(
      find.byKey(const Key('text-message')),
    );
    input.controller!.text = 'NEW';
    input.onChanged!('NEW');
    store.result.complete(
      StudioDraft(text: const TextDesign(text: 'OLD', scale: 3)),
    );
    await tester.pump(const Duration(seconds: 1));
    expect(input.controller!.text, 'NEW');
    expect(store.saved.last.text.text, 'NEW');
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets('late load preserves an in-flight canvas and frame edit', (
    tester,
  ) async {
    final store = DelayedDraftStore();
    await showApp(tester, store: store);
    await page(tester, 'Studio');
    final editor = tester.widget<MatrixEditor>(find.byType(MatrixEditor));
    editor.design.paint(3, 4, 91);
    editor.design.add(duplicate: true);
    editor.onChanged();
    store.result.complete(StudioDraft());
    await tester.pump(const Duration(seconds: 1));
    expect(editor.design.frames.length, 2);
    expect(pixelBrightness(editor.design.current, 3, 4), 91);
    expect(store.saved.last.matrix.frames.length, 2);
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets(
    'draft loading absorbs actual editor touches and restores selected size',
    (tester) async {
      final store = DelayedDraftStore();
      await showApp(tester, store: store);
      await page(tester, 'Text');
      await tester.tap(find.byTooltip('Alignment'), warnIfMissed: false);
      store.result.complete(
        StudioDraft(
          text: const TextDesign(text: 'LOADED', scale: 3, alignment: 0),
        ),
      );
      await tester.pump(const Duration(seconds: 1));
      expect(
        tester
            .widget<SelectionControl<int>>(find.byKey(const Key('text-size')))
            .value,
        3,
      );
      expect(
        tester
            .widget<TextField>(find.byKey(const Key('text-message')))
            .controller!
            .text,
        'LOADED',
      );
      await tester.pumpWidget(const SizedBox());
    },
  );
  testWidgets(
    'badge text replaces invalid local text and updates visible size',
    (tester) async {
      final link = StudioLink();
      link.values[BadgeField.text.uuid] = const TextDesign(
        text: 'BADGE TEXT',
        scale: 2,
      ).encode();
      final badge = BadgeController();
      await badge.attach(link, capabilities: BadgeCapabilities.studio);
      await showApp(tester, controller: badge);
      await page(tester, 'Text');
      await tester.enterText(
        find.byKey(const Key('text-message')),
        '\u{1f600}',
      );
      FocusManager.instance.primaryFocus?.unfocus();
      await tester.pump();
      await tester.tap(find.byTooltip('Load from badge'));
      await tester.pumpAndSettle();
      expect(find.text('Replace this draft?'), findsOneWidget);
      await tester.tap(find.text('Load draft'));
      await tester.pumpAndSettle();
      expect(
        tester
            .widget<TextField>(find.byKey(const Key('text-message')))
            .controller!
            .text,
        'BADGE TEXT',
      );
      expect(
        tester
            .widget<SelectionControl<int>>(find.byKey(const Key('text-size')))
            .value,
        2,
      );
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
      badge.dispose();
      await link.dispose();
    },
  );
  testWidgets('short wrapped text preview advances through all pages', (
    tester,
  ) async {
    final store = MemoryDraftStore();
    await store.save(
      StudioDraft(
        text: const TextDesign(
          text: 'A\nb\nc\nD\nE',
          scale: 1,
          layout: 1,
          interval: 50,
        ),
      ),
    );
    await showApp(tester, store: store);
    await page(tester, 'Text');
    final ticker =
        tester
                .widget<ValueListenableBuilder<int>>(
                  find.ancestor(
                    of: find.byType(MatrixPreview),
                    matching: find.byType(ValueListenableBuilder<int>),
                  ),
                )
                .valueListenable
            as ValueNotifier<int>;
    ticker.value = 0;
    await tester.pump();
    final before = Uint8List.fromList(
      tester.widget<MatrixPreview>(find.byType(MatrixPreview)).frame,
    );
    ticker.value = 1000;
    await tester.pump();
    expect(
      tester.widget<MatrixPreview>(find.byType(MatrixPreview)).frame,
      isNot(before),
    );
    await tester.pumpWidget(const SizedBox());
  });
  for (final brightness in [Brightness.dark, Brightness.light]) {
    for (final size in [
      (390.0, 844.0, 1.0),
      (320.0, 640.0, 1.0),
      (390.0, 700.0, 2.0),
      (1024.0, 844.0, 1.6),
    ]) {
      testWidgets(
        'fixed pages fit ${brightness.name} ${size.$1}x${size.$2} scale ${size.$3}',
        (tester) async {
          tester.platformDispatcher.textScaleFactorTestValue = size.$3;
          addTearDown(tester.platformDispatcher.clearTextScaleFactorTestValue);
          final store = MemoryDraftStore();
          final design = MatrixDesign();
          design.paint(9, 9, 120);
          design.add(duplicate: true);
          await store.save(
            StudioDraft(
              text: const TextDesign(text: 'Hello'),
              matrix: design,
              links: [SavedLink('Work', 'HTTPS://A.B/X')],
            ),
          );
          await showApp(
            tester,
            store: store,
            brightness: brightness,
            width: size.$1,
            height: size.$2,
          );
          for (final label in ['Badge', 'Links', 'Text', 'Studio', 'Audio']) {
            await page(tester, label);
            expect(find.byKey(const Key('page-heading')), findsNothing);
            for (final element in find.byType(Scrollable).evaluate()) {
              if (element.findAncestorWidgetOfExactType<EditableText>() !=
                  null) {
                continue;
              }
              expect(
                (element.widget as Scrollable).axisDirection,
                anyOf(AxisDirection.left, AxisDirection.right),
              );
            }
            expect(tester.takeException(), isNull, reason: label);
            if (size.$1 == 390 && size.$2 == 844) {
              await capture(
                tester,
                'compact-${label.toLowerCase()}-${brightness.name}',
              );
            }
          }
          await tester.pumpWidget(const SizedBox());
        },
      );
    }
  }
  testWidgets('text entry fits with the keyboard and large type', (
    tester,
  ) async {
    tester.platformDispatcher.textScaleFactorTestValue = 1.6;
    addTearDown(tester.platformDispatcher.clearTextScaleFactorTestValue);
    await showApp(tester, width: 320, height: 640);
    await page(tester, 'Text');
    tester.view.viewInsets = const FakeViewPadding(bottom: 280);
    addTearDown(tester.view.resetViewInsets);
    await tester.enterText(find.byKey(const Key('text-message')), 'Hello');
    await tester.pump();
    expect(find.text('Send text'), findsOneWidget);
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets('pairing help stays available without cluttering the homepage', (
    tester,
  ) async {
    await showApp(tester);
    await tester.tap(find.byTooltip('Badge options'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('Pairing help'));
    await tester.pumpAndSettle();
    expect(
      find.textContaining('A paired phone can reconnect directly'),
      findsOneWidget,
    );
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets('canvas strokes stay in the fixed workspace and undo together', (
    tester,
  ) async {
    await showApp(tester);
    await page(tester, 'Studio');
    final box = tester.getRect(find.byType(MatrixEditor));
    final gesture = await tester.startGesture(
      Offset(box.left + box.width * .25, box.top + box.height * .25),
    );
    await gesture.moveBy(Offset(0, box.height * .3));
    await gesture.up();
    await tester.pump();
    final design = tester
        .widget<MatrixEditor>(find.byType(MatrixEditor))
        .design;
    expect(design.current.any((v) => v != 0), isTrue);
    await tester.tap(find.byTooltip('Undo'));
    await tester.pump();
    expect(design.current.every((v) => v == 0), isTrue);
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
  });
  for (final scale in [1.0, 2.0]) {
    testWidgets('connected controls fit a small phone at scale $scale', (
      tester,
    ) async {
      tester.platformDispatcher.textScaleFactorTestValue = scale;
      addTearDown(tester.platformDispatcher.clearTextScaleFactorTestValue);
      final link = StudioLink(), badge = BadgeController();
      await badge.attach(link, capabilities: BadgeCapabilities.studio);
      await showApp(tester, controller: badge, width: 320, height: 640);
      for (final label in ['Badge', 'Links', 'Text', 'Studio', 'Audio']) {
        await page(tester, label);
        expect(tester.takeException(), isNull, reason: label);
      }
      await tester.pumpWidget(const SizedBox());
      badge.dispose();
      await link.dispose();
    });
  }
  testWidgets(
    '128-frame timeline and every tool sheet fit a narrow large-text phone',
    (tester) async {
      tester.platformDispatcher.textScaleFactorTestValue = 2;
      addTearDown(tester.platformDispatcher.clearTextScaleFactorTestValue);
      final design = MatrixDesign(
        frames: List.generate(
          128,
          (f) => Uint8List.fromList(List.generate(1024, (p) => (f + p) % 256)),
        ),
      );
      final store = MemoryDraftStore();
      await store.save(StudioDraft(matrix: design));
      await showApp(tester, store: store, width: 320, height: 640);
      await page(tester, 'Studio');
      tester.widget<MatrixEditor>(find.byType(MatrixEditor)).design.selected =
          127;
      await tester.tap(find.byTooltip('Pick brightness'));
      await tester.pump();
      expect(tester.takeException(), isNull);
      for (final tool in ['Brush', 'Actions', 'Media']) {
        await tester.tap(find.byTooltip(tool));
        await tester.pumpAndSettle();
        expect(tester.takeException(), isNull, reason: tool);
        await tester.tapAt(const Offset(2, 2));
        await tester.pumpAndSettle();
      }
      await tester.pumpWidget(const SizedBox());
    },
  );
}
