// SPDX-License-Identifier: MIT
import 'dart:io';
import 'dart:typed_data';
import 'dart:ui' as ui;
import 'package:badge_app/badge_controller.dart';
import 'package:badge_app/draft_store.dart';
import 'package:badge_app/image_import.dart';
import 'package:badge_app/matrix_widgets.dart';
import 'package:badge_app/selection_control.dart';
import 'package:badge_app/studio_home.dart';
import 'package:badge_app/studio_model.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'controller_test.dart' show FakeLink;
import 'screen_test.dart' show showApp, page;

class CountingDesign extends MatrixDesign {
  CountingDesign()
    : super(frames: [for (var i = 0; i < 128; i++) Uint8List(1024)]);
  int encodes = 0;
  @override
  Uint8List encode({bool forBadge = false}) {
    encodes++;
    return super.encode(forBadge: forBadge);
  }
}

Uint8List tinyGif(int count, {int size = 1}) {
  final bytes = Uint8List.fromList([
    ...'GIF89a'.codeUnits,
    size & 255,
    size >> 8,
    size & 255,
    size >> 8,
    0x80,
    0,
    0,
    0,
    0,
    0,
    255,
    255,
    255,
    for (var i = 0; i < count; i++) ...[
      0x21,
      0xf9,
      4,
      0,
      10,
      0,
      0,
      0,
      0x2c,
      0,
      0,
      0,
      0,
      1,
      0,
      1,
      0,
      0,
      2,
      2,
      0x44,
      1,
      0,
    ],
    0x3b,
  ]);
  return bytes;
}

void main() {
  testWidgets(
    'saved Bounce selection stays visible but unavailable on legacy badges',
    (tester) async {
      final link = FakeLink(), badge = BadgeController();
      await badge.attach(link);
      final store = MemoryDraftStore();
      await store.save(
        StudioDraft(text: const TextDesign(text: 'ABCDEFG', layout: 3)),
      );
      await showApp(tester, controller: badge, store: store);
      await page(tester, 'Text');
      expect(tester.takeException(), isNull);
      final selector = tester.widget<SelectionControl<int>>(
        find.byWidgetPredicate(
          (w) => w is SelectionControl<int> && w.title == 'Text layout',
        ),
      );
      expect(selector.value, 3);
      expect(selector.choices.last.enabled, isFalse);
      await tester.tap(find.byTooltip('Text layout'));
      await tester.pumpAndSettle();
      expect(
        find.text('Bounce lines (firmware update needed)'),
        findsOneWidget,
      );
      await tester.tap(find.text('Scroll'));
      await tester.pumpAndSettle();
      final dynamic state = tester.state(find.byType(BadgeHome));
      expect(state.draft.text.layout, 2);
      await tester.pumpWidget(const SizedBox());
      badge.dispose();
      await link.dispose();
    },
  );

  testWidgets(
    'continuous drawing does not recompress or autosave the clip during the stroke',
    (tester) async {
      await showApp(tester);
      await page(tester, 'Studio');
      final dynamic state = tester.state(find.byType(BadgeHome));
      final design = CountingDesign();
      state.draft.matrix = design;
      state.editDraft();
      await tester.pump();
      expect(design.encodes, 0);
      final canvas = find.byType(MatrixEditor);
      final rect = tester.getRect(canvas);
      final gesture = await tester.startGesture(rect.center);
      await tester.pump();
      final initial =
          design.encodes; // One undo checkpoint per stroke is allowed.
      for (var i = 0; i < 12; i++) {
        await gesture.moveTo(rect.center + Offset(i.toDouble(), 4));
        await tester.pump(const Duration(milliseconds: 60));
      }
      expect(design.encodes, initial);
      expect(state.artworkHasPixels, isTrue);
      await gesture.up();
      await tester.pump();
      // Finish any off-thread size computation before disposing the widget.
      await tester.runAsync(
        () => Future<void>.delayed(const Duration(milliseconds: 250)),
      );
      await tester.pump();
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets(
    'GIF logical canvas cannot allocate a source-sized native frame',
    (tester) async {
      await tester.runAsync(() async {
        // Native RGBA would require ~16 GiB; only a 1x1 patch is encoded.
        final image = await decodeRaster(tinyGif(2, size: 65535));
        expect(image.frames.length, 2);
        expect(
          image.frames.every(
            (f) => f.width == 256 && f.height == 256 && f.rgba.length == 262144,
          ),
          isTrue,
        );
        final full = await decodeRasterFile(
          'test/fixtures/gif-large-frame.gif',
        );
        expect(full.frames.single.width, 256);
        expect(full.frames.single.height, 5);
        expect(full.frames.single.rgba.every((v) => v == 255), isTrue);
        final dir = await Directory('build').createTemp('bounded-gif-');
        try {
          final file = await File(
            '${dir.path}/large.gif',
          ).writeAsBytes(tinyGif(129));
          final fromFile = await decodeRasterFile(file.path);
          expect(fromFile.sourceCount, 129);
          expect(fromFile.frames.length, 128);
        } finally {
          await dir.delete(recursive: true);
        }
        await expectLater(
          decodeRaster(tinyGif(1).sublist(0, 25)),
          throwsFormatException,
        );
      });
    },
  );

  for (final name in ['gif-disposal', 'gif-dictionary', 'gif-interlaced']) {
    testWidgets(
      'bounded $name matches native unscaled composition and timing',
      (tester) async {
        await tester.runAsync(() async {
          final bytes = await File('test/fixtures/$name.gif').readAsBytes();
          final sampled = await decodeRaster(bytes);
          final codec = await ui.instantiateImageCodec(bytes);
          try {
            expect(sampled.sourceCount, codec.frameCount);
            for (var i = 0; i < codec.frameCount; i++) {
              final frame = await codec.getNextFrame();
              try {
                final raw = (await frame.image.toByteData(
                  format: ui.ImageByteFormat.rawStraightRgba,
                ))!;
                final expected = raw.buffer.asUint8List(
                  raw.offsetInBytes,
                  raw.lengthInBytes,
                );
                final actual = sampled.frames[i];
                expect(
                  actual.duration,
                  frame.duration.inMilliseconds == 0
                      ? 150
                      : frame.duration.inMilliseconds,
                );
                expect(actual.width, frame.image.width);
                expect(actual.height, frame.image.height);
                for (var pixel = 0; pixel < expected.length; pixel += 4) {
                  expect(
                    actual.rgba[pixel + 3],
                    expected[pixel + 3],
                    reason: 'frame $i alpha at $pixel',
                  );
                  if (expected[pixel + 3] != 0) {
                    expect(
                      actual.rgba.sublist(pixel, pixel + 3),
                      expected.sublist(pixel, pixel + 3),
                      reason: 'frame $i at $pixel',
                    );
                  }
                }
              } finally {
                frame.image.dispose();
              }
            }
          } finally {
            codec.dispose();
          }
        });
      },
    );
  }
}
