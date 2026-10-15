// SPDX-License-Identifier: MIT
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter/gestures.dart';
import 'studio_model.dart';

class MatrixPreview extends StatelessWidget {
  const MatrixPreview({
    super.key,
    required this.frame,
    this.label = '32 by 32 LED matrix preview',
  });
  final Uint8List frame;
  final String label;
  @override
  Widget build(BuildContext context) => Semantics(
    label: label,
    image: true,
    child: AspectRatio(
      aspectRatio: 1,
      child: CustomPaint(
        painter: MatrixPainter(frame),
        child: const SizedBox.expand(),
      ),
    ),
  );
}

class MatrixPainter extends CustomPainter {
  MatrixPainter(List<int> frame) : frame = Uint8List.fromList(frame);
  final Uint8List frame;
  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRRect(
      RRect.fromRectAndRadius(Offset.zero & size, const Radius.circular(20)),
      Paint()..color = const Color(0xff101a20),
    );
    final cell = size.width / 34, inset = cell;
    final off = Paint()..color = const Color(0xff24313b),
        on = Paint()..color = const Color(0xff80f4cf);
    for (var y = 0; y < 32; y++) {
      for (var x = 0; x < 32; x++) {
        canvas.drawRRect(
          RRect.fromRectAndRadius(
            Rect.fromLTWH(
              inset + x * cell + cell * .12,
              inset + y * cell + cell * .12,
              cell * .76,
              cell * .76,
            ),
            Radius.circular(cell * .18),
          ),
          pixel(frame, x, y) ? on : off,
        );
      }
    }
  }

  @override
  bool shouldRepaint(MatrixPainter old) => !listEqualsLocal(frame, old.frame);
}

bool listEqualsLocal(List<int> a, List<int> b) =>
    a.length == b.length &&
    List.generate(a.length, (i) => a[i] == b[i]).every((v) => v);

class MatrixEditor extends StatefulWidget {
  const MatrixEditor({
    super.key,
    required this.design,
    required this.tool,
    required this.onChanged,
    this.enabled = true,
  });
  final MatrixDesign design;
  final PixelTool tool;
  final VoidCallback onChanged;
  final bool enabled;
  @override
  State<MatrixEditor> createState() => _MatrixEditorState();
}

class _MatrixEditorState extends State<MatrixEditor> {
  int? activePointer;
  (int, int)? start, previous;
  Uint8List? original;
  bool filled = false;
  (int, int) position(Offset point, double width) {
    final cell = width / 34;
    return (
      ((point.dx - cell) / cell).floor().clamp(0, 31),
      ((point.dy - cell) / cell).floor().clamp(0, 31),
    );
  }

  void begin((int, int) point) {
    if (!widget.enabled) return;
    widget.design.checkpoint();
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
        design.current.setAll(0, original!);
        if (widget.tool == PixelTool.line) {
          design.line(start!.$1, start!.$2, x, y);
        } else {
          design.rectangle(start!.$1, start!.$2, x, y);
        }
    }
    widget.onChanged();
  }

  void finish() {
    start = previous = null;
    original = null;
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
          Row(
            children: [
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(title, style: Theme.of(context).textTheme.titleLarge),
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
          const SizedBox(height: 20),
          child,
        ],
      ),
    ),
  );
}
