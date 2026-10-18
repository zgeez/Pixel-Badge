// SPDX-License-Identifier: MIT
import 'dart:convert';
import 'dart:typed_data';
import 'package:badge_app/qr_capacity.dart';
import 'package:badge_app/link_editor.dart';
import 'package:badge_app/protocol.dart';
import 'package:badge_app/studio_model.dart';
import 'package:badge_app/draft_store.dart';
import 'package:badge_app/image_import.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'screen_test.dart' show showApp, page, visible, capture, loadUiFonts;
import 'import_test.dart' show waitForImport;

void main() {
  testWidgets(
    'library renames text and animation without a manual draft save',
    (tester) async {
      final matrix = MatrixDesign();
      matrix.paint(3, 4, 91);
      matrix.add(duplicate: true);
      final store = MemoryDraftStore();
      await store.save(
        StudioDraft(
          library: [
            SavedDesign(
              id: 'text-id',
              name: 'Old text',
              isText: true,
              bytes: const TextDesign(text: 'Hello').encode(),
            ),
            SavedDesign(
              id: 'art-id',
              name: 'Old animation',
              isText: false,
              bytes: matrix.encode(),
            ),
          ],
        ),
      );
      await showApp(tester, store: store);
      for (final item in [
        ('Text', 'Old text', 'New text'),
        ('Studio', 'Old animation', 'New animation'),
      ]) {
        await page(tester, item.$1);
        await tester.tap(find.byTooltip('Open library'));
        await tester.pumpAndSettle();
        expect(find.text('Save draft'), findsNothing);
        await visible(tester, find.byTooltip('Rename ${item.$2}'));
        await tester.tap(find.byTooltip('Rename ${item.$2}'));
        await tester.pumpAndSettle();
        await tester.enterText(find.byType(TextField).last, item.$3);
        await tester.tap(find.widgetWithText(FilledButton, 'Rename'));
        await tester.pumpAndSettle();
        expect(find.text(item.$3), findsOneWidget);
        await tester.pageBack();
        await tester.pumpAndSettle();
      }
      final saved = await store.load();
      expect(saved.library.map((v) => v.id), ['text-id', 'art-id']);
      expect(saved.library.map((v) => v.name), ['New text', 'New animation']);
      expect(saved.library.last.bytes, matrix.encode());
      await tester.pumpWidget(const SizedBox());
    },
  );

  setUpAll(loadUiFonts);
  testWidgets('image import previews adjustments and returns editable frames', (
    tester,
  ) async {
    final source = ImportedRaster([
      RasterFrame(1, 1, Uint8List.fromList([255, 255, 255, 255]), 100),
      RasterFrame(1, 1, Uint8List.fromList([0, 0, 0, 255]), 200),
    ], 2);
    MatrixDesign? selected;
    await tester.pumpWidget(
      MaterialApp(
        home: Builder(
          builder: (context) => Scaffold(
            body: TextButton(
              onPressed: () async {
                selected = await showDialog<MatrixDesign>(
                  context: context,
                  builder: (_) => ImportDesignDialog(image: source),
                );
              },
              child: const Text('Import'),
            ),
          ),
        ),
      ),
    );
    await tester.runAsync(() async {
      await tester.tap(find.text('Import'));
      await tester.pump();
    });
    await tester.pump(const Duration(milliseconds: 300));
    expect(find.text('2 frames'), findsOneWidget);
    await waitForImport(tester);
    await tester.tap(find.text('Tone'));
    await tester.pump();
    await tester.ensureVisible(find.text('Invert'));
    await tester.pump();
    await tester.runAsync(() async {
      await tester.tap(find.text('Invert'));
      await Future<void>.delayed(const Duration(milliseconds: 180));
    });
    await tester.pump();
    await waitForImport(tester);
    await tester.tap(find.text('Use design'));
    await tester.pumpAndSettle();
    await tester.runAsync(
      () => Future<void>.delayed(const Duration(milliseconds: 10)),
    );
    expect(selected!.frames.length, 2);
    expect(selected!.interval, 150);
    expect(selected!.frames.first.every((v) => v == 0), isTrue);
    expect(selected!.frames.last.every((v) => v == 255), isTrue);
    expect(tester.takeException(), isNull);
  });
  test('all four V1 correction levels and exact capacity limits', () {
    for (final pair in [(7, 'H'), (11, 'Q'), (14, 'M'), (17, 'L')]) {
      expect(QrCapacity.fit('a' * pair.$1)!.correction, pair.$2);
    }
    expect(QrCapacity.fit('A' * 25)!.correction, 'L');
    expect(QrCapacity.fit('A' * 26), isNull);
    expect(QrCapacity.fit('a' * 18), isNull);
    expect(QrCapacity.fit('A' * 21, extended: false), isNull);
    validateValue(BadgeField.url, ascii.encode('HTTPS://EXAMPLE.COM/ABCDE'));
    validateValue(BadgeField.url, ascii.encode('https://a.b/abcde'));
    expect(
      () => validateValue(BadgeField.url, ascii.encode('https://a.b/abcdef')),
      throwsFormatException,
    );
    final packets = stagePackets(
      BadgeField.url,
      ascii.encode('HTTPS://EXAMPLE.COM/ABCDE'),
    );
    expect(packets.map((v) => v.length), [20, 9]);
    expect(packets.last.take(2), [18, 25]);
    expect(
      BadgeCapabilities.decode([2, 25, 32, 4, 7, 3, 16, 31]).maxUrlLength,
      25,
    );
  });
  test(
    'v1 drafts migrate and library snapshots do not share editor pixels',
    () {
      final old = jsonDecode(StudioDraft().encode()) as Map<String, dynamic>;
      old['version'] = 1;
      old.remove('library');
      final draft = StudioDraft.decode(jsonEncode(old));
      expect(draft.library, isEmpty);
      final design = MatrixDesign();
      setPixel(design.current, 2, 3, true);
      draft.library.add(
        SavedDesign(
          id: '1',
          name: 'Orbit',
          isText: false,
          bytes: design.encode(),
        ),
      );
      draft.library.add(
        SavedDesign(
          id: '2',
          name: 'Hello',
          isText: true,
          bytes: const TextDesign(text: 'HI').encode(),
        ),
      );
      design.clear();
      final saved = StudioDraft.decode(draft.encode());
      expect(
        pixel(MatrixDesign.decode(saved.library.first.bytes).current, 2, 3),
        isTrue,
      );
      expect(TextDesign.decode(saved.library.last.bytes).text, 'HI');
      saved.library.add(saved.library.first);
      expect(() => StudioDraft.decode(saved.encode()), throwsFormatException);
    },
  );
  test(
    'image conversion preserves aspect, transparency, inversion and dithering',
    () {
      final rgba = Uint8List.fromList([255, 255, 255, 255, 255, 255, 255, 255]);
      final image = RasterFrame(2, 1, rgba, 100);
      final fit = rasterize(image), crop = rasterize(image, crop: true);
      expect(pixel(fit, 0, 0), isFalse);
      expect(pixel(fit, 16, 16), isTrue);
      expect(pixel(crop, 0, 0), isTrue);
      expect(pixel(rasterize(image, invert: true), 16, 16), isFalse);
      final grey = RasterFrame(
        1,
        1,
        Uint8List.fromList([127, 127, 127, 255]),
        100,
      );
      final bits = rasterize(grey, dither: true);
      final count = [
        for (var y = 0; y < 32; y++)
          for (var x = 0; x < 32; x++)
            if (pixel(bits, x, y)) 1,
      ].length;
      expect(count, inInclusiveRange(450, 570));
      expect(
        rasterize(
          RasterFrame(1, 1, Uint8List.fromList([255, 255, 255, 0]), 100),
        ),
        Uint8List(128),
      );
    },
  );
  testWidgets('animated GIF import decodes both frames and bounded duration', (
    tester,
  ) async {
    final hex =
        '47494638396101000100800000000000ffffff21ff0b4e45545343415045322e30030100000021f904000a0000002c000000000100010000020244010021f90400140000002c00000000010001000002024c01003b';
    final bytes = Uint8List.fromList([
      for (var i = 0; i < hex.length; i += 2)
        int.parse(hex.substring(i, i + 2), radix: 16),
    ]);
    final decoded = await tester.runAsync(() => decodeRaster(bytes));
    expect(decoded!.frames.length, 2);
    expect(decoded.interval, 150);
    expect(decoded.convert().frames[0], isNot(decoded.convert().frames[1]));
    await expectLater(decodeRaster(Uint8List(0)), throwsFormatException);
  });
  testWidgets('disconnected home is clean and hero contents are centred', (
    tester,
  ) async {
    await showApp(tester);
    expect(find.text('Create'), findsNothing);
    expect(find.text('Ready when you are'), findsOneWidget);
    final rect = tester.getRect(find.text('Ready when you are'));
    expect(rect.center.dx, closeTo(195, 1));
    await capture(tester, 'polish-home');
    await page(tester, 'Links');
    expect(find.text('Offline draft'), findsNothing);
    expect(find.byIcon(Icons.check_circle_outline), findsNothing);
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets(
    'saving a link survives dialog exit and remove snackbar expires',
    (tester) async {
      final store = MemoryDraftStore();
      await showApp(tester, store: store);
      await page(tester, 'Links');
      await visible(tester, find.byTooltip('Add a custom link'));
      await tester.tap(find.byTooltip('Add a custom link'));
      await tester.pump(const Duration(milliseconds: 300));
      await tester.enterText(find.byType(TextFormField).first, 'Work');
      await tester.enterText(find.byType(TextFormField).last, 'https://A.B/X');
      tester.testTextInput.hide();
      await tester.pump();
      await tester.tap(find.text('Save link'));
      await tester.pump(const Duration(seconds: 1));
      await tester.pump(const Duration(milliseconds: 600));
      expect((await store.load()).links.single.label, 'Work');
      expect(tester.takeException(), isNull);
      await tester.tap(find.byTooltip('Links library'));
      await tester.pumpAndSettle();
      await visible(tester, find.byTooltip('Remove Work'));
      await tester.tap(find.byTooltip('Remove Work'));
      await tester.pump(const Duration(milliseconds: 300));
      expect(find.text('Work removed'), findsOneWidget);
      await tester.pump(const Duration(milliseconds: 300));
      await tester.pump(const Duration(seconds: 5));
      await tester.pump(const Duration(milliseconds: 300));
      expect(find.text('Work removed'), findsNothing);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    },
  );
  testWidgets(
    'URL warnings expire after one second but invalid links never save',
    (tester) async {
      final results = <SavedLink>[];
      await tester.pumpWidget(
        MaterialApp(
          home: Builder(
            builder: (context) => Scaffold(
              body: TextButton(
                onPressed: () async {
                  final value = await showDialog<SavedLink>(
                    context: context,
                    builder: (_) => const LinkEditorDialog(),
                  );
                  if (value != null) results.add(value);
                },
                child: const Text('Open'),
              ),
            ),
          ),
        ),
      );
      await tester.tap(find.text('Open'));
      await tester.pumpAndSettle();
      await tester.enterText(find.byType(TextFormField).first, 'Work');
      final field = find.byKey(const Key('link-url'));
      await tester.enterText(field, 'https://a.b/abcdef');
      await tester.pump();
      expect(
        find.text('Maximum 17 characters including HTTP(S)://.'),
        findsOneWidget,
      );
      await tester.pump(const Duration(milliseconds: 900));
      expect(
        find.text('Maximum 17 characters including HTTP(S)://.'),
        findsOneWidget,
      );
      await tester.pump(const Duration(milliseconds: 100));
      await tester.pump(const Duration(milliseconds: 250));
      expect(
        find.text('Maximum 17 characters including HTTP(S)://.'),
        findsNothing,
      );
      await tester.tap(find.text('Save link'));
      await tester.pump();
      expect(results, isEmpty);
      expect(find.byType(LinkEditorDialog), findsOneWidget);
      await tester.pump(const Duration(seconds: 1));
      await tester.pump(const Duration(milliseconds: 250));
      final input = tester.widget<TextField>(
        find.descendant(of: field, matching: find.byType(TextField)),
      );
      expect(input.decoration!.errorText, isNull);
      await tester.enterText(field, 'HTTPS://EXAMPLE.COM/ABCDEF');
      await tester.pump();
      expect(
        find.text('Maximum 25 characters including HTTP(S)://.'),
        findsOneWidget,
      );
      await tester.enterText(field, 'HTTPS://A.B/X');
      await tester.pump();
      expect(
        find.text('Maximum 25 characters including HTTP(S)://.'),
        findsNothing,
      );
      await tester.tap(find.text('Save link'));
      await tester.pumpAndSettle();
      expect(results.single.url, 'HTTPS://A.B/X');
      expect(tester.takeException(), isNull);
    },
  );
  testWidgets('text library persists and loads a separate saved design', (
    tester,
  ) async {
    final store = MemoryDraftStore();
    await showApp(tester, store: store);
    await page(tester, 'Text');
    await tester.enterText(find.byKey(const Key('text-message')), 'HELLO');
    tester.testTextInput.hide();
    await tester.pump();
    await visible(tester, find.byTooltip('Save to library'));
    await tester.tap(find.byTooltip('Save to library'));
    await tester.pump(const Duration(milliseconds: 300));
    await tester.enterText(find.byType(TextField).last, 'Greeting');
    await tester.pump();
    await tester.tap(find.text('Save'));
    await tester.pump(const Duration(seconds: 1));
    expect((await store.load()).library.single.name, 'Greeting');
    await capture(tester, 'polish-library');
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
  });
}
