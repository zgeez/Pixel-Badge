// SPDX-License-Identifier: MIT
import 'dart:convert';
import 'package:flutter_test/flutter_test.dart';
import 'package:badge_app/protocol.dart';

void main() {
  test(
    'pending status uses canonical wire errno independent of target libc',
    () {
      final pending = [1, 1, 0, 0, 0, 1, 1, 0x8d, 0xff, 0xff, 0xff, 3];
      final status = BadgeStatus.decode(pending);
      expect(status.phase, 1);
      expect(status.result, -115);
      expect(status.nfcVerified, isTrue);
      pending[7] =
          0x89; // Target libc -119 is not the protocol's pending value.
      expect(() => BadgeStatus.decode(pending), throwsFormatException);
      expect(BadgeStatus.decode([1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3]).phase, 0);
    },
  );
  test('scheme normalization preserves destination bytes', () {
    expect(normalizeUrl('https://A.B/L'), 'HTTPS://A.B/L');
    expect(normalizeUrl('https://A.B/x'), 'HTTPS://A.B/x');
    expect(
      () =>
          validateValue(BadgeField.url, ascii.encode('HTTPS://EXAMPLE.COM/x')),
      throwsFormatException,
    );
  });
  test('name staging uses bounded contiguous packets at MTU 23', () {
    final packets = stagePackets(
      BadgeField.name,
      ascii.encode('ABCDEFGHIJKLMNOPQRSTUVWXYZ123456'),
    );
    expect(packets.map((p) => p.length), [20, 16]);
    expect(packets[0].sublist(0, 2), [0, 32]);
    expect(packets[1].sublist(0, 2), [18, 32]);
    expect(
      () => stagePackets(BadgeField.name, ascii.encode('')),
      throwsFormatException,
    );
  });
  test('apply packet and errno result are little endian', () {
    expect(applyPacket(0x12345678, BadgeField.url), [
      1,
      0x78,
      0x56,
      0x34,
      0x12,
      1,
    ]);
    final status = BadgeStatus.decode([
      1,
      1,
      0,
      0,
      0,
      1,
      2,
      0xf3,
      0xff,
      0xff,
      0xff,
      1,
    ]);
    expect(status.result, -13);
    expect(status.nfcVerified, isFalse);
    expect(
      () => BadgeStatus.decode([2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]),
      throwsFormatException,
    );
  });
}
