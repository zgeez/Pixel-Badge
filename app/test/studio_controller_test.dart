// SPDX-License-Identifier: MIT
import 'dart:typed_data';
import 'package:badge_app/badge_controller.dart';
import 'package:badge_app/protocol.dart';
import 'package:badge_app/studio_model.dart';
import 'package:flutter_test/flutter_test.dart';
import 'controller_test.dart' show FakeLink;

class StudioLink extends FakeLink {
  StudioLink() {
    values[BadgeField.text.uuid] = TextDesign().encode();
    values[BadgeField.artwork.uuid] = MatrixDesign().encode();
  }
  int fragments = 0;
  int nfcError = 0;
  int? disconnectAt;
  bool malformedNfc = false;
  @override
  Uint8List status({int? overrideId, int? overrideField}) {
    final value = super.status(
      overrideId: overrideId,
      overrideField: overrideField,
    );
    value[0] = 2;
    return value;
  }

  @override
  Future<Uint8List> read(String uuid) async {
    if (uuid == BadgeField.nfcSync.uuid) {
      return (ByteData(7)
            ..setUint8(0, malformedNfc ? 99 : 1)
            ..setUint8(1, nfcVerified ? 1 : 0)
            ..setInt32(3, nfcError, Endian.little))
          .buffer
          .asUint8List();
    }
    return super.read(uuid);
  }

  @override
  Future<void> write(String uuid, Uint8List data) async {
    if (uuid == BadgeField.text.uuid || uuid == BadgeField.artwork.uuid) {
      fragments++;
      final header = uuid == BadgeField.artwork.uuid ? 5 : 4;
      final start = header == 5 ? 1 : 0;
      final offset = data[start] | data[start + 1] << 8;
      expect(offset, offset == 0 ? 0 : staged[uuid]!.length);
      staged[uuid] = Uint8List.fromList([
        if (offset != 0) ...staged[uuid]!,
        ...data.sublist(header),
      ]);
      if (fragments == disconnectAt) online.add(false);
    } else {
      await super.write(uuid, data);
    }
  }
}

void main() {
  test(
    'maximum animation is staged, confirmed and read back exactly',
    () async {
      final link = StudioLink()..dropNotification = true;
      final badge = BadgeController();
      await badge.attach(link, capabilities: BadgeCapabilities.studio);
      final value = MatrixDesign(
        frames: List.generate(16, (i) => Uint8List(128)..[i] = 255),
      ).encode();
      await badge.apply(BadgeField.artwork, value);
      expect(link.fragments, 137);
      expect(link.applies, 1);
      expect(badge.values[BadgeField.artwork], value);
      expect(badge.confirmed, isTrue);
      expect(badge.uploadProgress, 1);
      badge.dispose();
      await link.dispose();
    },
  );
  for (final disconnectAt in [3, 9]) {
    test(
      'upload disconnect at $disconnectAt never issues or replays Apply',
      () async {
        final link = StudioLink()..disconnectAt = disconnectAt;
        final badge = BadgeController();
        await badge.attach(link, capabilities: BadgeCapabilities.studio);
        await expectLater(
          badge.apply(BadgeField.artwork, MatrixDesign().encode()),
          throwsStateError,
        );
        expect(link.fragments, disconnectAt);
        expect(link.applies, 0);
        expect(badge.confirmed || badge.connected, isFalse);
        await badge.attach(link, capabilities: BadgeCapabilities.studio);
        expect(link.applies, 0);
        expect(badge.values[BadgeField.artwork], MatrixDesign().encode());
        badge.dispose();
        await link.dispose();
      },
    );
  }
  test('legacy firmware gates Studio before any packet', () async {
    final link = FakeLink();
    final badge = BadgeController();
    await badge.attach(link);
    await expectLater(
      badge.apply(BadgeField.text, TextDesign(text: 'Hi').encode()),
      throwsStateError,
    );
    await expectLater(badge.apply(BadgeField.mode, [3]), throwsStateError);
    expect(link.staged, isEmpty);
    badge.dispose();
    await link.dispose();
  });
  test(
    'NFC policy failure requests repair; malformed state is unconfirmed',
    () async {
      final link = StudioLink()..rejection = -13;
      final badge = BadgeController();
      await badge.attach(link, capabilities: BadgeCapabilities.studio);
      await expectLater(badge.apply(BadgeField.nfcSync, [1]), throwsStateError);
      expect(badge.nfcVerified, isFalse);
      expect(badge.message, contains('NFC tag setup needs repair'));
      expect(badge.nfcResult, -13);
      link.malformedNfc = true;
      await expectLater(badge.refresh(), throwsFormatException);
      expect(badge.confirmed || badge.writable, isFalse);
      badge.dispose();
      await link.dispose();
    },
  );
  test(
    'paged readback handles maximum size and rejects changed cursor',
    () async {
      final value = MatrixDesign(
        frames: List.generate(16, (_) => Uint8List(128)),
      ).encode();
      var cursor = 0, pages = 0;
      final output = await readArtworkPages(
        (command) async {
          cursor = command[1] | command[2] << 8;
        },
        () async {
          pages++;
          return Uint8List.fromList([
            cursor & 255,
            cursor >> 8,
            value.length & 255,
            value.length >> 8,
            ...value.sublist(cursor, (cursor + 16).clamp(0, value.length)),
          ]);
        },
      );
      expect(output, value);
      expect(pages, 129);
      await expectLater(
        readArtworkPages(
          (_) async {},
          () async => Uint8List.fromList([1, 0, 132, 0, 1]),
        ),
        throwsFormatException,
      );
      await expectLater(
        readArtworkPages(
          (_) async {},
          () async => Uint8List.fromList([0, 0, 132, 0]),
        ),
        throwsFormatException,
      );
    },
  );
}
