// SPDX-License-Identifier: MIT
import 'dart:convert';
import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:badge_app/protocol.dart';
import 'package:badge_app/studio_model.dart';
import 'package:badge_app/draft_store.dart';

void main() {
  test('saved link labels use the editor grapheme limit across snapshots', () {
    for (final grapheme in [
      '\u{1f600}',
      'e\u0301',
      '\u{1f469}\u200d\u{1f4bb}',
    ]) {
      final label = List.filled(32, grapheme).join();
      final draft = StudioDraft(links: [SavedLink(label, 'HTTPS://A.B/X')]);
      expect(StudioDraft.decode(draft.encode()).links.single.label, label);
      expect(
        () => SavedLink.fromJson({
          'label': label + grapheme,
          'url': 'HTTPS://A.B/X',
        }),
        throwsFormatException,
      );
    }
  });
  test('app frames match the firmware reference fixtures', () {
    final cases = [
      (const TextDesign(text: 'Hi!'), 0, 0x85cd5ee5),
      (
        const TextDesign(
          text: 'A\nb\nc\nD\nE',
          layout: 1,
          scale: 1,
          interval: 100,
        ),
        0,
        0x19a00905,
      ),
      (
        const TextDesign(
          text: 'A\nb\nc\nD\nE',
          layout: 1,
          scale: 1,
          interval: 100,
        ),
        2000,
        0x744791b8,
      ),
      (
        const TextDesign(
          text: 'Hello world!',
          layout: 2,
          scale: 2,
          interval: 90,
        ),
        1530,
        0x2897dd65,
      ),
      (const TextDesign(), 0, 0xd0fb099b),
    ];
    for (final (design, time, expected) in cases) {
      var hash = 2166136261;
      for (final byte in design.render(time)) {
        hash = ((hash ^ byte) * 16777619) & 0xffffffff;
      }
      expect(hash, expected);
    }
  });
  test('wire frame uses MSB-first rows and preserves all four corners', () {
    final art = MatrixDesign();
    for (final point in [(0, 0), (31, 0), (0, 31), (31, 31)]) {
      setPixel(art.current, point.$1, point.$2, true);
    }
    final bytes = art.encode();
    expect(bytes.sublist(0, 4), [1, 1, 150, 0]);
    expect(bytes[4], 128);
    expect(bytes[7], 1);
    expect(bytes[128], 128);
    expect(bytes[131], 1);
    expect(MatrixDesign.decode(bytes).current, art.current);
    setPixel(art.current, -1, 32, true);
    expect(art.encode(), bytes);
  });
  test('maximum animation uses bounded MTU-safe contiguous fragments', () {
    final art = MatrixDesign();
    while (art.frames.length < maxFrames) {
      art.add();
      setPixel(art.current, art.selected, art.selected, true);
    }
    final value = art.encode(),
        packets = stagePackets(BadgeField.artwork, value);
    expect(value.length, 16388);
    final assembled = <int>[];
    for (final packet in packets) {
      expect(packet.length, lessThanOrEqualTo(20));
      expect(packet[0], 1);
      expect(packet[1] + (packet[2] << 8), assembled.length);
      expect(packet[3] + (packet[4] << 8), 16388);
      assembled.addAll(packet.sublist(5));
    }
    expect(assembled, value);
    expect(() => art.add(), throwsStateError);
    final invalid = Uint8List.fromList(value)..[1] = 129;
    expect(() => MatrixDesign.decode(invalid), throwsFormatException);
  });
  test(
    'undo restores pixels and selected frame across duplication/removal',
    () {
      final art = MatrixDesign();
      setPixel(art.current, 2, 3, true);
      art.add(duplicate: true);
      expect(art.selected, 1);
      expect(pixel(art.current, 2, 3), true);
      art.undo();
      expect(art.frames.length, 1);
      expect(art.selected, 0);
      art.redo();
      expect(art.frames.length, 2);
      expect(art.selected, 1);
      art.remove();
      art.undo();
      expect(art.selected, 1);
      expect(art.frames.length, 2);
      art.flip(horizontal: true);
      expect(pixel(art.current, 29, 3), true);
      art.undo();
      expect(pixel(art.current, 2, 3), true);
    },
  );
  test('fill respects a closed border and cannot escape the canvas', () {
    final art = MatrixDesign();
    art.rectangle(4, 4, 10, 10);
    art.fill(7, 7);
    expect(pixel(art.current, 7, 7), true);
    expect(pixel(art.current, 3, 7), false);
    expect(pixel(art.current, 10, 10), true);
    expect(pixel(art.current, 11, 11), false);
    art.fill(7, 7);
    expect(pixel(art.current, 4, 4), false);
    art.line(0, 31, 31, 0);
    expect(pixel(art.current, 0, 31), true);
    expect(pixel(art.current, 31, 0), true);
  });
  test('text keeps case and pages multiline messages without truncating', () {
    const lower = TextDesign(text: 'Ab', scale: 1),
        upper = TextDesign(text: 'AB', scale: 1);
    expect(lower.render(0), isNot(upper.render(0)));
    const message = TextDesign(
      text: 'First\nSecond\nThird\nFourth\nFifth',
      layout: 1,
      scale: 1,
      interval: 150,
    );
    expect(message.render(0), isNot(message.render(3000)));
    expect(message.render(0), message.render(6000));
    const marquee = TextDesign(text: 'Hello World', layout: 2);
    expect(marquee.render(0), isNot(marquee.render(150)));
    expect(TextDesign.decode(message.encode()).text, message.text);
  });
  test('text validates timing, ASCII and fragmentation before transfer', () {
    expect(() => const TextDesign(text: 'é').encode(), throwsFormatException);
    expect(
      () => const TextDesign(interval: 49).encode(),
      throwsFormatException,
    );
    expect(() => TextDesign(text: 'a' * 97).encode(), throwsFormatException);
    final text = TextDesign(text: 'a' * 96).encode(),
        packets = stagePackets(BadgeField.text, text);
    expect(packets.every((p) => p.length <= 20), true);
    expect(packets.expand((p) => p.sublist(4)).toList(), text);
  });
  test(
    'offline drafts retain custom links, text and every animation frame',
    () async {
      final store = MemoryDraftStore(), draft = StudioDraft();
      draft.links.add(const SavedLink('Custom', 'HTTPS://A.B/x'));
      draft.text = const TextDesign(text: 'Hi\nthere', layout: 1);
      setPixel(draft.matrix.current, 3, 4, true);
      draft.matrix.add(duplicate: true);
      await store.save(draft);
      final read = await store.load();
      expect(read.links.single.url, 'HTTPS://A.B/x');
      expect(read.text.encode(), draft.text.encode());
      expect(read.matrix.encode(), draft.matrix.encode());
      final json = jsonDecode(draft.encode()) as Map<String, dynamic>;
      json['version'] = 99;
      expect(() => StudioDraft.decode(jsonEncode(json)), throwsFormatException);
    },
  );
  test(
    'capabilities gate Studio and v2 status accepts only defined fields',
    () {
      expect(
        BadgeCapabilities.decode([1, 20, 32, 4, 2, 3, 0, 0]).hasStudio,
        false,
      );
      expect(
        BadgeCapabilities.decode([2, 20, 32, 4, 4, 3, 16, 7]).hasStudio,
        true,
      );
      expect(
        () => BadgeCapabilities.decode([2, 20, 32, 4, 4, 3, 17, 7]),
        throwsFormatException,
      );
      expect(
        BadgeStatus.decode([2, 1, 0, 0, 0, 127, 2, 0, 0, 0, 0, 3]).field,
        127,
      );
      expect(
        () => BadgeStatus.decode([1, 1, 0, 0, 0, 127, 2, 0, 0, 0, 0, 3]),
        throwsFormatException,
      );
      expect(
        () => BadgeStatus.decode([2, 1, 0, 0, 0, 8, 2, 0, 0, 0, 0, 3]),
        throwsFormatException,
      );
    },
  );
}
