// SPDX-License-Identifier: MIT
import 'dart:convert';
import 'dart:typed_data';

enum BadgeField {
  url(1, 4),
  name(2, 3),
  brightness(3, 5),
  mode(4, 6),
  text(5, 9),
  artwork(6, 10),
  motion(7, 12),
  nfcSync(127, 11);

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
final motionStatusUuid = badgeUuid(13);
const brightnessLevels = [16, 64, 128, 255];
const legacyFields = [
  BadgeField.url,
  BadgeField.name,
  BadgeField.brightness,
  BadgeField.mode,
];

class BadgeCapabilities {
  const BadgeCapabilities(
    this.version, {
    this.hasAudio = false,
    this.hasMotion = false,
  });
  static const legacy = BadgeCapabilities(1);
  static const studio = BadgeCapabilities(2);
  final int version;
  final bool hasAudio;
  final bool hasMotion;
  int get modeCount => hasMotion
      ? 7
      : hasAudio
      ? 6
      : hasStudio
      ? 4
      : 2;
  bool get hasStudio => version == 2;
  List<BadgeField> get settings => [
    ...legacyFields,
    if (hasStudio) ...[BadgeField.text, BadgeField.artwork],
    if (hasMotion) BadgeField.motion,
  ];
  factory BadgeCapabilities.decode(List<int> bytes) {
    if (bytes.length != 8 ||
        (bytes[0] != 1 && bytes[0] != 2) ||
        bytes[1] != 20 ||
        bytes[2] != 32 ||
        bytes[3] != 4 ||
        bytes[5] & ~7 != 0 ||
        (bytes[0] == 1
            ? bytes[4] != 2 || bytes[6] != 0 || bytes[7] != 0
            : !((bytes[4] == 4 && bytes[7] == 7) ||
                      (bytes[4] == 6 && bytes[7] == 15) ||
                      (bytes[4] == 7 && bytes[7] == 31)) ||
                  bytes[6] != 16)) {
      throw const FormatException(
        'This badge uses an unsupported configuration protocol.',
      );
    }
    return BadgeCapabilities(
      bytes[0],
      hasAudio: bytes[4] >= 6,
      hasMotion: bytes[4] == 7,
    );
  }
}

String normalizeUrl(String value) => value.replaceFirstMapped(
  RegExp(r'^https?://', caseSensitive: false),
  (m) => m[0]!.toUpperCase(),
);

List<int> encodeAscii(String value, {bool lineBreaks = false}) {
  if (value.codeUnits.any(
    (c) => !(lineBreaks && c == 10) && (c < 32 || c > 126),
  )) {
    throw const FormatException(
      'Use English letters, numbers and basic punctuation. Emoji and accented characters are not supported.',
    );
  }
  return ascii.encode(value);
}

/// Default-MTU readback, with explicit cursors and a stable declared size.
Future<Uint8List> readArtworkPages(
  Future<void> Function(Uint8List) writeCursor,
  Future<Uint8List> Function() readPage,
) async {
  final output = <int>[];
  int? total;
  do {
    await writeCursor(
      Uint8List.fromList([0, output.length & 255, output.length >> 8]),
    );
    final page = await readPage();
    if (page.length < 5 || page.length > 20) {
      throw const FormatException('Incomplete artwork readback.');
    }
    final offset = page[0] + (page[1] << 8), size = page[2] + (page[3] << 8);
    if (size < 132 ||
        size > 2052 ||
        offset != output.length ||
        (total != null && total != size)) {
      throw const FormatException(
        'Artwork changed during readback. Refresh again.',
      );
    }
    total = size;
    output.addAll(page.sublist(4));
    if (output.length > total) {
      throw const FormatException('Malformed artwork page.');
    }
  } while (output.length < total);
  final bytes = Uint8List.fromList(output);
  validateValue(BadgeField.artwork, bytes);
  return bytes;
}

void validateValue(BadgeField field, List<int> value) {
  if (field == BadgeField.motion) {
    if (value.any((byte) => byte < 0 || byte > 255) ||
        value.length != 8 ||
        value[0] != 1 ||
        value[1] > 1 ||
        value[2] > 5 ||
        value[3] > 5 ||
        value[2] ~/ 2 == value[3] ~/ 2 ||
        value[4] < 1 ||
        value[4] > 5 ||
        value[5] < 16 ||
        value[5] > 192 ||
        value[6] > 3 ||
        value[7] != 0) {
      throw const FormatException('Unsupported motion settings.');
    }
    return;
  }
  if (field == BadgeField.text) {
    if (value.length < 6 ||
        value.length > 102 ||
        value[0] != 1 ||
        value[1] > 2 ||
        value[2] > 3 ||
        value[3] > 2 ||
        value[4] + (value[5] << 8) < 50 ||
        value[4] + (value[5] << 8) > 500 ||
        value.skip(6).any((v) => v != 10 && (v < 32 || v > 126))) {
      throw const FormatException(
        'Use up to 96 Latin characters, numbers, punctuation or line breaks.',
      );
    }
    return;
  }
  if (field == BadgeField.artwork) {
    if (value.length < 4 ||
        value[0] != 1 ||
        value[1] < 1 ||
        value[1] > 16 ||
        value.length != 4 + value[1] * 128 ||
        value[2] + (value[3] << 8) < 50 ||
        value[2] + (value[3] << 8) > 2000) {
      throw const FormatException(
        'Use 1–16 frames with a 50–2000 ms frame interval.',
      );
    }
    return;
  }
  if (field == BadgeField.nfcSync) {
    if (value.length != 1 || value[0] != 1) {
      throw const FormatException('Unsupported NFC action.');
    }
    return;
  }
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
          : value.single > 6)) {
    throw const FormatException('Unsupported setting value.');
  }
}

List<Uint8List> stagePackets(BadgeField field, List<int> value) {
  validateValue(field, value);
  if (field == BadgeField.text || field == BadgeField.artwork) {
    final size = field == BadgeField.artwork ? 15 : 16;
    return [
      for (var offset = 0; offset < value.length; offset += size)
        Uint8List.fromList([
          if (field == BadgeField.artwork) 1,
          offset & 255,
          offset >> 8,
          value.length & 255,
          value.length >> 8,
          ...value.sublist(offset, (offset + size).clamp(0, value.length)),
        ]),
    ];
  }
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

Uint8List applyPacket(int id, BadgeField field, {int version = 1}) {
  if (id <= 0 || id > 0xffffffff) {
    throw const FormatException('Reconnect to reset request IDs.');
  }
  final data = ByteData(6)
    ..setUint8(0, version)
    ..setUint32(1, id, Endian.little)
    ..setUint8(5, field.wire);
  return data.buffer.asUint8List();
}

class BadgeStatus {
  BadgeStatus(
    this.id,
    this.field,
    this.phase,
    this.result,
    this.flags, {
    this.version = 1,
  });
  final int id, field, phase, result, flags, version;
  bool get writable => flags & 1 != 0;
  bool get nfcVerified => flags & 2 != 0;
  factory BadgeStatus.decode(List<int> bytes) {
    if (bytes.length != 12 || (bytes[0] != 1 && bytes[0] != 2)) {
      throw const FormatException('Unsupported badge protocol.');
    }
    final data = ByteData.sublistView(Uint8List.fromList(bytes));
    final status = BadgeStatus(
      data.getUint32(1, Endian.little),
      bytes[5],
      bytes[6],
      data.getInt32(7, Endian.little),
      bytes[11],
      version: bytes[0],
    );
    if (status.phase > 2 ||
        status.flags & ~7 != 0 ||
        (status.phase == 0
            ? status.id != 0 || status.field != 0 || status.result != 0
            : status.id == 0 ||
                  status.field < 1 ||
                  (status.field > (status.version == 1 ? 4 : 7) &&
                      !(status.version == 2 && status.field == 127)) ||
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
