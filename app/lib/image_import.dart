// SPDX-License-Identifier: MIT
import 'dart:async';
import 'dart:math' as math;
import 'dart:ui' as ui;
import 'package:file_selector/file_selector.dart';
import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import 'matrix_widgets.dart';
import 'studio_model.dart';
import 'artwork_codec.dart';
import 'selection_control.dart';
import 'bounded_gif.dart';
import 'gif_input.dart';
import 'gif_file_stub.dart' if (dart.library.io) 'gif_file_io.dart';

class RasterFrame {
  const RasterFrame(this.width, this.height, this.rgba, this.duration);
  final int width, height, duration;
  final Uint8List rgba;
}

class ImportedRaster {
  const ImportedRaster(this.frames, this.sourceCount);
  final List<RasterFrame> frames;
  final int sourceCount;
  int get interval =>
      (frames.fold<int>(0, (n, f) => n + f.duration) ~/ frames.length).clamp(
        50,
        2000,
      );
  MatrixDesign convert({
    int threshold = 128,
    bool invert = false,
    bool dither = false,
    bool crop = false,
    bool grayscale = false,
    int brightness = 0,
    double contrast = 1,
    double zoom = 1,
    double offsetX = 0,
    double offsetY = 0,
    double? glyphPadding,
  }) => MatrixDesign(
    frames: [
      for (final frame in frames)
        rasterize(
          frame,
          threshold: threshold,
          invert: invert,
          dither: dither,
          crop: crop,
          grayscale: grayscale,
          brightness: brightness,
          contrast: contrast,
          zoom: zoom,
          offsetX: offsetX,
          offsetY: offsetY,
          glyphPadding: glyphPadding,
        ),
    ],
    interval: interval,
    durations: [for (final frame in frames) frame.duration.clamp(50, 2000)],
  );
}

Uint8List rasterize(
  RasterFrame image, {
  int threshold = 128,
  bool invert = false,
  bool dither = false,
  bool crop = false,
  bool grayscale = false,
  int brightness = 0,
  double contrast = 1,
  double zoom = 1,
  double offsetX = 0,
  double offsetY = 0,
  double? glyphPadding,
}) {
  if (image.width < 1 ||
      image.height < 1 ||
      image.rgba.length != image.width * image.height * 4 ||
      threshold < 1 ||
      threshold > 254) {
    throw const FormatException('Invalid image pixels.');
  }
  final scale =
      (crop
          ? math.max(32 / image.width, 32 / image.height)
          : math.min(32 / image.width, 32 / image.height)) *
      zoom;
  final left = (32 - image.width * scale) / 2 + offsetX * 16,
      top = (32 - image.height * scale) / 2 + offsetY * 16;
  final light = List<double>.filled(1024, 0);
  if (glyphPadding != null) {
    if (glyphPadding < 0 || glyphPadding > .15) {
      throw const FormatException('Invalid glyph padding.');
    }
    // Recover the logical 25 x 25 grid, rather than resizing its rendered gaps.
    // Center at native resolution: one source LED becomes one badge LED.
    final pitchX = image.width * (1 - 2 * glyphPadding) / 25;
    final pitchY = image.height * (1 - 2 * glyphPadding) / 25;
    for (var y = 0; y < 25; y++) {
      for (var x = 0; x < 25; x++) {
        final cx = image.width * glyphPadding + (x + .5) * pitchX;
        final cy = image.height * glyphPadding + (y + .5) * pitchY;
        light[(y + 3) * 32 + x + 3] = _average(
          image,
          cx - pitchX * .15,
          cy - pitchY * .15,
          cx + pitchX * .15,
          cy + pitchY * .15,
        );
      }
    }
  } else {
    for (var y = 0; y < 32; y++) {
      for (var x = 0; x < 32; x++) {
        light[y * 32 + x] = _average(
          image,
          (x - left) / scale,
          (y - top) / scale,
          (x + 1 - left) / scale,
          (y + 1 - top) / scale,
        );
      }
    }
  }
  for (var i = 0; i < 1024; i++) {
    light[i] = ((light[i] - 127.5) * contrast + 127.5 + brightness).clamp(
      0,
      255,
    );
  }
  if (grayscale) {
    return Uint8List.fromList([
      for (final value in light) invert ? 255 - value.round() : value.round(),
    ]);
  }
  final output = Uint8List(128);
  for (var y = 0; y < 32; y++) {
    for (var x = 0; x < 32; x++) {
      final i = y * 32 + x, lit = light[i] >= threshold;
      setPixel(output, x, y, invert ? !lit : lit);
      if (dither) {
        final error = light[i] - (lit ? 255 : 0);
        if (x < 31) light[i + 1] += error * 7 / 16;
        if (y < 31) {
          if (x > 0) light[i + 31] += error * 3 / 16;
          light[i + 32] += error * 5 / 16;
          if (x < 31) light[i + 33] += error / 16;
        }
      }
    }
  }
  return output;
}

double _luma(RasterFrame image, int x, int y) {
  final i = (y * image.width + x) * 4;
  return (2126 * image.rgba[i] +
          7152 * image.rgba[i + 1] +
          722 * image.rgba[i + 2]) *
      image.rgba[i + 3] /
      2550000;
}

// Area filtering avoids point-sampling aliases when reducing photos and GIFs.
double _average(
  RasterFrame image,
  double left,
  double top,
  double right,
  double bottom,
) {
  final area = (right - left) * (bottom - top);
  var sum = 0.0;
  for (
    var y = top.floor().clamp(0, image.height);
    y < bottom.ceil().clamp(0, image.height);
    y++
  ) {
    final wy = math.min(y + 1.0, bottom) - math.max(y.toDouble(), top);
    for (
      var x = left.floor().clamp(0, image.width);
      x < right.ceil().clamp(0, image.width);
      x++
    ) {
      final wx = math.min(x + 1.0, right) - math.max(x.toDouble(), left);
      sum += _luma(image, x, y) * wx * wy;
    }
  }
  return area > 0 ? sum / area : 0;
}

/// Conservative recognition of a centered, rendered 25 x 25 LED grid.
/// Ordinary images stay in Image mode; the user can override either choice.
double? detectGlyphPadding(ImportedRaster image) {
  final first = image.frames.first;
  if (first.width < 100 ||
      first.height < 100 ||
      (first.width / first.height - 1).abs() > .01) {
    return null;
  }
  final horizontal = List<double>.filled(first.width, 0);
  final vertical = List<double>.filled(first.height, 0);
  final step = math.max(1, image.frames.length ~/ 8);
  for (var f = 0; f < image.frames.length; f += step) {
    final frame = image.frames[f];
    if (frame.width != first.width || frame.height != first.height) return null;
    for (var y = 0; y < frame.height; y++) {
      for (var x = 0; x < frame.width; x++) {
        final value = _luma(frame, x, y);
        horizontal[x] = math.max(horizontal[x], value);
        vertical[y] = math.max(vertical[y], value);
      }
    }
  }
  double score(List<double> profile, double padding) {
    final pitch = profile.length * (1 - 2 * padding) / 25;
    final start = profile.length * padding;
    var centers = 0.0, gaps = 0.0;
    for (var i = 0; i < 25; i++) {
      centers += profile[(start + (i + .5) * pitch).floor()];
      if (i > 0) gaps += profile[(start + i * pitch).floor()];
    }
    return (centers / 25 - gaps / 24) / 255;
  }

  final candidates = <(double, double)>[];
  var bestScore = .30;
  for (var i = 0; i <= 150; i++) {
    final padding = i / 1000;
    final x = score(horizontal, padding), y = score(vertical, padding);
    if (x > .12 && y > .12 && x + y > bestScore) {
      bestScore = x + y;
    }
    if (x > .12 && y > .12) candidates.add((padding, x + y));
  }
  final best = candidates.where((v) => v.$2 >= bestScore - .00001).toList();
  return best.isEmpty
      ? null
      : best.fold<double>(0, (n, v) => n + v.$1) / best.length;
}

Future<ImportedRaster> decodeRaster(Uint8List bytes) async {
  if (bytes.isEmpty) {
    throw const FormatException('Choose a non-empty image or GIF.');
  }
  if (bytes.length >= 6 &&
      String.fromCharCodes(bytes.take(6)).startsWith('GIF8')) {
    return compute(_decodeGifBytes, bytes);
  }
  return _decodeRasterBuffer(await ui.ImmutableBuffer.fromUint8List(bytes));
}

/// Use the file-backed engine buffer instead of copying a large source into Dart.
Future<ImportedRaster> decodeRasterFile(String path) async {
  try {
    if (isGifFile(path)) return await compute(_decodeGifFile, path);
  } on FormatException {
    rethrow;
  } catch (_) {
    throw const FormatException('This image file could not be opened.');
  }
  final ui.ImmutableBuffer buffer;
  try {
    buffer = await ui.ImmutableBuffer.fromFilePath(path);
  } catch (_) {
    throw const FormatException('This image file could not be opened.');
  }
  return _decodeRasterBuffer(buffer);
}

ImportedRaster _fromGif(SampledGif gif) => ImportedRaster([
  for (var i = 0; i < gif.frames.length; i++)
    RasterFrame(gif.width, gif.height, gif.frames[i], gif.durations[i]),
], gif.count);
ImportedRaster _decodeGifBytes(Uint8List bytes) =>
    _fromGif(decodeSampledGif(MemoryGifInput(bytes)));
ImportedRaster _decodeGifFile(String path) =>
    _fromGif(decodeSampledGif(openGifFile(path)));

Future<ImportedRaster> _decodeRasterBuffer(ui.ImmutableBuffer buffer) async {
  ui.ImageDescriptor? descriptor;
  ui.Codec? codec;
  try {
    descriptor = await ui.ImageDescriptor.encoded(buffer);
    // Keep enough detail to distinguish existing LED dots from their gaps.
    // Do not upscale native pixel art before extracting its pixels.
    final longest = math.max(descriptor.width, descriptor.height);
    final ratio = math.min(1.0, 256 / longest);
    final width = (descriptor.width * ratio).round().clamp(1, 256);
    final height = (descriptor.height * ratio).round().clamp(1, 256);
    codec = await descriptor.instantiateCodec(
      targetWidth: width,
      targetHeight: height,
    );
    // Other animated codecs can ignore requested scaling too. Refuse a
    // source-sized native surface above 16 MiB before requesting any frame.
    // GIFs use the sampled streaming path instead, regardless of dimensions.
    if (codec.frameCount > 1 &&
        descriptor.width * descriptor.height > 4 * 1024 * 1024) {
      throw const FormatException(
        'This animation needs conversion to GIF first.',
      );
    }
    final frames = <RasterFrame>[];
    for (var i = 0; i < math.min(codec.frameCount, maxFrames); i++) {
      final info = await codec.getNextFrame();
      ui.Image? resized;
      ui.Picture? picture;
      try {
        // Some animated codecs return native-sized frames despite targetWidth.
        // Enforce the retained pixel budget explicitly in that case.
        if (info.image.width != width || info.image.height != height) {
          final recorder = ui.PictureRecorder();
          final canvas = Canvas(recorder);
          canvas.drawImageRect(
            info.image,
            Rect.fromLTWH(
              0,
              0,
              info.image.width.toDouble(),
              info.image.height.toDouble(),
            ),
            Rect.fromLTWH(0, 0, width.toDouble(), height.toDouble()),
            Paint()..filterQuality = FilterQuality.low,
          );
          picture = recorder.endRecording();
          resized = await picture.toImage(width, height);
        }
        final decoded = resized ?? info.image;
        final raw = await decoded.toByteData(
          format: ui.ImageByteFormat.rawStraightRgba,
        );
        if (raw == null) {
          throw const FormatException('Could not decode the image.');
        }
        frames.add(
          RasterFrame(
            decoded.width,
            decoded.height,
            Uint8List.fromList(
              raw.buffer.asUint8List(raw.offsetInBytes, raw.lengthInBytes),
            ),
            info.duration.inMilliseconds == 0
                ? 150
                : info.duration.inMilliseconds,
          ),
        );
      } finally {
        resized?.dispose();
        picture?.dispose();
        info.image.dispose();
      }
    }
    return ImportedRaster(frames, codec.frameCount);
  } on FormatException {
    rethrow;
  } catch (_) {
    throw const FormatException(
      'This image could not be opened. Try PNG, JPEG or GIF.',
    );
  } finally {
    codec?.dispose();
    descriptor?.dispose();
    buffer.dispose();
  }
}

Future<ImportedRaster?> pickRaster() async {
  const types = XTypeGroup(
    label: 'Images and GIFs',
    extensions: ['png', 'jpg', 'jpeg', 'gif', 'webp', 'bmp'],
    mimeTypes: ['image/*'],
    uniformTypeIdentifiers: ['public.image'],
  );
  final file = await openFile(acceptedTypeGroups: [types]);
  if (file == null) return null;
  return kIsWeb
      ? decodeRaster(await file.readAsBytes())
      : decodeRasterFile(file.path);
}

class ImportDesignDialog extends StatefulWidget {
  const ImportDesignDialog({super.key, required this.image});
  final ImportedRaster image;
  @override
  State<ImportDesignDialog> createState() => _ImportDesignDialogState();
}

class _ImportSettings {
  const _ImportSettings({
    required this.threshold,
    required this.invert,
    required this.dither,
    required this.crop,
    required this.grayscale,
    required this.brightness,
    required this.contrast,
    required this.zoom,
    required this.offsetX,
    required this.offsetY,
    required this.glyphPadding,
  });
  final int threshold, brightness;
  final bool invert, dither, crop, grayscale;
  final double contrast, zoom, offsetX, offsetY;
  final double? glyphPadding;
}

(MatrixDesign, int) _convertImport((ImportedRaster, _ImportSettings) input) {
  final (image, settings) = input;
  final design = image.convert(
    threshold: settings.threshold,
    invert: settings.invert,
    dither: settings.dither,
    crop: settings.crop,
    grayscale: settings.grayscale,
    brightness: settings.brightness,
    contrast: settings.contrast,
    zoom: settings.zoom,
    offsetX: settings.offsetX,
    offsetY: settings.offsetY,
    glyphPadding: settings.glyphPadding,
  );
  return (design, design.encode().length);
}

class _ImportDesignDialogState extends State<ImportDesignDialog> {
  int threshold = 128, brightness = 0, section = 0;
  int revision = 0, readyRevision = -1, encodedSize = 0;
  bool invert = false, dither = false, crop = false, grayscale = true;
  bool glyph = false, playing = true, converting = false;
  double contrast = 1, zoom = 1, offsetX = 0, offsetY = 0, padding = .05;
  final watch = Stopwatch()..start();
  Timer? timer, debounce;
  final scroll = ScrollController();
  MatrixDesign? design;
  String? error;

  void adjust(VoidCallback change) {
    setState(() {
      change();
      revision++;
      error = null;
    });
    debounce?.cancel();
    debounce = Timer(const Duration(milliseconds: 120), rebuildDesign);
  }

  Future<void> rebuildDesign() async {
    if (!mounted || converting) return;
    converting = true;
    final requested = revision;
    try {
      final result = await compute(_convertImport, (
        widget.image,
        _ImportSettings(
          threshold: threshold,
          invert: invert,
          dither: dither,
          crop: crop,
          grayscale: grayscale,
          brightness: brightness,
          contrast: contrast,
          zoom: zoom,
          offsetX: offsetX,
          offsetY: offsetY,
          glyphPadding: glyph ? padding : null,
        ),
      ));
      if (mounted && requested == revision) {
        setState(() {
          design = result.$1;
          encodedSize = result.$2;
          readyRevision = requested;
        });
      }
    } catch (_) {
      if (mounted && requested == revision) {
        setState(
          () => error =
              'Could not convert this image. Try resetting adjustments.',
        );
      }
    } finally {
      converting = false;
      if (mounted && requested != revision) {
        unawaited(rebuildDesign());
      }
    }
  }

  @override
  void initState() {
    super.initState();
    final detected = detectGlyphPadding(widget.image);
    glyph = detected != null;
    padding = detected ?? .05;
    unawaited(rebuildDesign());
    if (widget.image.frames.length > 1) {
      timer = Timer.periodic(const Duration(milliseconds: 33), (_) {
        if (mounted && playing && design != null) {
          setState(() {});
        }
      });
    }
  }

  @override
  void dispose() {
    timer?.cancel();
    debounce?.cancel();
    scroll.dispose();
    watch.stop();
    super.dispose();
  }

  Widget slider(
    String label,
    double value,
    double min,
    double max,
    ValueChanged<double> change,
    String display,
  ) {
    final control = Slider(
      value: value,
      min: min,
      max: max,
      label: display,
      semanticFormatterCallback: (_) => display,
      onChanged: (v) => adjust(() => change(v)),
    );
    if (MediaQuery.textScalerOf(context).scale(14) > 20) {
      return Column(
        children: [
          Row(
            children: [
              Expanded(child: Text(label)),
              Text(display),
            ],
          ),
          control,
        ],
      );
    }
    return Row(
      children: [
        SizedBox(
          width: 84,
          child: Text(label, style: Theme.of(context).textTheme.labelMedium),
        ),
        Expanded(child: control),
        SizedBox(
          width: 42,
          child: Text(
            display,
            textAlign: TextAlign.end,
            style: Theme.of(context).textTheme.labelSmall,
          ),
        ),
      ],
    );
  }

  Widget controls() {
    if (section == 0) {
      return Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          SelectionControl<bool>(
            title: 'Source type',
            value: glyph,
            expand: true,
            choices: const [
              BadgeChoice(false, 'Image / photo', icon: Icons.image_outlined),
              BadgeChoice(true, 'Glyph - 25 x 25', icon: Icons.grid_on),
            ],
            onChanged: (v) => adjust(() => glyph = v),
          ),
          const SizedBox(height: 12),
          if (glyph)
            Text(
              '25 x 25 pixels, centered on the badge.',
              style: Theme.of(context).textTheme.bodySmall,
            )
          else
            SegmentedButton<bool>(
              showSelectedIcon: false,
              segments: const [
                ButtonSegment(value: false, label: Text('Fit')),
                ButtonSegment(value: true, label: Text('Crop')),
              ],
              selected: {crop},
              onSelectionChanged: (v) => adjust(() => crop = v.first),
            ),
        ],
      );
    }
    if (section == 1) {
      return Column(
        children: [
          SegmentedButton<bool>(
            showSelectedIcon: false,
            segments: const [
              ButtonSegment(value: true, label: Text('Grayscale')),
              ButtonSegment(value: false, label: Text('Threshold')),
            ],
            selected: {grayscale},
            onSelectionChanged: (v) => adjust(() => grayscale = v.first),
          ),
          slider(
            'Brightness',
            brightness.toDouble(),
            -255,
            255,
            (v) => brightness = v.round(),
            '$brightness',
          ),
          slider(
            'Contrast',
            contrast,
            0,
            3,
            (v) => contrast = v,
            contrast.toStringAsFixed(1),
          ),
          if (!grayscale)
            slider(
              'Threshold',
              threshold.toDouble(),
              1,
              254,
              (v) => threshold = v.round(),
              '$threshold',
            ),
          Wrap(
            spacing: 8,
            children: [
              FilterChip(
                label: const Text('Invert'),
                selected: invert,
                showCheckmark: false,
                onSelected: (v) => adjust(() => invert = v),
              ),
              if (!grayscale)
                FilterChip(
                  label: const Text('Dither'),
                  selected: dither,
                  showCheckmark: false,
                  onSelected: (v) => adjust(() => dither = v),
                ),
            ],
          ),
        ],
      );
    }
    return Column(
      children: [
        if (glyph) ...[
          slider(
            'Grid inset',
            padding * 100,
            0,
            15,
            (v) => padding = v / 100,
            '${(padding * 100).toStringAsFixed(1)}%',
          ),
          Text(
            'Adjust if the source has a border around its LED grid.',
            style: Theme.of(context).textTheme.bodySmall,
          ),
        ] else ...[
          slider(
            'Zoom',
            zoom,
            1,
            4,
            (v) => zoom = v,
            '${zoom.toStringAsFixed(1)}x',
          ),
          slider(
            'Horizontal',
            offsetX,
            -1,
            1,
            (v) => offsetX = v,
            '${(offsetX * 100).round()}',
          ),
          slider(
            'Vertical',
            offsetY,
            -1,
            1,
            (v) => offsetY = v,
            '${(offsetY * 100).round()}',
          ),
        ],
        TextButton.icon(
          onPressed: () => adjust(() {
            brightness = 0;
            contrast = zoom = 1;
            offsetX = offsetY = 0;
            crop = invert = dither = false;
            threshold = 128;
            grayscale = true;
            padding = detectGlyphPadding(widget.image) ?? .05;
          }),
          icon: const Icon(Icons.restart_alt),
          label: const Text('Reset adjustments'),
        ),
      ],
    );
  }

  @override
  Widget build(BuildContext context) {
    final current = design;
    final index = current?.frameAt(watch.elapsedMilliseconds) ?? 0;
    final ready = current != null && readyRevision == revision && error == null;
    return Dialog.fullscreen(
      child: Scaffold(
        appBar: AppBar(
          title: const Text('Import media'),
          leading: IconButton(
            tooltip: 'Cancel import',
            icon: const Icon(Icons.close),
            onPressed: () => Navigator.pop(context),
          ),
        ),
        body: SafeArea(
          child: Center(
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 600),
              child: Padding(
                padding: const EdgeInsets.fromLTRB(16, 0, 16, 12),
                child: LayoutBuilder(
                  builder: (context, box) => Column(
                    children: [
                      Expanded(
                        child: Center(
                          child: AspectRatio(
                            aspectRatio: 1,
                            child: current == null
                                ? const Center(
                                    child: CircularProgressIndicator(),
                                  )
                                : MatrixPreview(
                                    frame: current.frames[index],
                                    label: 'Imported media preview',
                                  ),
                          ),
                        ),
                      ),
                      Row(
                        children: [
                          if (widget.image.frames.length > 1)
                            IconButton(
                              tooltip: playing
                                  ? 'Pause import preview'
                                  : 'Play import preview',
                              icon: Icon(
                                playing ? Icons.pause : Icons.play_arrow,
                              ),
                              onPressed: () => setState(() {
                                playing = !playing;
                                if (playing) {
                                  watch.start();
                                } else {
                                  watch.stop();
                                }
                              }),
                            ),
                          Expanded(
                            child: Text(
                              widget.image.sourceCount > maxFrames
                                  ? 'First $maxFrames of ${widget.image.sourceCount} frames'
                                  : '${widget.image.frames.length} frame${widget.image.frames.length == 1 ? '' : 's'}',
                              style: Theme.of(context).textTheme.bodySmall,
                            ),
                          ),
                          Text(
                            ready
                                ? '${(encodedSize / 1024).toStringAsFixed(1)} KiB'
                                : 'Updating...',
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                        ],
                      ),
                      Row(
                        children: [
                          for (var i = 0; i < 3; i++)
                            Expanded(
                              child: Tooltip(
                                message:
                                    '${['Source', 'Tone', 'Place'][i]} adjustments',
                                child: TextButton(
                                  style: TextButton.styleFrom(
                                    backgroundColor: section == i
                                        ? Theme.of(
                                            context,
                                          ).colorScheme.secondaryContainer
                                        : null,
                                  ),
                                  onPressed: () {
                                    if (scroll.hasClients) scroll.jumpTo(0);
                                    setState(() => section = i);
                                  },
                                  child: Row(
                                    mainAxisAlignment: MainAxisAlignment.center,
                                    children: [
                                      Icon(
                                        [
                                          Icons.image_outlined,
                                          Icons.tune,
                                          Icons.crop,
                                        ][i],
                                        size: 20,
                                      ),
                                      if (MediaQuery.textScalerOf(
                                            context,
                                          ).scale(14) <=
                                          20) ...[
                                        const SizedBox(width: 6),
                                        Flexible(
                                          child: Text(
                                            ['Source', 'Tone', 'Place'][i],
                                          ),
                                        ),
                                      ],
                                    ],
                                  ),
                                ),
                              ),
                            ),
                        ],
                      ),
                      const SizedBox(height: 12),
                      SizedBox(
                        height: (box.maxHeight * .38).clamp(130.0, 264.0),
                        child: Scrollbar(
                          controller: scroll,
                          thumbVisibility: true,
                          child: SingleChildScrollView(
                            controller: scroll,
                            child: Padding(
                              padding: const EdgeInsets.fromLTRB(0, 8, 8, 0),
                              child: controls(),
                            ),
                          ),
                        ),
                      ),
                      if (error != null)
                        Text(
                          error!,
                          style: TextStyle(
                            color: Theme.of(context).colorScheme.error,
                          ),
                        ),
                      const SizedBox(height: 8),
                      SizedBox(
                        width: double.infinity,
                        child: FilledButton.icon(
                          onPressed: ready && encodedSize <= maxArtworkBytes
                              ? () => Navigator.pop(context, current)
                              : null,
                          icon: const Icon(Icons.check),
                          label: const Text('Use design'),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
