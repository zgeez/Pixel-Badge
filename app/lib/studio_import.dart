// SPDX-License-Identifier: MIT
import 'dart:convert';
import 'dart:io';
import 'dart:ui' as ui;
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:file_selector/file_selector.dart';
import 'image_import.dart';
import 'studio_model.dart';
import 'artwork_codec.dart';

const editorMedia = MethodChannel('com.ziadg.badge_app/editor');

Future<void> copyStudioData(MatrixDesign design) => Clipboard.setData(
  ClipboardData(
    text: jsonEncode({
      'format': 'pixel-badge',
      'version': 2,
      'artwork': base64Encode(design.encode()),
    }),
  ),
);
MatrixDesign parseStudioData(String text) {
  if (text.length > 4194304) {
    throw const FormatException('Design file is too large.');
  }
  final value = jsonDecode(text);
  if (value is Map &&
      value['format'] == 'pixel-badge' &&
      value['artwork'] is String) {
    return MatrixDesign.decode(base64Decode(value['artwork'] as String));
  }
  final source = value is Map ? value['frames'] : value;
  if (source is! List || source.isEmpty) {
    throw const FormatException(
      'Use a Pixel Badge design or an array of square pixel frames.',
    );
  }
  final frames = source.first is num ? [source] : source;
  if (frames.length > maxFrames) {
    throw const FormatException('Import up to 128 frames.');
  }
  final out = <Uint8List>[], durations = <int>[];
  for (final frame in frames) {
    final pixels = frame is Map ? frame['pixels'] : frame;
    if (pixels is! List ||
        (pixels.length != 1024 &&
            pixels.length != 625 &&
            pixels.length != 400)) {
      throw const FormatException(
        'Use rectangular 32x32, 25x25 or 20x20 pixel arrays.',
      );
    }
    if (pixels.any((v) => v is! int || v < 0 || v > 255)) {
      throw const FormatException('Pixel brightness must be 0-255.');
    }
    final side = pixels.length == 1024
        ? 32
        : pixels.length == 625
        ? 25
        : 20;
    final rgba = Uint8List(side * side * 4);
    for (var i = 0; i < pixels.length; i++) {
      rgba.setRange(i * 4, i * 4 + 4, [pixels[i], pixels[i], pixels[i], 255]);
    }
    out.add(rasterize(RasterFrame(side, side, rgba, 150), grayscale: true));
    final duration = frame is Map ? frame['duration'] : 150;
    if (duration is! int || duration < 50 || duration > 2000) {
      throw const FormatException('Frame duration must be 50-2000 ms.');
    }
    durations.add(duration);
  }
  return MatrixDesign(frames: out, durations: durations);
}

Future<MatrixDesign?> importStudioData(BuildContext context) =>
    showDialog<MatrixDesign>(
      context: context,
      builder: (_) => const StudioDataDialog(),
    );

class StudioDataDialog extends StatefulWidget {
  const StudioDataDialog({super.key});
  @override
  State<StudioDataDialog> createState() => _StudioDataDialogState();
}

class _StudioDataDialogState extends State<StudioDataDialog> {
  final text = TextEditingController();
  String? error;
  @override
  void dispose() {
    text.dispose();
    super.dispose();
  }

  Future<void> loadFile() async {
    try {
      final file = await openFile(
        acceptedTypeGroups: const [
          XTypeGroup(
            label: 'Design JSON',
            extensions: ['json'],
            mimeTypes: ['application/json'],
            uniformTypeIdentifiers: ['public.json'],
          ),
        ],
      );
      if (file == null) return;
      if (await file.length() > 4194304) {
        throw const FormatException('File is too large.');
      }
      final value = await file.readAsString();
      if (mounted) {
        setState(() {
          text.text = value;
          error = null;
        });
      }
    } catch (_) {
      if (mounted) setState(() => error = 'Could not open this design.');
    }
  }

  @override
  Widget build(BuildContext context) => AlertDialog(
    title: const Text('Import design data'),
    content: SizedBox(
      width: 400,
      child: SingleChildScrollView(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: text,
              minLines: 4,
              maxLines: 10,
              decoration: InputDecoration(
                hintText: 'Paste design JSON',
                errorText: error,
                errorMaxLines: 3,
              ),
            ),
            TextButton.icon(
              onPressed: loadFile,
              icon: const Icon(Icons.folder_open),
              label: const Text('Open JSON file'),
            ),
          ],
        ),
      ),
    ),
    actions: [
      TextButton(
        onPressed: () => Navigator.pop(context),
        child: const Text('Cancel'),
      ),
      FilledButton(
        onPressed: () {
          try {
            Navigator.pop(context, parseStudioData(text.text));
          } catch (_) {
            setState(
              () => error =
                  'Invalid design. Use Pixel Badge JSON or square 0-255 frame arrays.',
            );
          }
        },
        child: const Text('Import'),
      ),
    ],
  );
}

class RasterTextDialog extends StatefulWidget {
  const RasterTextDialog({super.key});
  @override
  State<RasterTextDialog> createState() => _RasterTextDialogState();
}

class _RasterTextDialogState extends State<RasterTextDialog> {
  final text = TextEditingController();
  double size = 24;
  bool pixelFont = false, busy = false;
  String? error;
  @override
  void dispose() {
    text.dispose();
    super.dispose();
  }

  Future<void> generate() async {
    if (text.text.trim().isEmpty) return;
    setState(() => busy = true);
    ui.Image? image;
    ui.Picture? picture;
    try {
      if (pixelFont) {
        final design = TextDesign(text: text.text, scale: 0);
        design.encode(); // Validate pixel-font text before rasterizing.
        final frame = design.render(0);
        final rgba = Uint8List(32 * 32 * 4);
        for (var i = 0; i < 1024; i++) {
          final level = pixelBrightness(frame, i % 32, i ~/ 32);
          rgba.setRange(i * 4, i * 4 + 4, [level, level, level, 255]);
        }
        if (mounted) {
          Navigator.pop(
            context,
            ImportedRaster([RasterFrame(32, 32, rgba, 150)], 1),
          );
        }
        return;
      }
      final recorder = ui.PictureRecorder(), canvas = Canvas(recorder);
      canvas.drawColor(Colors.black, BlendMode.src);
      final painter = TextPainter(
        text: TextSpan(
          text: text.text,
          style: TextStyle(
            color: Colors.white,
            fontSize: size,
            fontWeight: FontWeight.w600,
          ),
        ),
        textDirection: TextDirection.ltr,
        textAlign: TextAlign.center,
      );
      painter.layout(maxWidth: 64);
      painter.paint(
        canvas,
        Offset((64 - painter.width) / 2, (64 - painter.height) / 2),
      );
      painter.dispose();
      picture = recorder.endRecording();
      image = await picture.toImage(64, 64);
      final bytes = await image.toByteData(
        format: ui.ImageByteFormat.rawStraightRgba,
      );
      if (bytes == null) {
        throw const FormatException('Text could not be rendered.');
      }
      if (mounted) {
        Navigator.pop(
          context,
          ImportedRaster([
            RasterFrame(
              64,
              64,
              Uint8List.fromList(bytes.buffer.asUint8List()),
              150,
            ),
          ], 1),
        );
      }
    } catch (_) {
      if (mounted) {
        setState(() => error = 'Choose shorter text, or use System for emoji.');
      }
    } finally {
      image?.dispose();
      picture?.dispose();
      if (mounted) setState(() => busy = false);
    }
  }

  @override
  Widget build(BuildContext context) => AlertDialog(
    title: const Text('Text / Emoji to pixels'),
    scrollable: true,
    content: SizedBox(
      width: 360,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          TextField(
            controller: text,
            maxLength: 96,
            maxLines: 3,
            decoration: InputDecoration(
              hintText: 'Text or emoji',
              errorText: error,
            ),
          ),
          SegmentedButton<bool>(
            showSelectedIcon: false,
            segments: const [
              ButtonSegment(value: false, label: Text('System')),
              ButtonSegment(value: true, label: Text('Pixel')),
            ],
            selected: {pixelFont},
            onSelectionChanged: (v) => setState(() => pixelFont = v.first),
          ),
          if (!pixelFont)
            Slider(
              min: 8,
              max: 48,
              value: size,
              label: '${size.round()} px',
              onChanged: (v) => setState(() => size = v),
            ),
        ],
      ),
    ),
    actions: [
      TextButton(
        onPressed: () => Navigator.pop(context),
        child: const Text('Cancel'),
      ),
      FilledButton(
        onPressed: busy ? null : generate,
        child: const Text('Preview'),
      ),
    ],
  );
}

Future<ImportedRaster?> pickVideo(BuildContext context) async {
  if (!Platform.isAndroid) {
    throw StateError('Video import is available on Android.');
  }
  final file = await openFile(
    acceptedTypeGroups: const [
      XTypeGroup(
        label: 'Video',
        extensions: ['mp4', 'mov', 'webm'],
        mimeTypes: ['video/*'],
      ),
    ],
  );
  if (file == null) return null;
  if (await file.length() > 128 * 1024 * 1024) {
    throw const FormatException('Choose a video up to 128 MB.');
  }
  final info = await editorMedia.invokeMapMethod<String, dynamic>('videoInfo', {
    'path': file.path,
  });
  if (!context.mounted) return null;
  final duration = (info!['duration'] as int) / 1000;
  if (duration < .05) {
    throw const FormatException('Choose a video at least 50 ms long.');
  }
  final clip = await showDialog<(double, double, int)>(
    context: context,
    builder: (_) => VideoClipDialog(duration: duration),
  );
  if (clip == null) return null;
  final result = await editorMedia.invokeListMethod<dynamic>('videoFrames', {
    'path': file.path,
    'start': (clip.$1 * 1000).round(),
    'end': (clip.$2 * 1000).round(),
    'fps': clip.$3,
  });
  if (result == null || result.isEmpty) {
    throw const FormatException('No video frames could be read.');
  }
  return ImportedRaster([
    for (final item in result)
      RasterFrame(
        item['width'] as int,
        item['height'] as int,
        item['pixels'] as Uint8List,
        item['duration'] as int,
      ),
  ], result.length);
}

class VideoClipDialog extends StatefulWidget {
  const VideoClipDialog({super.key, required this.duration});
  final double duration;
  @override
  State<VideoClipDialog> createState() => _VideoClipDialogState();
}

class _VideoClipDialogState extends State<VideoClipDialog> {
  late RangeValues range = RangeValues(0, widget.duration.clamp(.05, 3.2));
  int fps = 10;
  @override
  Widget build(BuildContext context) => AlertDialog(
    title: const Text('Video clip'),
    content: SizedBox(
      width: 360,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text(
            '${range.start.toStringAsFixed(1)} - ${range.end.toStringAsFixed(1)} seconds',
          ),
          RangeSlider(
            min: 0,
            max: widget.duration,
            values: range,
            onChanged: (v) => setState(() => range = v),
          ),
          Text('$fps FPS / up to $maxFrames frames'),
          Slider(
            min: 5,
            max: 20,
            divisions: 15,
            value: fps.toDouble(),
            onChanged: (v) => setState(() => fps = v.round()),
          ),
          const Text('Frames are sampled across the selected clip.'),
        ],
      ),
    ),
    actions: [
      TextButton(
        onPressed: () => Navigator.pop(context),
        child: const Text('Cancel'),
      ),
      FilledButton(
        onPressed: range.end - range.start < .05 || range.end - range.start > 64
            ? null
            : () => Navigator.pop(context, (range.start, range.end, fps)),
        child: const Text('Import'),
      ),
    ],
  );
}

Future<void> exportStudio(MatrixDesign design, {bool png = false}) async {
  Uint8List data;
  final type = png ? 'image/png' : 'application/json',
      name = png ? 'pixel-badge.png' : 'pixel-badge.json';
  if (png) {
    final pixels = grayPixels(design.current), rgba = Uint8List(4096);
    for (var i = 0; i < 1024; i++) {
      rgba.setRange(i * 4, i * 4 + 4, [pixels[i], pixels[i], pixels[i], 255]);
    }
    final buffer = await ui.ImmutableBuffer.fromUint8List(rgba);
    ui.ImageDescriptor? descriptor;
    ui.Codec? codec;
    ui.Image? image;
    try {
      descriptor = ui.ImageDescriptor.raw(
        buffer,
        width: 32,
        height: 32,
        pixelFormat: ui.PixelFormat.rgba8888,
      );
      codec = await descriptor.instantiateCodec();
      image = (await codec.getNextFrame()).image;
      final bytes = await image.toByteData(format: ui.ImageByteFormat.png);
      data = Uint8List.fromList(bytes!.buffer.asUint8List());
    } finally {
      image?.dispose();
      codec?.dispose();
      descriptor?.dispose();
      buffer.dispose();
    }
  } else {
    data = Uint8List.fromList(
      utf8.encode(
        jsonEncode({
          'format': 'pixel-badge',
          'version': 2,
          'artwork': base64Encode(design.encode()),
        }),
      ),
    );
  }
  if (Platform.isAndroid) {
    await editorMedia.invokeMethod<bool>('export', {
      'name': name,
      'mime': type,
      'bytes': data,
    });
  } else {
    final location = await getSaveLocation(suggestedName: name);
    if (location != null) {
      await XFile.fromData(
        data,
        mimeType: type,
        name: name,
      ).saveTo(location.path);
    }
  }
}

Future<ImportedRaster?> pasteStudioImage() async {
  if (!Platform.isAndroid) {
    throw StateError(
      'Image paste is available on Android; use file import here.',
    );
  }
  final path = await editorMedia.invokeMethod<String>('clipboardImage');
  if (path == null) return null;
  try {
    return await decodeRasterFile(path);
  } finally {
    try {
      await File(path).delete();
    } on FileSystemException {
      // Android may already have reclaimed the temporary cache file.
    }
  }
}
