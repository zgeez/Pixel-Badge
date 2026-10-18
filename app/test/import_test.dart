// SPDX-License-Identifier: MIT
import 'dart:io';
import 'dart:typed_data';
import 'package:badge_app/image_import.dart';
import 'package:badge_app/artwork_codec.dart';
import 'package:badge_app/studio_model.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

// Small compressed fixtures with large source dimensions, without a large bitmap.
Uint8List grayPng(int width, int height, {int metadataBytes = 0}) {
  Uint8List chunk(String type, List<int> data) {
    final payload = Uint8List.fromList([...type.codeUnits, ...data]);
    return Uint8List.fromList([
      ...(ByteData(4)..setUint32(0, data.length)).buffer.asUint8List(),
      ...payload,
      ...(ByteData(
        4,
      )..setUint32(0, artworkCrc32(payload))).buffer.asUint8List(),
    ]);
  }

  final header = ByteData(13)
    ..setUint32(0, width)
    ..setUint32(4, height)
    ..setUint8(8, 8);
  final rows = Uint8List((width + 1) * height)
    ..fillRange(0, (width + 1) * height, 192);
  for (var y = 0; y < height; y++) {
    rows[y * (width + 1)] = 0;
  }
  final out = BytesBuilder(copy: false)
    ..add([137, 80, 78, 71, 13, 10, 26, 10])
    ..add(chunk('IHDR', header.buffer.asUint8List()));
  if (metadataBytes != 0) {
    out.add(
      chunk('tEXt', [78, 111, 116, 101, 0, ...List.filled(metadataBytes, 65)]),
    );
  }
  return (out
        ..add(chunk('IDAT', ZLibEncoder().convert(rows)))
        ..add(chunk('IEND', [])))
      .takeBytes();
}

RasterFrame dotGrid({int size = 250, double padding = .05}) {
  final rgba = Uint8List(size * size * 4);
  final pitch = size * (1 - padding * 2) / 25;
  for (var y = 0; y < size; y++) {
    for (var x = 0; x < size; x++) {
      final gx = (x + .5 - size * padding) / pitch;
      final gy = (y + .5 - size * padding) / pitch;
      var value = 0;
      if (gx >= 0 &&
          gx < 25 &&
          gy >= 0 &&
          gy < 25 &&
          (gx % 1 - .5).abs() < .35 &&
          (gy % 1 - .5).abs() < .35) {
        value = 64 + (gx.floor() * 7 + gy.floor() * 11) % 192;
      }
      final i = (y * size + x) * 4;
      rgba.setRange(i, i + 4, [value, value, value, 255]);
    }
  }
  return RasterFrame(size, size, rgba, 150);
}

Future<void> waitForImport(WidgetTester tester) async {
  for (var i = 0; i < 200; i++) {
    await tester.pump(const Duration(milliseconds: 20));
    await tester.runAsync(
      () => Future<void>.delayed(const Duration(milliseconds: 10)),
    );
    final button = find.ancestor(
      of: find.text('Use design'),
      matching: find.byWidgetPredicate((widget) => widget is FilledButton),
    );
    if (button.evaluate().isNotEmpty &&
        tester.widget<FilledButton>(button).onPressed != null) {
      return;
    }
  }
  fail('Import conversion did not finish.');
}

void main() {
  test('area filtering averages source gaps instead of aliasing stripes', () {
    final pixels = Uint8List(64 * 32 * 4);
    for (var y = 0; y < 32; y++) {
      for (var x = 0; x < 64; x++) {
        final value = x.isEven ? 0 : 255;
        pixels.setRange((y * 64 + x) * 4, (y * 64 + x) * 4 + 4, [
          value,
          value,
          value,
          255,
        ]);
      }
    }
    final frame = rasterize(
      RasterFrame(64, 32, pixels, 150),
      grayscale: true,
      crop: false,
    );
    expect(frame.sublist(8 * 32, 24 * 32).every((v) => v == 128), isTrue);
    expect(frame.sublist(0, 8 * 32).every((v) => v == 0), isTrue);
  });

  test(
    'rendered glyph detection removes gaps and preserves every logical LED',
    () {
      final source = ImportedRaster([dotGrid()], 1);
      final padding = detectGlyphPadding(source);
      expect(padding, closeTo(.05, .005));
      final frame = source
          .convert(grayscale: true, glyphPadding: padding)
          .current;
      for (var y = 0; y < 25; y++) {
        for (var x = 0; x < 25; x++) {
          expect(
            pixelBrightness(frame, x + 3, y + 3),
            closeTo(64 + (x * 7 + y * 11) % 192, 1),
          );
        }
      }
      expect(pixelBrightness(frame, 2, 3), 0);
      expect(pixelBrightness(frame, 28, 27), 0);
    },
  );

  test(
    'grid detection uses more than the first frame and rejects flat images',
    () {
      final empty = RasterFrame(250, 250, Uint8List(250 * 250 * 4), 100);
      expect(detectGlyphPadding(ImportedRaster([empty], 1)), isNull);
      expect(
        detectGlyphPadding(ImportedRaster([empty, dotGrid()], 2)),
        closeTo(.05, .005),
      );
      expect(
        detectGlyphPadding(ImportedRaster([dotGrid(size: 32)], 1)),
        isNull,
      );
      expect(
        () => rasterize(dotGrid(), glyphPadding: .2),
        throwsFormatException,
      );
    },
  );

  testWidgets('large photos resize automatically and preserve aspect ratio', (
    tester,
  ) async {
    await tester.runAsync(() async {
      for (final size in [(6000, 3000), (100, 7000)]) {
        final decoded = await decodeRaster(grayPng(size.$1, size.$2));
        final first = decoded.frames.single;
        expect(first.width, lessThanOrEqualTo(256));
        expect(first.height, lessThanOrEqualTo(256));
        expect(first.width / first.height, closeTo(size.$1 / size.$2, .01));
        expect(first.rgba.length, first.width * first.height * 4);
        final design = decoded.convert(grayscale: true);
        expect(MatrixDesign.decode(design.encode()).frames, design.frames);
      }
    });
  });

  testWidgets('file import accepts a source larger than eight megabytes', (
    tester,
  ) async {
    await tester.runAsync(() async {
      final parent = await Directory(
        'build/import-fixtures',
      ).create(recursive: true);
      final directory = await parent.createTemp('source-');
      try {
        final file = File('${directory.path}/large-source.png');
        await file.writeAsBytes(
          grayPng(5000, 32, metadataBytes: 8 * 1024 * 1024 + 1),
        );
        expect(await file.length(), greaterThan(8 * 1024 * 1024));
        final image = await decodeRasterFile(file.path);
        expect(image.frames.single.width, 256);
        expect(image.frames.single.height, 2);
        expect(image.convert(grayscale: true).frames.length, 1);
        await file.writeAsBytes([1, 2, 3]);
        await expectLater(decodeRasterFile(file.path), throwsFormatException);
        await expectLater(
          decodeRasterFile('${directory.path}/missing.png'),
          throwsFormatException,
        );
      } finally {
        await directory.delete(recursive: true);
      }
    });
  });

  testWidgets('large GIF frames are resized instead of rejected', (
    tester,
  ) async {
    await tester.runAsync(() async {
      const hex =
          '47494638396101000100800000000000ffffff21ff0b4e45545343415045322e30030100000021f904000a0000002c000000000100010000020244010021f90400140000002c00000000010001000002024c01003b';
      final bytes = Uint8List.fromList([
        for (var i = 0; i < hex.length; i += 2)
          int.parse(hex.substring(i, i + 2), radix: 16),
      ]);
      final header = ByteData.sublistView(bytes)
        ..setUint16(6, 5000, Endian.little)
        ..setUint16(8, 100, Endian.little);
      final image = await decodeRaster(header.buffer.asUint8List());
      expect(image.sourceCount, 2);
      expect(image.frames.length, 2);
      expect(
        image.frames.every((f) => f.width <= 256 && f.height <= 256),
        isTrue,
      );
      expect(image.frames.map((f) => f.duration), [100, 200]);
    });
  });

  // Optional real-file check. Owner media stays outside the repository.
  const mediaRoot = String.fromEnvironment('GLYPH_REVIEW');
  if (mediaRoot.isNotEmpty) {
    testWidgets('cached owner glyphs detect grids; ordinary GIFs stay images', (
      tester,
    ) async {
      await tester.runAsync(() async {
        for (var i = 0; i < 4; i++) {
          final image = await decodeRaster(
            await File('$mediaRoot/source-$i.gif').readAsBytes(),
          );
          final padding = detectGlyphPadding(image);
          expect(padding != null, i >= 2, reason: 'source-$i');
          expect(image.frames.length, lessThanOrEqualTo(128));
          final frame = image.frames.first;
          expect(frame.width, lessThanOrEqualTo(256));
          final result = image.convert(grayscale: true, glyphPadding: padding);
          expect(MatrixDesign.decode(result.encode()).frames, result.frames);
          // ignore: avoid_print
          print(
            'source-$i: ${image.sourceCount} frames; glyph inset=$padding; '
            '${result.encode().length} bytes',
          );
        }
      });
    });
  }
}
