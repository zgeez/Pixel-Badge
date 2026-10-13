// SPDX-License-Identifier: MIT
import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';
import 'package:badge_app/badge_controller.dart';
import 'package:badge_app/protocol.dart';
import 'package:flutter_test/flutter_test.dart';

class FakeLink implements BadgeLink {
  final results = StreamController<Uint8List>.broadcast(sync: true);
  final online = StreamController<bool>.broadcast(sync: true);
  final values = <String, Uint8List>{
    BadgeField.url.uuid: Uint8List.fromList(ascii.encode('HTTPS://A.B/X')),
    BadgeField.name.uuid: Uint8List.fromList(ascii.encode('BADGE')),
    BadgeField.brightness.uuid: Uint8List.fromList([128]),
    BadgeField.mode.uuid: Uint8List.fromList([0]),
  };
  final staged = <String, Uint8List>{};
  int id = 0, field = 0, phase = 0, result = 0;
  int rejection = 0, applies = 0;
  bool nfcVerified = true;
  bool mismatch = false,
      failRead = false,
      dropNotification = false,
      loseConnection = false,
      staleFirst = false;
  Uint8List status({int? overrideId, int? overrideField}) {
    final data = ByteData(12)
      ..setUint8(0, 1)
      ..setUint32(1, overrideId ?? id, Endian.little)
      ..setUint8(5, overrideField ?? field)
      ..setUint8(6, phase)
      ..setInt32(7, result, Endian.little)
      ..setUint8(11, nfcVerified ? 3 : 1);
    return data.buffer.asUint8List();
  }

  @override
  Future<Uint8List> read(String uuid) async {
    if (failRead) throw StateError('Transport read failed.');
    return uuid == statusUuid ? status() : values[uuid]!;
  }

  @override
  Future<void> write(String uuid, Uint8List data) async {
    if (uuid != applyUuid) {
      if (uuid == BadgeField.name.uuid) {
        final previous = data[0] == 0 ? <int>[] : staged[uuid]!.toList();
        staged[uuid] = Uint8List.fromList([...previous, ...data.sublist(2)]);
      } else {
        staged[uuid] = data;
      }
      return;
    }
    ++applies;
    id = ByteData.sublistView(data).getUint32(1, Endian.little);
    field = data[5];
    phase = 2;
    result = rejection;
    if (staleFirst) {
      results.add(status(overrideId: id + 99));
      results.add(status(overrideField: field == 1 ? 3 : 1));
      await Future<void>.delayed(const Duration(milliseconds: 10));
    }
    final setting = BadgeField.values.firstWhere((v) => v.wire == field);
    if (!mismatch && rejection == 0) {
      values[setting.uuid] = staged[setting.uuid]!;
    }
    if (loseConnection) {
      online.add(false);
    } else if (!dropNotification) {
      results.add(status());
    }
  }

  @override
  Stream<Uint8List> get statuses => results.stream;
  @override
  Stream<bool> get connections => online.stream;
  @override
  Future<void> close() async {}
  Future<void> dispose() async {
    await results.close();
    await online.close();
  }
}

void main() {
  for (final lostAfterRefresh in [false, true]) {
    test(
      'missing NFC prevents URL writes, lost after refresh=$lostAfterRefresh',
      () async {
        final link = FakeLink()..nfcVerified = lostAfterRefresh;
        final controller = BadgeController();
        await controller.attach(link);
        link.nfcVerified = false;
        await expectLater(
          controller.apply(BadgeField.url, ascii.encode('HTTPS://A.B/Y')),
          throwsA(
            isA<StateError>().having(
              (e) => e.message,
              'message',
              BadgeController.nfcRequiredMessage,
            ),
          ),
        );
        expect(link.staged, isEmpty);
        expect(link.applies, 0);
        expect(controller.url, 'HTTPS://A.B/X');
        expect(controller.nfcVerified, isFalse);
        await controller.refresh();
        await controller.apply(BadgeField.name, ascii.encode('MY BADGE'));
        expect(controller.name, 'MY BADGE');
        expect(link.applies, 1);
        controller.dispose();
        await link.dispose();
      },
    );
  }
  test(
    'refresh failure clears confirmation and refresh excludes Apply',
    () async {
      final link = FakeLink();
      final controller = BadgeController();
      await controller.attach(link);
      final refresh = controller.refresh();
      await expectLater(
        controller.apply(BadgeField.mode, [1]),
        throwsStateError,
      );
      await refresh;
      expect(link.applies, 0);
      link.failRead = true;
      await expectLater(controller.refresh(), throwsStateError);
      expect(
        controller.confirmed || controller.writable || controller.busy,
        isFalse,
      );
      controller.dispose();
      await link.dispose();
    },
  );
  for (final dropped in [false, true]) {
    test('exact URL confirmation, notification dropped=$dropped', () async {
      final link = FakeLink()
        ..dropNotification = dropped
        ..staleFirst = true;
      final controller = BadgeController();
      await controller.attach(link);
      await controller.apply(BadgeField.url, ascii.encode('HTTPS://A.B/Y'));
      expect(controller.url, 'HTTPS://A.B/Y');
      expect(controller.confirmed && controller.nfcVerified, isTrue);
      expect(link.applies, 1);
      controller.dispose();
      await link.dispose();
    });
  }
  test(
    'rejection preserves readback and requires refresh before retry',
    () async {
      final link = FakeLink()..rejection = -13;
      final controller = BadgeController();
      await controller.attach(link);
      await expectLater(
        controller.apply(BadgeField.url, ascii.encode('HTTPS://A.B/Y')),
        throwsStateError,
      );
      expect(controller.url, 'HTTPS://A.B/X');
      expect(controller.confirmed, isFalse);
      expect(controller.message, contains('Run the NFC verifier'));
      expect(controller.nfcVerified, isFalse);
      await expectLater(
        controller.apply(BadgeField.url, ascii.encode('HTTPS://A.B/Y')),
        throwsStateError,
      );
      expect(link.applies, 1);
      await controller.refresh();
      expect(controller.confirmed, isTrue);
      controller.dispose();
      await link.dispose();
    },
  );
  test(
    'success response without exact readback cannot show active link',
    () async {
      final link = FakeLink()..mismatch = true;
      final controller = BadgeController();
      await controller.attach(link);
      await expectLater(
        controller.apply(BadgeField.url, ascii.encode('HTTPS://A.B/Y')),
        throwsStateError,
      );
      expect(controller.url, 'HTTPS://A.B/X');
      expect(controller.confirmed, isFalse);
      controller.dispose();
      await link.dispose();
    },
  );
  test('disconnect after Apply is indeterminate, never replayed', () async {
    final link = FakeLink()..loseConnection = true;
    final controller = BadgeController();
    await controller.attach(link);
    await expectLater(
      controller.apply(BadgeField.url, ascii.encode('HTTPS://A.B/Y')),
      throwsStateError,
    );
    expect(controller.connected || controller.confirmed, isFalse);
    expect(controller.url, 'HTTPS://A.B/X');
    expect(link.applies, 1);
    controller.dispose();
    await link.dispose();
  });
  test('long name uses staging fragments and one Apply', () async {
    final link = FakeLink();
    final controller = BadgeController();
    await controller.attach(link);
    const name = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ123456';
    await controller.apply(BadgeField.name, ascii.encode(name));
    expect(controller.name, name);
    expect(link.applies, 1);
    controller.dispose();
    await link.dispose();
  });
}
