// SPDX-License-Identifier: MIT
import 'dart:convert';
import 'dart:typed_data';
import 'package:characters/characters.dart';
import 'font5x8.dart';
import 'protocol.dart';

const matrixSize = 32, frameBytes = 128, maxFrames = 16;

enum PixelTool { pen, erase, line, rectangle, fill }

bool pixel(List<int> frame, int x, int y) =>
    x >= 0 &&
    x < 32 &&
    y >= 0 &&
    y < 32 &&
    frame[y * 4 + x ~/ 8] & (128 >> (x % 8)) != 0;
void setPixel(Uint8List frame, int x, int y, bool on) {
  if (x < 0 || x >= 32 || y < 0 || y >= 32) return;
  final index = y * 4 + x ~/ 8, mask = 128 >> (x % 8);
  frame[index] = on ? frame[index] | mask : frame[index] & ~mask;
}

class TextDesign {
  const TextDesign({
    this.text = '',
    this.layout = 0,
    this.scale = 0,
    this.alignment = 1,
    this.interval = 150,
  });
  final String text;
  final int layout, scale, alignment, interval;
  Uint8List encode() {
    final bytes = Uint8List.fromList([
      1,
      layout,
      scale,
      alignment,
      interval & 255,
      interval >> 8,
      ...encodeAscii(text, lineBreaks: true),
    ]);
    validateValue(BadgeField.text, bytes);
    return bytes;
  }

  factory TextDesign.decode(List<int> bytes) {
    validateValue(BadgeField.text, bytes);
    return TextDesign(
      text: ascii.decode(bytes.sublist(6)),
      layout: bytes[1],
      scale: bytes[2],
      alignment: bytes[3],
      interval: bytes[4] + (bytes[5] << 8),
    );
  }
  TextDesign copyWith({
    String? text,
    int? layout,
    int? scale,
    int? alignment,
    int? interval,
  }) => TextDesign(
    text: text ?? this.text,
    layout: layout ?? this.layout,
    scale: scale ?? this.scale,
    alignment: alignment ?? this.alignment,
    interval: interval ?? this.interval,
  );

  /// Integer rendering mirrors firmware/content.cpp, including pagination.
  Uint8List render(int elapsed, {String fallback = 'BADGE'}) {
    encode();
    final frame = Uint8List(frameBytes);
    final value = text.isEmpty ? fallback : text;
    if (value.isEmpty) return frame;
    List<String> wrap(int size) {
      final columns = 32 ~/ (6 * size), lines = <String>[];
      var cursor = 0;
      while (cursor < value.length && lines.length < 97) {
        final start = cursor;
        var end = start, space = value.length;
        while (end < value.length &&
            value.codeUnitAt(end) != 10 &&
            end - start < columns) {
          if (value.codeUnitAt(end) == 32) space = end;
          end++;
        }
        if (end < value.length &&
            value.codeUnitAt(end) != 10 &&
            end - start == columns &&
            space < end &&
            space > start) {
          end = space;
        }
        lines.add(value.substring(start, end));
        cursor = end;
        if (cursor < value.length &&
            (value.codeUnitAt(cursor) == 10 ||
                value.codeUnitAt(cursor) == 32)) {
          cursor++;
        }
      }
      return lines;
    }

    var size = scale;
    if (size == 0) {
      size = 1;
      if (layout == 0) {
        for (var candidate = 3; candidate >= 1; candidate--) {
          if (wrap(candidate).length <= 32 ~/ (8 * candidate)) {
            size = candidate;
            break;
          }
        }
      } else if (layout == 2 && !value.contains('\n')) {
        size = value.length == 1
            ? 3
            : value.length == 2
            ? 2
            : 1;
      }
    }
    int aligned(int width) => alignment == 0
        ? 0
        : alignment == 2
        ? 32 - width
        : ((32 - width) / 2).truncate();
    void letter(int character, int left, int top) {
      if (character < 32 || character > 126) return;
      for (var x = 0; x < 5; x++) {
        for (var y = 0; y < 8; y++) {
          if (font5x8[(character - 32) * 5 + x] & (1 << y) == 0) continue;
          for (var dy = 0; dy < size; dy++) {
            for (var dx = 0; dx < size; dx++) {
              setPixel(frame, left + x * size + dx, top + y * size + dy, true);
            }
          }
        }
      }
    }

    if (layout == 2) {
      final width = (value.length * 6 - 1) * size,
          scrolling = (value.length * 6 - 1) * size > 32;
      final left = scrolling
          ? -(elapsed ~/ interval % (width + 8))
          : aligned(width);
      for (var copy = 0; copy < (scrolling ? 2 : 1); copy++) {
        for (var i = 0; i < value.length; i++) {
          letter(
            value.codeUnitAt(i) == 10 ? 32 : value.codeUnitAt(i),
            left + copy * (width + 8) + i * 6 * size,
            (32 - 8 * size) ~/ 2,
          );
        }
      }
      return frame;
    }
    final lines = wrap(size);
    final rows = 32 ~/ (8 * size), pages = (lines.length + rows - 1) ~/ rows;
    final first = (elapsed ~/ (interval * 20) % pages) * rows;
    final visible = (lines.length - first).clamp(0, rows),
        top = (32 - visible * 8 * size) ~/ 2;
    for (var row = 0; row < visible; row++) {
      final line = lines[first + row],
          width = lines[first + row].isEmpty
              ? 0
              : (lines[first + row].length * 6 - 1) * size;
      for (var i = 0; i < line.length; i++) {
        letter(
          line.codeUnitAt(i),
          aligned(width) + i * 6 * size,
          top + row * 8 * size,
        );
      }
    }
    return frame;
  }
}

class MatrixDesign {
  MatrixDesign({List<Uint8List>? frames, this.interval = 150})
    : frames =
          frames?.map(Uint8List.fromList).toList() ?? [Uint8List(frameBytes)];
  List<Uint8List> frames;
  int interval, selected = 0;
  final _undo = <(Uint8List, int)>[], _redo = <(Uint8List, int)>[];
  Uint8List get current => frames[selected];
  bool get canUndo => _undo.isNotEmpty;
  bool get canRedo => _redo.isNotEmpty;
  Uint8List encode() {
    final value = Uint8List.fromList([
      1,
      frames.length,
      interval & 255,
      interval >> 8,
      ...frames.expand((v) => v),
    ]);
    validateValue(BadgeField.artwork, value);
    return value;
  }

  factory MatrixDesign.decode(List<int> bytes) {
    validateValue(BadgeField.artwork, bytes);
    return MatrixDesign(
      interval: bytes[2] + (bytes[3] << 8),
      frames: [
        for (var offset = 4; offset < bytes.length; offset += frameBytes)
          Uint8List.fromList(bytes.sublist(offset, offset + frameBytes)),
      ],
    );
  }
  void checkpoint() {
    _undo.add((encode(), selected));
    if (_undo.length > 40) _undo.removeAt(0);
    _redo.clear();
  }

  void _restore((Uint8List, int) snapshot) {
    final next = MatrixDesign.decode(snapshot.$1);
    frames = next.frames;
    interval = next.interval;
    selected = snapshot.$2.clamp(0, frames.length - 1);
  }

  void undo() {
    if (!canUndo) return;
    _redo.add((encode(), selected));
    _restore(_undo.removeLast());
  }

  void redo() {
    if (!canRedo) return;
    _undo.add((encode(), selected));
    _restore(_redo.removeLast());
  }

  void add({bool duplicate = false}) {
    if (frames.length == maxFrames) {
      throw StateError('This badge stores up to 16 frames.');
    }
    checkpoint();
    frames.insert(
      selected + 1,
      duplicate ? Uint8List.fromList(current) : Uint8List(frameBytes),
    );
    selected++;
  }

  void remove() {
    if (frames.length == 1) throw StateError('Keep at least one frame.');
    checkpoint();
    frames.removeAt(selected);
    selected = selected.clamp(0, frames.length - 1);
  }

  void reorder(int direction) {
    final target = selected + direction;
    if (target < 0 || target >= frames.length) return;
    checkpoint();
    final frame = frames.removeAt(selected);
    frames.insert(target, frame);
    selected = target;
  }

  void clear() {
    checkpoint();
    current.fillRange(0, frameBytes, 0);
  }

  void invert() {
    checkpoint();
    for (var i = 0; i < frameBytes; i++) {
      current[i] ^= 255;
    }
  }

  void flip({required bool horizontal}) {
    checkpoint();
    final original = Uint8List.fromList(current);
    for (var y = 0; y < 32; y++) {
      for (var x = 0; x < 32; x++) {
        setPixel(
          current,
          x,
          y,
          pixel(original, horizontal ? 31 - x : x, horizontal ? y : 31 - y),
        );
      }
    }
  }

  void line(int x0, int y0, int x1, int y1, {bool on = true}) {
    final dx = (x1 - x0).abs(),
        dy = -(y1 - y0).abs(),
        sx = x0 < x1 ? 1 : -1,
        sy = y0 < y1 ? 1 : -1;
    var error = dx + dy;
    while (true) {
      setPixel(current, x0, y0, on);
      if (x0 == x1 && y0 == y1) break;
      final twice = 2 * error;
      if (twice >= dy) {
        error += dy;
        x0 += sx;
      }
      if (twice <= dx) {
        error += dx;
        y0 += sy;
      }
    }
  }

  void rectangle(int x0, int y0, int x1, int y1) {
    line(x0, y0, x1, y0);
    line(x1, y0, x1, y1);
    line(x1, y1, x0, y1);
    line(x0, y1, x0, y0);
  }

  void fill(int x, int y) {
    if (x < 0 || x > 31 || y < 0 || y > 31) return;
    final original = pixel(current, x, y), pending = <int>[y * 32 + x];
    while (pending.isNotEmpty) {
      final point = pending.removeLast(), px = point % 32, py = point ~/ 32;
      if (pixel(current, px, py) != original) continue;
      setPixel(current, px, py, !original);
      if (px > 0) pending.add(point - 1);
      if (px < 31) pending.add(point + 1);
      if (py > 0) pending.add(point - 32);
      if (py < 31) pending.add(point + 32);
    }
  }
}

class SavedLink {
  const SavedLink(this.label, this.url);
  final String label, url;
  Map<String, Object> toJson() => {'label': label, 'url': url};
  factory SavedLink.fromJson(Map<String, dynamic> json) {
    final label = json['label'], url = json['url'];
    if (label is! String ||
        label.trim().isEmpty ||
        label.characters.length > 32 ||
        url is! String) {
      throw const FormatException('Invalid saved link.');
    }
    validateValue(BadgeField.url, ascii.encode(url));
    return SavedLink(label, url);
  }
}

class StudioDraft {
  StudioDraft({List<SavedLink>? links, TextDesign? text, MatrixDesign? matrix})
    : links = links ?? [],
      text = text ?? const TextDesign(),
      matrix = matrix ?? MatrixDesign();
  List<SavedLink> links;
  TextDesign text;
  MatrixDesign matrix;
  String encode() => jsonEncode({
    'version': 1,
    'links': links.map((v) => v.toJson()).toList(),
    'text': base64Encode(text.encode()),
    'artwork': base64Encode(matrix.encode()),
  });
  factory StudioDraft.decode(String raw) {
    final data = jsonDecode(raw);
    if (data is! Map<String, dynamic> ||
        data['version'] != 1 ||
        data['links'] is! List ||
        (data['links'] as List).length > 32 ||
        data['text'] is! String ||
        data['artwork'] is! String) {
      throw const FormatException('Saved Studio draft is incompatible.');
    }
    return StudioDraft(
      links: (data['links'] as List)
          .map((v) => SavedLink.fromJson(v as Map<String, dynamic>))
          .toList(),
      text: TextDesign.decode(base64Decode(data['text'])),
      matrix: MatrixDesign.decode(base64Decode(data['artwork'])),
    );
  }
}
