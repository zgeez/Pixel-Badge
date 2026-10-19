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
  bool nfcWorking = false;
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
            ..setUint8(2, nfcWorking ? 1 : 0)
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
      final wide = uuid == BadgeField.artwork.uuid && data[0] == 2;
      final header = wide
          ? 9
          : uuid == BadgeField.artwork.uuid
          ? 5
          : 4;
      final start = header == 4 ? 0 : 1;
      final offset = wide
          ? ByteData.sublistView(data).getUint32(1, Endian.little)
          : data[start] | data[start + 1] << 8;
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

class BulkStudioLink extends StudioLink implements BulkArtworkLink {
  int invalidations = 0;
  @override
  Future<int> artworkPayloadSize() async => 244;
  @override
  void invalidateArtworkCache() {
    invalidations++;
  }
}

class ReadSetupLink extends BulkStudioLink implements ArtworkReadProgressLink {
  bool negotiated = false;
  void Function(double)? progress;
  @override
  Future<int> artworkPayloadSize() async {
    negotiated = true;
    return 244;
  }

  @override
  void setArtworkReadProgress(void Function(double)? listener) =>
      progress = listener;
  @override
  Future<Uint8List> read(String uuid) async {
    if (uuid == BadgeField.artwork.uuid) {
      expect(
        negotiated,
        isTrue,
        reason: 'Initial reconnect must negotiate before artwork reads',
      );
      progress?.call(0.5);
      progress?.call(1);
    }
    return super.read(uuid);
  }
}

void main() {
  test(
    'initial large-artwork read negotiates first and exposes read progress',
    () async {
      final link = ReadSetupLink(), badge = BadgeController();
      final values = <double>[];
      badge.addListener(() {
        if (badge.busy) values.add(badge.uploadProgress);
      });
      await badge.attach(
        link,
        capabilities: BadgeCapabilities.decode([2, 25, 32, 4, 4, 3, 128, 103]),
      );
      expect(badge.confirmed, isTrue);
      expect(values, containsAllInOrder([0.5, 1.0]));
      expect(link.progress, isNull);
      badge.dispose();
      await link.dispose();
    },
  );

  test(
    '128 raw grayscale frames cross 64 KiB with exact confirmed bulk readback',
    () async {
      final link = BulkStudioLink(), badge = BadgeController();
      final caps = BadgeCapabilities.decode([2, 25, 32, 4, 4, 3, 128, 103]);
      await badge.attach(link, capabilities: caps);
      final design = MatrixDesign(
        frames: List.generate(
          128,
          (frame) =>
              Uint8List.fromList(List.generate(1024, (p) => (p + frame) % 256)),
        ),
      );
      final value = design.encode(forBadge: true);
      expect(value.length, 131718);
      await badge.apply(BadgeField.artwork, value);
      expect(badge.values[BadgeField.artwork], value);
      expect(badge.confirmed, isTrue);
      expect(link.fragments, (131718 / 235).ceil());
      expect(link.invalidations, 1);
      badge.dispose();
      await link.dispose();
    },
  );
  test('older badges reject large projects before staging', () async {
    final link = StudioLink(), badge = BadgeController();
    await badge.attach(
      link,
      capabilities: BadgeCapabilities.decode([2, 25, 32, 4, 4, 3, 32, 39]),
    );
    final value = MatrixDesign(
      frames: List.generate(128, (_) => Uint8List(128)),
    ).encode();
    await expectLater(badge.apply(BadgeField.artwork, value), throwsStateError);
    expect(link.fragments, 0);
    badge.dispose();
    await link.dispose();
  });

  for (final error in [-13, -19]) {
    for (final explicitDisconnect in [false, true]) {
      test(
        'NFC diagnostic $error is cleared on badge switch, explicit disconnect=$explicitDisconnect',
        () async {
          final studio = StudioLink()
            ..nfcVerified = false
            ..nfcWorking = true
            ..nfcError = error;
          final legacy = FakeLink()..nfcVerified = false;
          final badge = BadgeController();
          try {
            await badge.attach(studio, capabilities: BadgeCapabilities.studio);
            expect(badge.nfcResult, error);
            expect(badge.nfcBusy, isTrue);
            expect(badge.nfcMessage, BadgeController.nfcFailureMessage(error));
            if (explicitDisconnect) {
              await badge.disconnect();
              expect(badge.nfcResult, 0);
              expect(badge.nfcBusy, isFalse);
              expect(badge.nfcVerified, isFalse);
            }
            await badge.attach(legacy);
            expect(badge.capabilities.hasStudio, isFalse);
            expect(badge.confirmed, isTrue);
            expect(badge.nfcVerified, isFalse);
            expect(badge.nfcResult, 0);
            expect(badge.nfcBusy, isFalse);
            expect(badge.nfcMessage, BadgeController.nfcRequiredMessage);
          } finally {
            badge.dispose();
            await studio.dispose();
            await legacy.dispose();
          }
        },
      );
    }
  }
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
