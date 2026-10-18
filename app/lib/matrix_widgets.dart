// SPDX-License-Identifier: MIT
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter/gestures.dart';
import 'studio_model.dart';

class MatrixPreview extends StatelessWidget {
  const MatrixPreview({
    super.key,
    required this.frame,
    this.onion,
    this.onionOpacity = .25,
    this.selection,
    this.label = '32 by 32 LED matrix preview',
    this.showGrid = true,
    this.solidPixels = false,
  });
  final Uint8List frame;
  final Uint8List? onion;
  final double onionOpacity;
  final (int, int, int, int)? selection;
  final String label;
  final bool showGrid, solidPixels;
  @override
  Widget build(BuildContext context) => Semantics(
    label: label,
    image: true,
    child: AspectRatio(
      aspectRatio: 1,
      child: CustomPaint(
        painter: MatrixPainter(
          frame,
          onion: onion,
          onionOpacity: onionOpacity,
          selection: selection,
          showGrid: showGrid,
          solidPixels: solidPixels,
        ),
        child: const SizedBox.expand(),
      ),
    ),
  );
}

class MatrixPainter extends CustomPainter {
  MatrixPainter(
    List<int> frame, {
    List<int>? onion,
    this.onionOpacity = .25,
    this.selection,
    this.showGrid = true,
    this.solidPixels = false,
  }) : frame = Uint8List.fromList(frame),
       onion = onion == null ? null : Uint8List.fromList(onion);
  final Uint8List? onion;
  final double onionOpacity;
  final (int, int, int, int)? selection;
  final Uint8List frame;
  final bool showGrid, solidPixels;
  static final palette = List.generate(
    256,
    (i) => Paint()..color = Color.lerp(Colors.black, Colors.white, i / 255)!,
  );
  static final solidPalette = List.generate(
    256,
    (i) => Paint()
      ..color = Color.fromARGB(255, i, i, i)
      ..isAntiAlias = false,
  );
  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRRect(
      RRect.fromRectAndRadius(Offset.zero & size, const Radius.circular(20)),
      Paint()..color = Colors.black,
    );
    final cell = size.width / 34, inset = cell;
    final guide = Paint()..color = const Color(0xff202020);
    final gap = solidPixels ? 0.0 : .12;
    for (var y = 0; y < 32; y++) {
      for (var x = 0; x < 32; x++) {
        final value = pixelBrightness(frame, x, y);
        if (value == 0 && !showGrid) continue;
        canvas.drawRRect(
          RRect.fromRectAndRadius(
            Rect.fromLTWH(
              inset + x * cell + cell * gap,
              inset + y * cell + cell * gap,
              cell * (1 - 2 * gap),
              cell * (1 - 2 * gap),
            ),
            Radius.circular(solidPixels ? 0 : cell * .18),
          ),
          value == 0
              ? guide
              : (solidPixels ? solidPalette[value] : palette[value]),
        );
      }
    }
    if (onion != null) {
      final ghost = Paint()
        ..color = const Color(0xffb2a6ff).withValues(alpha: onionOpacity);
      for (var y = 0; y < 32; y++) {
        for (var x = 0; x < 32; x++) {
          if (pixelBrightness(frame, x, y) == 0 &&
              pixelBrightness(onion!, x, y) > 0) {
            canvas.drawCircle(
              Offset(inset + (x + .5) * cell, inset + (y + .5) * cell),
              cell * .25,
              ghost,
            );
          }
        }
      }
    }
    if (selection != null) {
      final b = selection!;
      canvas.drawRect(
        Rect.fromLTWH(
          inset + b.$1 * cell,
          inset + b.$2 * cell,
          (b.$3 - b.$1 + 1) * cell,
          (b.$4 - b.$2 + 1) * cell,
        ),
        Paint()
          ..color = const Color(0xffffd275)
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2,
      );
    }
  }

  @override
  bool shouldRepaint(MatrixPainter old) =>
      !listEqualsLocal(frame, old.frame) ||
      showGrid != old.showGrid ||
      solidPixels != old.solidPixels ||
      onionOpacity != old.onionOpacity ||
      selection != old.selection ||
      (onion == null) != (old.onion == null) ||
      (onion != null &&
          old.onion != null &&
          !listEqualsLocal(onion!, old.onion!));
}

bool listEqualsLocal(List<int> a, List<int> b) {
  if (a.length != b.length) return false;
  for (var i = 0; i < a.length; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

class MatrixEditor extends StatefulWidget {
  const MatrixEditor({
    super.key,
    required this.design,
    required this.tool,
    required this.onChanged,
    this.onStrokeStart,
    this.onStrokeEnd,
    this.enabled = true,
    this.onion,
    this.onionOpacity = .25,
  });
  final MatrixDesign design;
  final PixelTool tool;
  final VoidCallback onChanged;
  final VoidCallback? onStrokeStart, onStrokeEnd;
  final bool enabled;
  final Uint8List? onion;
  final double onionOpacity;
  @override
  State<MatrixEditor> createState() => _MatrixEditorState();
}

class _MatrixEditorState extends State<MatrixEditor> {
  int? activePointer;
  (int, int)? start, previous;
  Uint8List? original;
  bool filled = false;
  (int, int, int, int)? movingBox;
  (int, int) position(Offset point, double width) {
    final cell = width / 34;
    return (
      ((point.dx - cell) / cell).floor().clamp(0, 31),
      ((point.dy - cell) / cell).floor().clamp(0, 31),
    );
  }

  void begin((int, int) point) {
    if (!widget.enabled) return;
    widget.onStrokeStart?.call();
    final box = widget.design.selection;
    movingBox =
        widget.tool == PixelTool.select &&
            box != null &&
            point.$1 >= box.$1 &&
            point.$1 <= box.$3 &&
            point.$2 >= box.$2 &&
            point.$2 <= box.$4
        ? box
        : null;
    if (widget.tool != PixelTool.picker &&
        (widget.tool != PixelTool.select || movingBox != null)) {
      widget.design.checkpoint();
    }
    start = previous = point;
    filled = false;
    original = Uint8List.fromList(widget.design.current);
    update(point);
  }

  void update((int, int) point) {
    if (!widget.enabled || start == null) return;
    final design = widget.design, x = point.$1, y = point.$2;
    switch (widget.tool) {
      case PixelTool.fill:
        if (!filled) {
          design.fill(start!.$1, start!.$2);
          filled = true;
        }
      case PixelTool.pen:
      case PixelTool.erase:
        final from = previous ?? point;
        design.line(from.$1, from.$2, x, y, on: widget.tool == PixelTool.pen);
        previous = point;
      case PixelTool.line:
      case PixelTool.rectangle:
      case PixelTool.ellipse:
        design.frames[design.selected] = Uint8List.fromList(original!);
        if (widget.tool == PixelTool.line) {
          design.line(start!.$1, start!.$2, x, y);
        } else if (widget.tool == PixelTool.rectangle) {
          design.rectangle(start!.$1, start!.$2, x, y);
        } else {
          design.ellipse(start!.$1, start!.$2, x, y);
        }
      case PixelTool.picker:
        design.brush = pixelBrightness(design.current, x, y);
      case PixelTool.select:
        final box = movingBox;
        if (box == null) {
          design.select(start!.$1, start!.$2, x, y);
        } else {
          final dx = (x - start!.$1).clamp(-box.$1, 31 - box.$3),
              dy = (y - start!.$2).clamp(-box.$2, 31 - box.$4);
          design.frames[design.selected] = Uint8List.fromList(original!);
          final captured = <int>[];
          for (var yy = box.$2; yy <= box.$4; yy++) {
            for (var xx = box.$1; xx <= box.$3; xx++) {
              captured.add(pixelBrightness(original!, xx, yy));
              setBrightness(design.current, xx, yy, 0);
            }
          }
          var at = 0;
          for (var yy = box.$2; yy <= box.$4; yy++) {
            for (var xx = box.$1; xx <= box.$3; xx++) {
              setBrightness(design.current, xx + dx, yy + dy, captured[at++]);
            }
          }
          design.select(box.$1 + dx, box.$2 + dy, box.$3 + dx, box.$4 + dy);
        }
    }
    widget.onChanged();
  }

  void finish() {
    widget.onStrokeEnd?.call();
    start = previous = null;
    original = null;
  }

  @override
  void dispose() {
    // A tab switch or preview toggle can remove an editor mid-stroke.
    if (activePointer != null) widget.onStrokeEnd?.call();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      final width = constraints.maxWidth;
      return Semantics(
        label:
            'Pixel canvas. Choose a drawing tool, then touch or drag to edit.',
        child: RawGestureDetector(
          // Drawing owns the gesture even for vertical strokes. Move mode
          // releases it to the surrounding zoom/scroll surface.
          gestures: widget.enabled
              ? {
                  EagerGestureRecognizer:
                      GestureRecognizerFactoryWithHandlers<
                        EagerGestureRecognizer
                      >(EagerGestureRecognizer.new, (_) {}),
                }
              : {},
          child: Listener(
            behavior: HitTestBehavior.opaque,
            onPointerDown: widget.enabled
                ? (event) {
                    if (activePointer != null) return;
                    activePointer = event.pointer;
                    begin(position(event.localPosition, width));
                  }
                : null,
            onPointerMove: widget.enabled
                ? (event) {
                    if (event.pointer == activePointer) {
                      update(position(event.localPosition, width));
                    }
                  }
                : null,
            onPointerUp: (event) {
              if (event.pointer == activePointer) {
                activePointer = null;
                finish();
              }
            },
            onPointerCancel: (event) {
              if (event.pointer == activePointer) {
                activePointer = null;
                finish();
              }
            },
            child: MatrixPreview(
              frame: widget.design.current,
              label: 'Editable frame ${widget.design.selected + 1}',
              onion: widget.onion,
              onionOpacity: widget.onionOpacity,
              selection: widget.design.selection,
            ),
          ),
        ),
      );
    },
  );
}

class StudioPanel extends StatelessWidget {
  const StudioPanel({
    super.key,
    required this.title,
    required this.child,
    this.subtitle,
    this.help,
    this.trailing,
  });
  final String title;
  final String? subtitle, help;
  final Widget child;
  final Widget? trailing;
  @override
  Widget build(BuildContext context) => Card(
    margin: const EdgeInsets.only(bottom: 16),
    child: Padding(
      padding: const EdgeInsets.all(20),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          if (title.isNotEmpty || help != null || trailing != null)
            Row(
              children: [
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        title,
                        style: Theme.of(context).textTheme.titleLarge,
                      ),
                      if (subtitle != null) ...[
                        const SizedBox(height: 4),
                        Text(
                          subtitle!,
                          style: Theme.of(context).textTheme.bodyMedium,
                        ),
                      ],
                    ],
                  ),
                ),
                if (help != null)
                  IconButton(
                    tooltip: '$title help',
                    icon: const Icon(Icons.info_outline, size: 20),
                    onPressed: () => showDialog<void>(
                      context: context,
                      builder: (context) => AlertDialog(
                        title: Text(title),
                        content: Text(help!),
                        actions: [
                          TextButton(
                            onPressed: () => Navigator.pop(context),
                            child: const Text('Done'),
                          ),
                        ],
                      ),
                    ),
                  ),
                ?trailing,
              ],
            ),
          if (title.isNotEmpty || help != null || trailing != null)
            const SizedBox(height: 20),
          child,
        ],
      ),
    ),
  );
}
