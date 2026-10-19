// SPDX-License-Identifier: MIT
import 'dart:convert';
import 'dart:typed_data';
import 'dart:math' as math;
import 'package:characters/characters.dart';
import 'font5x8.dart';
import 'protocol.dart';
import 'artwork_codec.dart';

const matrixSize = 32, frameBytes = 128, maxFrames = maxArtworkFrames;

enum PixelTool { pen, erase, line, rectangle, fill, ellipse, picker, select }

int pixelBrightness(List<int> frame, int x, int y) {
  if (x < 0 || x >= 32 || y < 0 || y >= 32) return 0;
  return frame.length == 1024
      ? frame[y * 32 + x]
      : (frame[y * 4 + x ~/ 8] & (128 >> (x % 8)) != 0 ? 255 : 0);
}

bool pixel(List<int> frame, int x, int y) => pixelBrightness(frame, x, y) != 0;
void setPixel(Uint8List frame, int x, int y, bool on) =>
    setBrightness(frame, x, y, on ? 255 : 0);
void setBrightness(Uint8List frame, int x, int y, int level) {
  if (x < 0 || x >= 32 || y < 0 || y >= 32) return;
  if (frame.length == 1024) {
    frame[y * 32 + x] = level.clamp(0, 255);
    return;
  }
  final i = y * 4 + x ~/ 8, mask = 128 >> (x % 8);
  frame[i] = level == 0 ? frame[i] & ~mask : frame[i] | mask;
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
    if (layout == 3) {
      final lines = value.split('\n');
      final rows = 32 ~/ (8 * size), pages = (lines.length + rows - 1) ~/ rows;
      var longest = 0;
      for (final line in lines) {
        longest = math.max(
          longest,
          math.max(0, (line.length * 6 - 1) * size - 32),
        );
      }
      final halfMs = (longest + 6) * interval;
      final cycleMs = longest == 0 ? 40 * interval : 2 * halfMs;
      final phase = elapsed % cycleMs;
      final progress = phase <= halfMs ? phase : cycleMs - phase;
      final first = (elapsed ~/ cycleMs % pages) * rows;
      final visible = math.min(rows, lines.length - first);
      final top = (32 - visible * 8 * size) ~/ 2;
      for (var row = 0; row < visible; row++) {
        final line = lines[first + row],
            width = math.max(0, (lines[first + row].length * 6 - 1) * size);
        final travel = math.max(0, width - 32);
        final shift = ((travel + 6) * progress + halfMs ~/ 2) ~/ halfMs;
        final left = travel == 0 ? aligned(width) : 3 - shift;
        for (var i = 0; i < line.length; i++) {
          letter(line.codeUnitAt(i), left + i * 6 * size, top + row * 8 * size);
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
  MatrixDesign({
    List<Uint8List>? frames,
    int interval = 150,
    List<int>? durations,
    this.playback = 0,
  }) : _interval = interval,
       durations = List<int>.from(
         durations ?? List.filled(frames?.length ?? 1, interval),
       ),
       frames =
           frames?.map(Uint8List.fromList).toList() ?? [Uint8List(frameBytes)];
  List<Uint8List> frames;
  int _interval, selected = 0, playback;
  List<int> durations;
  int get interval => _interval;
  set interval(int value) {
    _interval = value;
    durations = List.filled(frames.length, value, growable: true);
  }

  int frameAt(int elapsed) {
    final order = playback == 2
        ? [for (var i = frames.length - 1; i >= 0; i--) i]
        : [
            for (var i = 0; i < frames.length; i++) i,
            if (playback == 3)
              for (var i = frames.length - 2; i > 0; i--) i,
          ];
    final total = order.fold<int>(0, (sum, i) => sum + durations[i]);
    var at = playback == 1 ? elapsed.clamp(0, total - 1) : elapsed % total;
    for (final i in order) {
      if (at < durations[i]) return i;
      at -= durations[i];
    }
    return order.last;
  }

  final _undo = <(Uint8List, int)>[], _redo = <(Uint8List, int)>[];
  Uint8List get current => frames[selected];
  bool get canUndo => _undo.isNotEmpty;
  bool get canRedo => _redo.isNotEmpty;
  int brush = 255, brushSize = 1;
  Uint8List encode({bool forBadge = false}) => encodeArtwork(
    frames,
    interval,
    forBadge: forBadge,
    durations: durations,
    playback: playback,
  );
  factory MatrixDesign.decode(List<int> bytes) {
    final clip = decodeArtworkClip(bytes);
    return MatrixDesign(
      interval: clip.interval,
      frames: clip.frames,
      durations: clip.durations,
      playback: clip.playback,
    );
  }
  void paint(int x, int y, int value) {
    if (value != 0 && value != 255 && current.length != 1024) {
      frames[selected] = grayPixels(current);
    }
    final half = brushSize ~/ 2;
    for (var yy = y - half; yy < y - half + brushSize; yy++) {
      for (var xx = x - half; xx < x - half + brushSize; xx++) {
        setBrightness(current, xx, yy, value);
      }
    }
  }

  (int, int, int, int)? selection;
  Uint8List? clipboard;
  int clipboardWidth = 0, clipboardHeight = 0;
  void ellipse(int x0, int y0, int x1, int y1) {
    final cx = (x0 + x1) / 2,
        cy = (y0 + y1) / 2,
        rx = (x1 - x0).abs() / 2,
        ry = (y1 - y0).abs() / 2;
    for (var i = 0; i < 128; i++) {
      final angle = i * math.pi / 64;
      paint(
        (cx + rx * math.cos(angle)).round(),
        (cy + ry * math.sin(angle)).round(),
        brush,
      );
    }
  }

  void rotate() {
    checkpoint();
    final before = Uint8List.fromList(current);
    for (var y = 0; y < 32; y++) {
      for (var x = 0; x < 32; x++) {
        setBrightness(current, 31 - y, x, pixelBrightness(before, x, y));
      }
    }
  }

  void select(int x0, int y0, int x1, int y1) => selection = (
    math.min(x0, x1).clamp(0, 31),
    math.min(y0, y1).clamp(0, 31),
    math.max(x0, x1).clamp(0, 31),
    math.max(y0, y1).clamp(0, 31),
  );
  void copySelection({bool cut = false}) {
    final box = selection ?? (0, 0, 31, 31);
    clipboardWidth = box.$3 - box.$1 + 1;
    clipboardHeight = box.$4 - box.$2 + 1;
    clipboard = Uint8List(clipboardWidth * clipboardHeight);
    if (cut) checkpoint();
    for (var y = 0; y < clipboardHeight; y++) {
      for (var x = 0; x < clipboardWidth; x++) {
        clipboard![y * clipboardWidth + x] = pixelBrightness(
          current,
          box.$1 + x,
          box.$2 + y,
        );
        if (cut) setBrightness(current, box.$1 + x, box.$2 + y, 0);
      }
    }
  }

  void pasteSelection({int? atX, int? atY}) {
    if (clipboard == null) return;
    checkpoint();
    final x = atX ?? selection?.$1 ?? 0, y = atY ?? selection?.$2 ?? 0;
    if (current.length != 1024) frames[selected] = grayPixels(current);
    for (var yy = 0; yy < clipboardHeight; yy++) {
      for (var xx = 0; xx < clipboardWidth; xx++) {
        setBrightness(
          current,
          x + xx,
          y + yy,
          clipboard![yy * clipboardWidth + xx],
        );
      }
    }
    select(
      x,
      y,
      (x + clipboardWidth - 1).clamp(0, 31),
      (y + clipboardHeight - 1).clamp(0, 31),
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
    _interval = next.interval;
    durations = next.durations;
    playback = next.playback;
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
      throw StateError('This editor holds up to 128 frames.');
    }
    checkpoint();
    frames.insert(
      selected + 1,
      duplicate ? Uint8List.fromList(current) : Uint8List(frameBytes),
    );
    durations.insert(selected + 1, durations[selected]);
    selected++;
  }

  void remove() {
    if (frames.length == 1) throw StateError('Keep at least one frame.');
    checkpoint();
    frames.removeAt(selected);
    durations.removeAt(selected);
    selected = selected.clamp(0, frames.length - 1);
  }

  void reorder(int direction) {
    final target = selected + direction;
    if (target < 0 || target >= frames.length) return;
    checkpoint();
    final frame = frames.removeAt(selected);
    frames.insert(target, frame);
    final duration = durations.removeAt(selected);
    durations.insert(target, duration);
    selected = target;
  }

  void clear() {
    checkpoint();
    current.fillRange(0, current.length, 0);
  }

  void invert() {
    checkpoint();
    for (var i = 0; i < current.length; i++) {
      current[i] ^= 255;
    }
  }

  void flip({required bool horizontal}) {
    checkpoint();
    final original = Uint8List.fromList(current);
    for (var y = 0; y < 32; y++) {
      for (var x = 0; x < 32; x++) {
        setBrightness(
          current,
          x,
          y,
          pixelBrightness(
            original,
            horizontal ? 31 - x : x,
            horizontal ? y : 31 - y,
          ),
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
      paint(x0, y0, on ? brush : 0);
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
    if (brush != 0 && brush != 255 && current.length != 1024) {
      frames[selected] = grayPixels(current);
    }
    final original = pixelBrightness(current, x, y),
        pending = <int>[y * 32 + x];
    final replacement = original == brush ? 0 : brush;
    if (original == replacement) return;
    while (pending.isNotEmpty) {
      final point = pending.removeLast(), px = point % 32, py = point ~/ 32;
      if (pixelBrightness(current, px, py) != original) continue;
      setBrightness(current, px, py, replacement);
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

class SavedDesign {
  SavedDesign({
    required this.id,
    required this.name,
    required this.isText,
    required List<int> bytes,
  }) : bytes = Uint8List.fromList(bytes) {
    if (id.isEmpty ||
        id.length > 80 ||
        name.trim().isEmpty ||
        name.characters.length > 40) {
      throw const FormatException('Invalid library item.');
    }
    if (isText) {
      validateValue(BadgeField.text, this.bytes);
    } else {
      decodeArtwork(this.bytes);
    }
  }
  final String id, name;
  final bool isText;
  final Uint8List bytes;
  late final textDesign = TextDesign.decode(bytes);
  late final matrixDesign = MatrixDesign.decode(bytes);
  Map<String, Object> toJson() => {
    'id': id,
    'name': name,
    'text': isText,
    'data': base64Encode(bytes),
  };
  factory SavedDesign.fromJson(Map<String, dynamic> value) {
    if (value['id'] is! String ||
        value['name'] is! String ||
        value['text'] is! bool ||
        value['data'] is! String) {
      throw const FormatException('Invalid library item.');
    }
    return SavedDesign(
      id: value['id'],
      name: value['name'],
      isText: value['text'],
      bytes: base64Decode(value['data']),
    );
  }
}

class StudioDraft {
  StudioDraft({
    List<SavedLink>? links,
    TextDesign? text,
    MatrixDesign? matrix,
    List<SavedDesign>? library,
  }) : links = links ?? [],
       text = text ?? const TextDesign(),
       matrix = matrix ?? MatrixDesign(),
       library = library ?? [];
  List<SavedLink> links;
  TextDesign text;
  MatrixDesign matrix;
  List<SavedDesign> library;
  String encode() => jsonEncode({
    'version': 2,
    'links': links.map((v) => v.toJson()).toList(),
    'text': base64Encode(text.encode()),
    'artwork': base64Encode(matrix.encode()),
    'library': library.map((v) => v.toJson()).toList(),
  });
  factory StudioDraft.decode(String raw) {
    if (raw.length > 16777216) {
      throw const FormatException('Library is too large.');
    }
    final data = jsonDecode(raw);
    if (data is! Map<String, dynamic> ||
        (data['version'] != 1 && data['version'] != 2) ||
        data['links'] is! List ||
        (data['links'] as List).length > 32 ||
        data['text'] is! String ||
        data['artwork'] is! String ||
        (data['version'] == 2 &&
            (data['library'] is! List ||
                (data['library'] as List).length > 64))) {
      throw const FormatException('Saved Studio draft is incompatible.');
    }
    final saved = data['version'] == 1
        ? <SavedDesign>[]
        : (data['library'] as List)
              .map((v) => SavedDesign.fromJson(v as Map<String, dynamic>))
              .toList();
    if (saved.map((v) => v.id).toSet().length != saved.length) {
      throw const FormatException('Duplicate library item.');
    }
    return StudioDraft(
      links: (data['links'] as List)
          .map((v) => SavedLink.fromJson(v as Map<String, dynamic>))
          .toList(),
      text: TextDesign.decode(base64Decode(data['text'])),
      matrix: MatrixDesign.decode(base64Decode(data['artwork'])),
      library: saved,
    );
  }
}
