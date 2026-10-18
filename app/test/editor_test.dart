// SPDX-License-Identifier: MIT
import 'dart:typed_data';
import 'package:badge_app/draft_store.dart';
import 'package:badge_app/font5x8.dart';
import 'package:badge_app/protocol.dart';
import 'package:badge_app/studio_model.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'screen_test.dart' show showApp, page, capture, loadUiFonts;

void main() {
  setUpAll(loadUiFonts);
  test(
    'sequential read uses one cursor command and verifies every page offset',
    () async {
      final commands = <Uint8List>[];
      var offset = 0;
      final source = MatrixDesign(
        frames: [Uint8List.fromList(List.generate(1024, (i) => i % 256))],
      ).encode();
      final result = await readArtworkPages(
        (v) async => commands.add(v),
        () async {
          final end = (offset + 236).clamp(0, source.length);
          final page = Uint8List.fromList([
            ...(ByteData(8)
                  ..setUint32(0, offset, Endian.little)
                  ..setUint32(4, source.length, Endian.little))
                .buffer
                .asUint8List(),
            ...source.sublist(offset, end),
          ]);
          offset = end;
          return page;
        },
        wide: true,
        sequential: true,
      );
      expect(result, source);
      expect(commands, [
        [4],
      ]);
      await expectLater(
        readArtworkPages(
          (_) async {},
          () async => Uint8List.fromList([1, 0, 0, 0, 10, 0, 0, 0, 1]),
          wide: true,
          sequential: true,
        ),
        throwsFormatException,
      );
    },
  );

  test('editor capability and brightness preserve legacy support', () {
    final caps = BadgeCapabilities.decode([2, 25, 32, 4, 7, 3, 128, 255]);
    expect(caps.hasEditorV2, isTrue);
    expect(
      () => BadgeCapabilities.decode([2, 25, 32, 4, 7, 3, 16, 159]),
      throwsFormatException,
    );
    expect(
      BadgeCapabilities.decode([2, 25, 32, 4, 7, 3, 128, 127]).hasEditorV2,
      isFalse,
    );
    validateValue(BadgeField.brightness, [13]);
    validateValue(BadgeField.brightness, [16]);
    expect(brightnessLevels.first, 13);
  });

  test('bounce lines share reversal and proportional travel without holds', () {
    const design = TextDesign(
      text: 'ABCDEFG\nHIJKLM\nZ',
      layout: 3,
      scale: 1,
      interval: 100,
    );
    // Long lines overflow by 9 and 3 pixels. Both sweep in 1500 ms,
    // from +3 to -(overflow + 3), then reverse without an endpoint hold.
    for (final sample in <(int, int, int)>[
      (0, 3, 3),
      (100, 2, 2),
      (750, -5, -2),
      (1500, -12, -6),
      (1600, -11, -5),
      (2250, -5, -2),
      (3000, 3, 3),
    ]) {
      final frame = design.render(sample.$1);
      for (final row in <(String, int, int)>[
        ('ABCDEFG', sample.$2, 4),
        ('HIJKLM', sample.$3, 12),
        ('Z', 13, 20),
      ]) {
        for (var x = 0; x < 32; x++) {
          for (var y = 0; y < 8; y++) {
            final at = x - row.$2, letter = at ~/ 6, column = at % 6;
            final expected =
                at >= 0 &&
                letter < row.$1.length &&
                column < 5 &&
                (font5x8[(row.$1.codeUnitAt(letter) - 32) * 5 + column] &
                        (1 << y)) !=
                    0;
            expect(
              pixel(frame, x, row.$3 + y),
              expected,
              reason: 'time ${sample.$1}, line ${row.$1}, ($x,$y)',
            );
          }
        }
      }
    }
    expect(design.render(0), isNot(design.render(100)));
    expect(design.render(1500), isNot(design.render(1600)));
    expect(
      const TextDesign(text: '\n\n\n\n', layout: 3).render(0xffffffff),
      Uint8List(128),
    );
    const pages = TextDesign(text: 'A\nB\nC\nD\nE', layout: 3, interval: 100);
    expect(pages.render(0), isNot(pages.render(4000)));
    expect(pages.render(0), pages.render(8000));
  });

  testWidgets(
    'keyboard expands the editor without losing focus or draft text',
    (tester) async {
      final store = MemoryDraftStore();
      await store.save(
        StudioDraft(text: const TextDesign(text: 'First\nSecond\nThird')),
      );
      await showApp(tester, store: store);
      await page(tester, 'Text');
      final field = find.byKey(const Key('text-message'));
      final originalHeight = tester.getSize(field).height;
      await tester.tap(field);
      await tester.pump();
      final original = tester.widget<TextField>(field);
      tester.view.viewInsets = const FakeViewPadding(bottom: 300);
      addTearDown(tester.view.resetViewInsets);
      await tester.pumpAndSettle();
      final expanded = tester.widget<TextField>(field);
      expect(expanded.expands, isTrue);
      expect(expanded.controller, same(original.controller));
      expect(expanded.focusNode!.hasFocus, isTrue);
      expect(tester.getSize(field).height, greaterThan(originalHeight + 50));
      expect(expanded.controller!.text, 'First\nSecond\nThird');
      expect(tester.takeException(), isNull);
      await capture(tester, 'bounce-keyboard-editor');
      tester.view.physicalSize = const Size(320, 720);
      tester.platformDispatcher.textScaleFactorTestValue = 2;
      addTearDown(tester.platformDispatcher.clearTextScaleFactorTestValue);
      await tester.pumpAndSettle();
      expect(tester.takeException(), isNull);
      expect(tester.widget<TextField>(field).focusNode!.hasFocus, isTrue);
      await capture(tester, 'bounce-keyboard-large-text');
      tester.platformDispatcher.clearTextScaleFactorTestValue();
      tester.view.physicalSize = const Size(390, 844);
      tester.view.viewInsets = FakeViewPadding.zero;
      await tester.pumpAndSettle();
      expect(tester.widget<TextField>(field).expands, isFalse);
      expect(tester.getSize(field).height, lessThan(originalHeight + 25));
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets('caret arrows move without selecting or altering text', (
    tester,
  ) async {
    final store = MemoryDraftStore();
    await store.save(StudioDraft(text: const TextDesign(text: 'ABCDE')));
    await showApp(tester, store: store);
    await page(tester, 'Text');
    await tester.tap(find.byKey(const Key('text-message')));
    await tester.pump();
    final editor = tester
        .widget<TextField>(find.byKey(const Key('text-message')))
        .controller!;
    editor.selection = const TextSelection(baseOffset: 1, extentOffset: 4);
    await tester.tap(find.byTooltip('Move cursor right'));
    await tester.pump();
    expect(editor.selection, const TextSelection.collapsed(offset: 4));
    await tester.tap(find.byTooltip('Move cursor left'));
    await tester.pump();
    expect(editor.selection, const TextSelection.collapsed(offset: 3));
    expect(editor.text, 'ABCDE');
    await capture(tester, 'editor-caret');
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('new design replaces all frames but preserves the library', (
    tester,
  ) async {
    final matrix = MatrixDesign()
      ..paint(1, 1, 255)
      ..add(duplicate: true);
    final store = MemoryDraftStore();
    await store.save(
      StudioDraft(
        matrix: matrix,
        library: [
          SavedDesign(
            id: 'saved',
            name: 'Keep',
            isText: false,
            bytes: matrix.encode(),
          ),
        ],
      ),
    );
    await showApp(tester, store: store);
    await page(tester, 'Studio');
    await tester.tap(find.byTooltip('Actions'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('New design'));
    await tester.pumpAndSettle();
    await tester.tap(find.widgetWithText(FilledButton, 'New design'));
    await tester.pumpAndSettle();
    await tester.pump(const Duration(seconds: 1));
    final saved = await store.load();
    expect(saved.matrix.frames.length, 1);
    expect(saved.matrix.current.every((v) => v == 0), isTrue);
    expect(saved.library.single.name, 'Keep');
    await tester.pumpWidget(const SizedBox());
  });
}
