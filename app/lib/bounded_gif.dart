// SPDX-License-Identifier: MIT
import 'dart:math' as math;
import 'dart:typed_data';
import 'gif_input.dart';

/// GIF89a sampled compositor. No source-sized pixel or index buffer is created.
/// LZW uses 4096 entries; disposal uses at most two 256x256 RGBA canvases.
/// Retained frames are capped independently by the badge's frame limit.
class SampledGif {
  SampledGif(this.width, this.height, this.frames, this.durations, this.count);
  final int width, height, count;
  final List<Uint8List> frames;
  final List<int> durations;
}

SampledGif decodeSampledGif(GifInput input, {int maxFrames = 128}) {
  try {
    return _Gif(input, maxFrames).decode();
  } finally {
    input.close();
  }
}

class _Blocks {
  _Blocks(this.input);
  final GifInput input;
  int remaining = 0, bits = 0, value = 0;
  bool ended = false;
  int code(int size) {
    while (bits < size) {
      if (remaining == 0) {
        if (ended) throw const FormatException('Incomplete GIF pixels.');
        remaining = input.byte();
        if (remaining == 0) {
          ended = true;
          throw const FormatException('Incomplete GIF pixels.');
        }
      }
      value |= input.byte() << bits;
      remaining--;
      bits += 8;
    }
    final result = value & ((1 << size) - 1);
    value >>= size;
    bits -= size;
    return result;
  }

  void finish() {
    if (ended) return;
    input.skip(remaining);
    for (var n = input.byte(); n != 0; n = input.byte()) {
      input.skip(n);
    }
    ended = true;
  }
}

class _Gif {
  _Gif(this.input, this.limit);
  final GifInput input;
  final int limit;
  late int sourceWidth, sourceHeight, width, height;
  late Uint8List canvas;
  late List<int> xs, ys;
  List<int>? global;
  int transparent = -1, disposal = 0, delay = 150, expandedPixels = 0;
  final frames = <Uint8List>[], durations = <int>[];

  List<int> palette(int flags) => [
    for (var i = 0; i < 1 << ((flags & 7) + 1); i++)
      (input.byte() << 24) | (input.byte() << 16) | (input.byte() << 8) | 255,
  ];
  void blocks() => _Blocks(input).finish();
  void color(int at, int value) {
    canvas[at] = (value >> 24) & 255;
    canvas[at + 1] = (value >> 16) & 255;
    canvas[at + 2] = (value >> 8) & 255;
    canvas[at + 3] = value & 255;
  }

  SampledGif decode() {
    final signature = String.fromCharCodes([
      for (var i = 0; i < 6; i++) input.byte(),
    ]);
    if (signature != 'GIF87a' && signature != 'GIF89a') {
      throw const FormatException('Invalid GIF header.');
    }
    sourceWidth = input.word();
    sourceHeight = input.word();
    if (sourceWidth == 0 || sourceHeight == 0) {
      throw const FormatException('Empty GIF canvas.');
    }
    final ratio = math.min(1.0, 256 / math.max(sourceWidth, sourceHeight));
    width = (sourceWidth * ratio).round().clamp(1, 256);
    height = (sourceHeight * ratio).round().clamp(1, 256);
    canvas = Uint8List(width * height * 4);
    xs = [
      for (var x = 0; x < width; x++) ((x + .5) * sourceWidth / width).floor(),
    ];
    ys = [
      for (var y = 0; y < height; y++)
        ((y + .5) * sourceHeight / height).floor(),
    ];
    final flags = input.byte(), background = input.byte();
    input.byte();
    if (flags & 128 != 0) global = palette(flags);
    var count = 0;
    while (true) {
      final block = input.byte();
      if (block == 0x3b) break;
      if (block == 0x21) {
        final kind = input.byte();
        if (kind == 0xf9) {
          if (input.byte() != 4) {
            throw const FormatException('Invalid GIF control.');
          }
          final flags = input.byte();
          disposal = (flags >> 2) & 7;
          delay = input.word() * 10;
          if (delay == 0) delay = 150;
          final index = input.byte();
          transparent = flags & 1 != 0 ? index : -1;
          if (input.byte() != 0 || disposal > 3) {
            throw const FormatException('Invalid GIF control.');
          }
        } else {
          // Plain-text graphics cannot be represented as image frames here.
          if (kind == 1) {
            throw const FormatException('GIF text blocks are not supported.');
          }
          blocks();
        }
        continue;
      }
      if (block != 0x2c) throw const FormatException('Invalid GIF block.');
      final left = input.word(),
          top = input.word(),
          w = input.word(),
          h = input.word();
      final flags = input.byte();
      if (w == 0 ||
          h == 0 ||
          left + w > sourceWidth ||
          top + h > sourceHeight) {
        throw const FormatException('Invalid GIF frame bounds.');
      }
      final colors = flags & 128 != 0 ? palette(flags) : global;
      if (colors == null) throw const FormatException('GIF palette missing.');
      final minCode = input.byte();
      if (minCode < 2 || minCode > 8) {
        throw const FormatException('Invalid GIF codes.');
      }
      count++;
      if (frames.length >= limit) {
        blocks();
        continue;
      }
      // Bound CPU as well as memory for corrupt/decompression-bomb input.
      expandedPixels += w * h;
      if (expandedPixels > 256 * 1024 * 1024) {
        throw const FormatException(
          'This GIF is too complex. Export a smaller animation.',
        );
      }
      if (frames.isEmpty &&
          transparent < 0 &&
          global != null &&
          background < global!.length) {
        for (var i = 0; i < canvas.length; i += 4) {
          color(i, global![background]);
        }
      }
      final previous = disposal == 3 ? Uint8List.fromList(canvas) : null;
      pixels(left, top, w, h, flags & 64 != 0, colors, minCode);
      frames.add(Uint8List.fromList(canvas));
      durations.add(delay);
      if (disposal == 3) {
        canvas = previous!;
      } else if (disposal == 2) {
        final bg =
            transparent < 0 && global != null && background < global!.length
            ? global![background]
            : 0;
        for (var y = 0; y < height; y++) {
          if (ys[y] < top || ys[y] >= top + h) continue;
          for (var x = 0; x < width; x++) {
            if (xs[x] >= left && xs[x] < left + w) {
              color((y * width + x) * 4, bg);
            }
          }
        }
      }
      transparent = -1;
      disposal = 0;
      delay = 150;
    }
    if (frames.isEmpty) throw const FormatException('GIF contains no frames.');
    return SampledGif(width, height, frames, durations, count);
  }

  void pixels(
    int left,
    int top,
    int w,
    int h,
    bool interlaced,
    List<int> colors,
    int minimum,
  ) {
    final blocks = _Blocks(input),
        prefix = Uint16List(4096),
        suffix = Uint8List(4096),
        stack = Uint8List(4097);
    final clear = 1 << minimum, end = clear + 1;
    var size = minimum + 1, next = clear + 2, old = -1, first = 0;
    var position = 0, row = 0, pass = 0;
    // Only sampled output coordinates need painting, not a source-sized row.
    final sampleX = <int, int>{
      for (var x = 0; x < width; x++)
        if (xs[x] >= left && xs[x] < left + w) xs[x] - left: x,
    };
    final sampleY = <int, int>{
      for (var y = 0; y < height; y++)
        if (ys[y] >= top && ys[y] < top + h) ys[y] - top: y,
    };
    void emit(int value) {
      if (position >= w * h || value >= colors.length) {
        throw const FormatException('Invalid GIF pixels.');
      }
      final x = position % w, dx = sampleX[x], dy = sampleY[row];
      if (dx != null && dy != null && value != transparent) {
        color((dy * width + dx) * 4, colors[value]);
      }
      position++;
      if (position % w == 0) {
        if (!interlaced) {
          row++;
        } else {
          const starts = [0, 4, 2, 1], steps = [8, 8, 4, 2];
          row += steps[pass];
          while (row >= h && pass < 3) {
            row = starts[++pass];
          }
        }
      }
    }

    while (true) {
      var code = blocks.code(size);
      if (code == clear) {
        size = minimum + 1;
        next = clear + 2;
        old = -1;
        continue;
      }
      if (code == end) break;
      if (old == -1) {
        if (code >= clear) {
          throw const FormatException('Invalid GIF first code.');
        }
        emit(code);
        first = code;
        old = code;
        continue;
      }
      final original = code;
      var depth = 0;
      if (code == next) {
        stack[depth++] = first;
        code = old;
      } else if (code > next) {
        throw const FormatException('Invalid GIF dictionary code.');
      }
      while (code >= clear) {
        if (code >= next || depth >= 4096) {
          throw const FormatException('Invalid GIF dictionary.');
        }
        stack[depth++] = suffix[code];
        code = prefix[code];
      }
      first = code;
      stack[depth++] = first;
      while (depth > 0) {
        emit(stack[--depth]);
      }
      if (next < 4096) {
        prefix[next] = old;
        suffix[next++] = first;
        if (next == 1 << size && size < 12) size++;
      }
      old = original;
    }
    if (position != w * h) throw const FormatException('Incomplete GIF frame.');
    blocks.finish();
  }
}
