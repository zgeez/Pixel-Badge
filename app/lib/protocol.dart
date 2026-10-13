// SPDX-License-Identifier: MIT
import 'dart:convert';
import 'dart:typed_data';

enum BadgeField {
  url(1, 4),
  name(2, 3),
  brightness(3, 5),
  mode(4, 6);

  const BadgeField(this.wire, this.characteristic);
  final int wire, characteristic;
  String get uuid => badgeUuid(characteristic);
}

String badgeUuid(int index) =>
    'f3b000${index.toRadixString(16).padLeft(2, '0')}-6e9b-4f4c-a8f8-731a9d46e1c0';
final serviceUuid = badgeUuid(1);
final infoUuid = badgeUuid(2);
final applyUuid = badgeUuid(7);
final statusUuid = badgeUuid(8);
const brightnessLevels = [16, 64, 128, 255];

String normalizeUrl(String value) => value.replaceFirstMapped(
  RegExp(r'^https?://', caseSensitive: false),
  (m) => m[0]!.toUpperCase(),
);

void validateValue(BadgeField field, List<int> value) {
  if (field == BadgeField.name) {
    if (value.isEmpty ||
        value.length > 32 ||
        value.any((v) => v < 32 || v > 126)) {
      throw const FormatException(
        'Name must contain 1–32 printable ASCII characters.',
      );
    }
  } else if (field == BadgeField.url) {
    final text = ascii.decode(value, allowInvalid: false);
    const allowed =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?#[]!\$&'()*+,;=%";
    final prefix = RegExp(r'^https?://', caseSensitive: false).firstMatch(text);
    if (value.isEmpty ||
        value.length > 20 ||
        prefix == null ||
        prefix.end == text.length ||
        '/?#:'.contains(text[prefix.end]) ||
        text.split('').any((c) => !allowed.contains(c)) ||
        RegExp(r'%(?![0-9a-fA-F]{2})').hasMatch(text)) {
      throw const FormatException(
        'Use an HTTP(S) link of at most 20 ASCII characters.',
      );
    }
    // Fixed V1-M limits: 20 characters in this application, 14 in byte mode.
    if (!RegExp(r'^[0-9A-Z $%*+./:\-]+$').hasMatch(text) && text.length > 14) {
      throw const FormatException(
        'This link does not fit the badge QR. Use a short link.',
      );
    }
  } else if (value.length != 1 ||
      (field == BadgeField.brightness
          ? !brightnessLevels.contains(value.single)
          : value.single > 1)) {
    throw const FormatException('Unsupported setting value.');
  }
}

List<Uint8List> stagePackets(BadgeField field, List<int> value) {
  validateValue(field, value);
  if (field != BadgeField.name) return [Uint8List.fromList(value)];
  return [
    for (var offset = 0; offset < value.length; offset += 18)
      Uint8List.fromList([
        offset,
        value.length,
        ...value.sublist(offset, (offset + 18).clamp(0, value.length)),
      ]),
  ];
}

Uint8List applyPacket(int id, BadgeField field) {
  if (id <= 0 || id > 0xffffffff) {
    throw const FormatException('Reconnect to reset request IDs.');
  }
  final data = ByteData(6)
    ..setUint8(0, 1)
    ..setUint32(1, id, Endian.little)
    ..setUint8(5, field.wire);
  return data.buffer.asUint8List();
}

class BadgeStatus {
  BadgeStatus(this.id, this.field, this.phase, this.result, this.flags);
  final int id, field, phase, result, flags;
  bool get writable => flags & 1 != 0;
  bool get nfcVerified => flags & 2 != 0;
  factory BadgeStatus.decode(List<int> bytes) {
    if (bytes.length != 12 || bytes[0] != 1) {
      throw const FormatException('Unsupported badge protocol.');
    }
    final data = ByteData.sublistView(Uint8List.fromList(bytes));
    final status = BadgeStatus(
      data.getUint32(1, Endian.little),
      bytes[5],
      bytes[6],
      data.getInt32(7, Endian.little),
      bytes[11],
    );
    if (status.phase > 2 ||
        status.flags & ~7 != 0 ||
        (status.phase == 0
            ? status.id != 0 || status.field != 0 || status.result != 0
            : status.id == 0 ||
                  status.field < 1 ||
                  status.field > 4 ||
                  (status.phase == 1
                      ? status.result != -115
                      : status.result > 0))) {
      throw const FormatException('Malformed badge status.');
    }
    return status;
  }
}

bool sameBytes(List<int> a, List<int> b) =>
    a.length == b.length &&
    List.generate(a.length, (i) => a[i] == b[i]).every((v) => v);

const linkLabels = ['Portfolio', 'LinkedIn', 'GitHub', 'Resume'];
const configuredLinks = [
  String.fromEnvironment('BADGE_URL_PORTFOLIO'),
  String.fromEnvironment('BADGE_URL_LINKEDIN'),
  String.fromEnvironment('BADGE_URL_GITHUB'),
  String.fromEnvironment('BADGE_URL_RESUME'),
];
