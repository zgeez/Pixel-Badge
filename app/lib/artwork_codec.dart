// SPDX-License-Identifier: MIT
import 'dart:typed_data';

const maxArtworkFrames = 128,
    maxArtworkBytes = 131718,
    maxLocalArtworkBytes = 131718;
Uint8List grayPixels(List<int> frame) {
  if (frame.length == 1024) return Uint8List.fromList(frame);
  if (frame.length != 128) throw const FormatException('Invalid pixel frame.');
  return Uint8List.fromList(
    List.generate(1024, (i) => frame[i ~/ 8] & (128 >> (i % 8)) != 0 ? 255 : 0),
  );
}

Uint8List _runs(List<int> values) {
  final result = <int>[];
  var offset = 0;
  while (offset < 1024) {
    var end = offset + 1;
    while (end < 1024 && values[end] == values[offset]) {
      end++;
    }
    result.addAll([(end - offset) & 255, (end - offset) >> 8, values[offset]]);
    offset = end;
  }
  return Uint8List.fromList(result);
}

Uint8List? _binary(List<int> values) {
  final out = Uint8List(128);
  for (var i = 0; i < 1024; i++) {
    if (values[i] != 0 && values[i] != 255) return null;
    if (values[i] != 0) out[i ~/ 8] |= 128 >> (i % 8);
  }
  return out;
}

Uint8List encodeArtwork(
  List<Uint8List> frames,
  int interval, {
  bool forBadge = false,
  List<int>? durations,
  int playback = 0,
}) {
  final timing = durations ?? List.filled(frames.length, interval);
  if (timing.length != frames.length ||
      timing.any((v) => v < 50 || v > 2000) ||
      playback < 0 ||
      playback > 3) {
    throw const FormatException('Invalid animation timing.');
  }
  if (frames.isEmpty ||
      frames.length > maxArtworkFrames ||
      interval < 50 ||
      interval > 2000) {
    throw const FormatException('Use 1-128 frames and a 50-2000 ms interval.');
  }
  final gray = frames.map(grayPixels).toList();
  final binary = gray.map(_binary).toList();
  if (playback == 0 &&
      timing.every((v) => v == interval) &&
      binary.every((v) => v != null)) {
    return Uint8List.fromList([
      1,
      frames.length,
      interval & 255,
      interval >> 8,
      ...binary.expand((v) => v!),
    ]);
  }
  final out = <int>[
    2,
    frames.length,
    interval & 255,
    interval >> 8,
    playback,
    0,
  ];
  for (var i = 0; i < frames.length; i++) {
    var mode = 1, data = gray[i];
    if (binary[i] != null) {
      mode = 0;
      data = binary[i]!;
    }
    final runs = _runs(gray[i]);
    if (runs.length < data.length) {
      mode = 2;
      data = runs;
    }
    if (i > 0) {
      final delta = _runs(
        List.generate(1024, (p) => gray[i][p] ^ gray[i - 1][p]),
      );
      if (delta.length < data.length) {
        mode = 3;
        data = delta;
      }
    }
    out.addAll([
      mode,
      data.length & 255,
      data.length >> 8,
      timing[i] & 255,
      timing[i] >> 8,
      ...data,
    ]);
  }
  if (out.length > (forBadge ? maxArtworkBytes : maxLocalArtworkBytes)) {
    throw FormatException(
      'Design needs ${out.length} bytes; the badge holds $maxArtworkBytes. Use fewer frames or simpler pixels.',
    );
  }
  return Uint8List.fromList(out);
}

class ArtworkClip {
  ArtworkClip(this.frames, this.durations, this.interval, this.playback);
  final List<Uint8List> frames;
  final List<int> durations;
  final int interval, playback;
}

List<Uint8List> decodeArtwork(List<int> value, {bool forBadge = false}) =>
    decodeArtworkClip(value, forBadge: forBadge).frames;
ArtworkClip decodeArtworkClip(List<int> value, {bool forBadge = false}) {
  final maxBytes = forBadge ? maxArtworkBytes : maxLocalArtworkBytes;
  if (value.length < 4 ||
      value.length > maxBytes ||
      value[1] < 1 ||
      value[1] > maxArtworkFrames ||
      value[2] + (value[3] << 8) < 50 ||
      value[2] + (value[3] << 8) > 2000) {
    throw const FormatException('Invalid artwork size or timing.');
  }
  if (value[0] == 1) {
    if (value.length != 4 + value[1] * 128) {
      throw const FormatException('Incomplete artwork.');
    }
    return ArtworkClip(
      [
        for (var offset = 4; offset < value.length; offset += 128)
          Uint8List.fromList(value.sublist(offset, offset + 128)),
      ],
      List.filled(value[1], value[2] + (value[3] << 8)),
      value[2] + (value[3] << 8),
      0,
    );
  }
  if (value[0] != 2 || value.length < 6 || value[4] > 3 || value[5] != 0) {
    throw const FormatException('Unknown artwork format.');
  }
  final frames = <Uint8List>[];
  final durations = <int>[];
  var offset = 6;
  for (var i = 0; i < value[1]; i++) {
    if (offset + 5 > value.length) {
      throw const FormatException('Incomplete frame.');
    }
    final mode = value[offset],
        size = value[offset + 1] + (value[offset + 2] << 8);
    final duration = value[offset + 3] + (value[offset + 4] << 8);
    if (duration < 50 || duration > 2000) {
      throw const FormatException('Invalid frame duration.');
    }
    durations.add(duration);
    offset += 5;
    if (offset + size > value.length) {
      throw const FormatException('Incomplete pixels.');
    }
    final data = value.sublist(offset, offset + size);
    Uint8List pixels;
    if (mode == 0 && size == 128) {
      pixels = grayPixels(data);
    } else if (mode == 1 && size == 1024) {
      pixels = Uint8List.fromList(data);
    } else if ((mode == 2 || (mode == 3 && i > 0)) &&
        size > 0 &&
        size % 3 == 0) {
      pixels = mode == 3 ? Uint8List.fromList(frames.last) : Uint8List(1024);
      var at = 0;
      for (var n = 0; n < size; n += 3) {
        final count = data[n] + (data[n + 1] << 8), shade = data[n + 2];
        if (count == 0 || at + count > 1024) {
          throw const FormatException('Invalid pixel run.');
        }
        for (var p = at; p < at + count; p++) {
          pixels[p] = mode == 3 ? pixels[p] ^ shade : shade;
        }
        at += count;
      }
      if (at != 1024) throw const FormatException('Incomplete pixel run.');
    } else {
      throw const FormatException('Invalid frame encoding.');
    }
    frames.add(pixels);
    offset += size;
  }
  if (offset != value.length) {
    throw const FormatException('Unexpected trailing pixels.');
  }
  return ArtworkClip(frames, durations, value[2] + (value[3] << 8), value[4]);
}

final _artworkCrcTable = List<int>.generate(256, (n) {
  var crc = n;
  for (var bit = 0; bit < 8; bit++) {
    crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xedb88320 : 0);
  }
  return crc;
});
int artworkCrc32(List<int> bytes) {
  var crc = 0xffffffff;
  for (final byte in bytes) {
    crc = (crc >> 8) ^ _artworkCrcTable[(crc ^ byte) & 255];
  }
  return (crc ^ 0xffffffff) & 0xffffffff;
}
