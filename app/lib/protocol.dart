// SPDX-License-Identifier: MIT
import 'dart:convert';
import 'dart:typed_data';
import 'qr_capacity.dart';
import 'artwork_codec.dart';

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
const brightnessLevels = [13, 64, 128, 255];
const legacyBrightnessLevels = [16, 64, 128, 255];
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
    this.maxUrlLength = 20,
    this.hasGrayscale = false,
    this.hasLargeArtwork = false,
    this.hasEditorV2 = false,
    this.maxFrames = 16,
  });
  static const legacy = BadgeCapabilities(1);
  static const studio = BadgeCapabilities(2);
  final int version, maxUrlLength, maxFrames;
  final bool hasGrayscale;
  final bool hasLargeArtwork, hasEditorV2;
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
        (bytes[1] != 20 && !(bytes[0] == 2 && bytes[1] == 25)) ||
        bytes[2] != 32 ||
        bytes[3] != 4 ||
        bytes[5] & ~7 != 0 ||
        (bytes[7] & 128 != 0 && bytes[7] & 64 == 0) ||
        (bytes[0] == 1
            ? bytes[4] != 2 || bytes[6] != 0 || bytes[7] != 0
            : !((bytes[4] == 4 && bytes[7] & ~224 == 7) ||
                      (bytes[4] == 6 && bytes[7] & ~224 == 15) ||
                      (bytes[4] == 7 && bytes[7] & ~224 == 31)) ||
                  bytes[6] !=
                      (bytes[7] & 64 != 0
                          ? 128
                          : bytes[7] & 32 != 0
                          ? 32
                          : 16))) {
      throw const FormatException(
        'This badge uses an unsupported configuration protocol.',
      );
    }
    return BadgeCapabilities(
      bytes[0],
      hasAudio: bytes[4] >= 6,
      hasMotion: bytes[4] == 7,
      maxUrlLength: bytes[1],
      hasGrayscale: bytes[7] & 32 != 0,
      hasLargeArtwork: bytes[7] & 64 != 0,
      hasEditorV2: bytes[7] & 128 != 0,
      maxFrames: bytes[6],
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

/// Paged readback, with explicit cursors and a stable declared size.
Future<Uint8List> readArtworkPages(
  Future<void> Function(Uint8List) writeCursor,
  Future<Uint8List> Function() readPage, {
  bool wide = false,
  bool sequential = false,
  void Function(double)? onProgress,
}) async {
  final output = <int>[];
  onProgress?.call(0);
  int? total;
  if (sequential) {
    await writeCursor(Uint8List.fromList([4]));
  }
  do {
    if (!sequential) {
      await writeCursor(
        wide
            ? (ByteData(5)
                    ..setUint8(0, 2)
                    ..setUint32(1, output.length, Endian.little))
                  .buffer
                  .asUint8List()
            : Uint8List.fromList([0, output.length & 255, output.length >> 8]),
      );
    }
    final page = await readPage();
    final header = wide ? 8 : 4;
    if (page.length <= header || page.length > (wide ? 244 : 20)) {
      throw const FormatException('Incomplete artwork readback.');
    }
    final raw = ByteData.sublistView(page);
    final offset = wide
            ? raw.getUint32(0, Endian.little)
            : raw.getUint16(0, Endian.little),
        size = wide
            ? raw.getUint32(4, Endian.little)
            : raw.getUint16(2, Endian.little);
    if (size < 4 ||
        size > maxArtworkBytes ||
        offset != output.length ||
        (total != null && total != size)) {
      throw const FormatException(
        'Artwork changed during readback. Refresh again.',
      );
    }
    total = size;
    output.addAll(page.sublist(header));
    if (output.length > total) {
      throw const FormatException('Malformed artwork page.');
    }
    onProgress?.call(output.length / total);
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
        value[1] > 3 ||
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
    decodeArtwork(value, forBadge: true);
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
        value.length > 25 ||
        prefix == null ||
        prefix.end == text.length ||
        '/?#:'.contains(text[prefix.end]) ||
        text.split('').any((c) => !allowed.contains(c)) ||
        RegExp(r'%(?![0-9a-fA-F]{2})').hasMatch(text)) {
      throw const FormatException(
        'Use a short HTTP(S) link: up to 25 QR alphanumeric characters or 17 ASCII bytes, including the scheme.',
      );
    }
    if (QrCapacity.fit(text) == null) {
      throw const FormatException(
        'Too long for 32 by 32: maximum 25 QR alphanumeric characters or 17 ASCII bytes, including HTTP(S)://. Use a shorter link.',
      );
    }
  } else if (value.length != 1 ||
      (field == BadgeField.brightness
          ? (!brightnessLevels.contains(value.single) &&
                !legacyBrightnessLevels.contains(value.single))
          : value.single > 6)) {
    throw const FormatException('Unsupported setting value.');
  }
}

List<Uint8List> stagePackets(
  BadgeField field,
  List<int> value, {
  bool wideArtwork = false,
  int payloadSize = 20,
}) {
  validateValue(field, value);
  if (field == BadgeField.artwork && wideArtwork) {
    final size = payloadSize.clamp(20, 244) - 9;
    return [
      for (var offset = 0; offset < value.length; offset += size)
        Uint8List.fromList([
          ...(ByteData(9)
                ..setUint8(0, 2)
                ..setUint32(1, offset, Endian.little)
                ..setUint32(5, value.length, Endian.little))
              .buffer
              .asUint8List(),
          ...value.sublist(offset, (offset + size).clamp(0, value.length)),
        ]),
    ];
  }
  if (field == BadgeField.artwork && value.length > 65535) {
    throw const FormatException('Update badge firmware for larger artwork.');
  }
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
  if (field != BadgeField.name &&
      !(field == BadgeField.url && value.length > 20)) {
    return [Uint8List.fromList(value)];
  }
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
