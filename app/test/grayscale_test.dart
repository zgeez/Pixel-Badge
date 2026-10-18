// SPDX-License-Identifier: MIT
import 'dart:convert';
import 'dart:math';
import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:badge_app/artwork_codec.dart';
import 'package:badge_app/studio_model.dart';
import 'package:badge_app/studio_import.dart';
import 'package:badge_app/image_import.dart';
import 'package:badge_app/protocol.dart';

void main() {
  test('artwork CRC32 matches IEEE and detects byte changes', () {
    expect(artworkCrc32(ascii.encode('123456789')), 0xcbf43926);
    expect(artworkCrc32([1, 2, 3]), isNot(artworkCrc32([1, 2, 4])));
  });
  test(
    'wide readback uses 32-bit cursors and rejects changing totals',
    () async {
      final source = MatrixDesign(
        frames: List.generate(
          128,
          (f) => Uint8List.fromList(List.generate(1024, (p) => (f + p) % 256)),
        ),
      ).encode();
      var cursor = 0;
      final bytes = await readArtworkPages(
        (request) async {
          expect(request[0], 2);
          cursor = ByteData.sublistView(request).getUint32(1, Endian.little);
        },
        () async {
          return Uint8List.fromList([
            ...(ByteData(8)
                  ..setUint32(0, cursor, Endian.little)
                  ..setUint32(4, source.length, Endian.little))
                .buffer
                .asUint8List(),
            ...source.sublist(cursor, (cursor + 236).clamp(0, source.length)),
          ]);
        },
        wide: true,
      );
      expect(bytes, source);
      expect(cursor, greaterThan(65535));
    },
  );

  test(
    'firmware shared RLE and XOR-delta fixture is exact and rejects truncation',
    () {
      final fixture = Uint8List.fromList([
        2,
        2,
        150,
        0,
        0,
        0,
        2,
        3,
        0,
        100,
        0,
        0,
        4,
        64,
        3,
        6,
        0,
        250,
        0,
        1,
        0,
        192,
        255,
        3,
        0,
      ]);
      final design = MatrixDesign.decode(fixture);
      expect(pixelBrightness(design.frames[0], 0, 0), 64);
      expect(pixelBrightness(design.frames[1], 0, 0), 128);
      expect(pixelBrightness(design.frames[1], 1, 0), 64);
      expect(
        MatrixDesign.decode(design.encode(forBadge: true)).frames,
        design.frames,
      );
      expect(
        [
          design.frameAt(99),
          design.frameAt(100),
          design.frameAt(349),
          design.frameAt(350),
        ],
        [0, 1, 1, 0],
      );
      for (var n = 0; n < fixture.length; n++) {
        expect(
          () => decodeArtwork(fixture.sublist(0, n)),
          throwsFormatException,
        );
      }
      final bad = Uint8List.fromList(fixture)
        ..[11] = 0
        ..[12] = 0;
      expect(() => decodeArtwork(bad), throwsFormatException);
      expect(() => decodeArtwork([...fixture, 0]), throwsFormatException);
    },
  );
  test(
    'all 256 shades and timing playback survive portable data and library rename',
    () {
      final design = MatrixDesign(
        frames: [
          Uint8List.fromList(List.generate(1024, (i) => i % 256)),
          Uint8List(1024)..fillRange(0, 1024, 128),
          Uint8List(1024)..fillRange(0, 1024, 192),
        ],
        durations: [100, 200, 300],
      );
      final bytes = design.encode(forBadge: true);
      expect(
        grayPixels(MatrixDesign.decode(bytes).frames.first),
        design.frames.first,
      );
      design.playback = 1;
      expect(design.frameAt(900), 2);
      design.playback = 2;
      expect(
        [design.frameAt(0), design.frameAt(300), design.frameAt(500)],
        [2, 1, 0],
      );
      design.playback = 3;
      expect([design.frameAt(600), design.frameAt(800)], [1, 0]);
      final imported = parseStudioData(
        jsonEncode({
          'format': 'pixel-badge',
          'version': 2,
          'artwork': base64Encode(design.encode()),
        }),
      );
      expect(imported.encode(), design.encode());
      final saved = SavedDesign(
        id: 'same-id',
        name: 'Renamed animation',
        isText: false,
        bytes: design.encode(),
      );
      final draft = StudioDraft(
        matrix: design,
        library: [
          saved,
          SavedDesign(
            id: 'text',
            name: 'Renamed text',
            isText: true,
            bytes: const TextDesign(text: 'Hi').encode(),
          ),
        ],
      );
      final restored = StudioDraft.decode(draft.encode());
      expect(restored.library.map((v) => v.name), [
        'Renamed animation',
        'Renamed text',
      ]);
      expect(restored.library.first.id, 'same-id');
      expect(restored.library.first.bytes, design.encode());
    },
  );
  test(
    '128 raw gray frames fit while older capability limits remain explicit',
    () {
      final binary = MatrixDesign(
        frames: List.generate(32, (_) => Uint8List(128)),
      );
      expect(binary.encode(forBadge: true).length, 4100);
      final random = Random(77);
      final large = MatrixDesign(
        frames: List.generate(
          128,
          (_) => Uint8List.fromList(
            List.generate(1024, (_) => random.nextInt(256)),
          ),
        ),
      );
      expect(large.encode().length, 131718);
      expect(large.encode(forBadge: true), large.encode());
      final draft = StudioDraft(
        matrix: large,
        library: [
          SavedDesign(
            id: 'large',
            name: 'Complex',
            isText: false,
            bytes: large.encode(),
          ),
        ],
      );
      expect(
        StudioDraft.decode(draft.encode()).matrix.encode(),
        large.encode(),
      );
      final caps = BadgeCapabilities.decode([2, 25, 32, 4, 7, 3, 32, 63]);
      expect(caps.hasGrayscale, isTrue);
      expect(caps.maxFrames, 32);
      expect(
        BadgeCapabilities.decode([2, 25, 32, 4, 7, 3, 16, 31]).hasGrayscale,
        isFalse,
      );
    },
  );
  test(
    'grayscale brush, selection, transforms and undo preserve intensity and duration',
    () {
      final design = MatrixDesign()
        ..brush = 73
        ..brushSize = 3;
      design.checkpoint();
      design.paint(5, 5, 73);
      expect(design.current.length, 1024);
      expect(pixelBrightness(design.current, 4, 4), 73);
      design.select(4, 4, 6, 6);
      design.copySelection(cut: true);
      expect(pixelBrightness(design.current, 5, 5), 0);
      design.pasteSelection(atX: 10, atY: 10);
      expect(pixelBrightness(design.current, 11, 11), 73);
      design.rotate();
      expect(pixelBrightness(design.current, 20, 11), 73);
      design.undo();
      expect(pixelBrightness(design.current, 11, 11), 73);
      design.redo();
      expect(pixelBrightness(design.current, 20, 11), 73);
      design.durations[0] = 250;
      design.add(duplicate: true);
      expect(design.durations, [250, 250]);
      design.durations[1] = 700;
      design.reorder(-1);
      expect(design.durations, [700, 250]);
      design.remove();
      expect(design.durations, [250]);
      design.brushSize = 1;
      design.ellipse(2, 2, 8, 8);
      expect(pixelBrightness(design.current, 5, 2), 73);
    },
  );
  test(
    'image conversion keeps gray alpha, adjustments, fit and individual timings',
    () {
      final source = ImportedRaster([
        RasterFrame(1, 1, Uint8List.fromList([128, 128, 128, 255]), 100),
        RasterFrame(1, 1, Uint8List.fromList([128, 128, 128, 128]), 300),
      ], 2);
      final design = source.convert(grayscale: true);
      expect(design.durations, [100, 300]);
      expect(pixelBrightness(design.frames[0], 0, 0), 128);
      expect(pixelBrightness(design.frames[1], 0, 0), 64);
      expect(
        pixelBrightness(
          source.convert(grayscale: true, brightness: 30).frames.first,
          0,
          0,
        ),
        158,
      );
      expect(
        pixelBrightness(
          source.convert(grayscale: true, invert: true).frames.first,
          0,
          0,
        ),
        127,
      );
      final moved = source.convert(grayscale: true, offsetX: 1);
      expect(pixelBrightness(moved.frames.first, 0, 0), 0);
      expect(pixelBrightness(moved.frames.first, 31, 0), 128);
      expect(
        () => parseStudioData(jsonEncode([List.filled(1024, 256)])),
        throwsFormatException,
      );
      final external = parseStudioData(jsonEncode([List.filled(625, 97)]));
      expect(pixelBrightness(external.current, 0, 0), 97);
    },
  );
}
