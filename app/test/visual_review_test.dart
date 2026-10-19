// SPDX-License-Identifier: MIT
import 'dart:io';
import 'package:badge_app/audio_controller.dart';
import 'package:badge_app/audio_page.dart';
import 'package:badge_app/badge_controller.dart';
import 'package:badge_app/draft_store.dart';
import 'package:badge_app/image_import.dart';
import 'package:badge_app/link_editor.dart';
import 'package:badge_app/main.dart';
import 'package:badge_app/protocol.dart';
import 'package:badge_app/studio_import.dart';
import 'package:badge_app/studio_model.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'import_test.dart' show dotGrid, waitForImport;
import 'motion_test.dart' show MotionLink;
import 'screen_test.dart' show showApp, page, capture, loadUiFonts;

Future<void> click(WidgetTester tester, Finder target) async {
  await tester.ensureVisible(target);
  await tester.tap(target);
  await tester.pump();
  await tester.pump(const Duration(milliseconds: 300));
}

Future<void> dismiss(WidgetTester tester) async {
  await tester.tapAt(const Offset(2, 2));
  await tester.pumpAndSettle();
}

Future<void> surface(
  WidgetTester tester,
  Widget child,
  Brightness brightness, {
  double width = 390,
  double height = 844,
  double scale = 1,
  double keyboard = 0,
}) async {
  tester.view.physicalSize = Size(width, height);
  tester.view.devicePixelRatio = 1;
  addTearDown(tester.view.resetPhysicalSize);
  addTearDown(tester.view.resetDevicePixelRatio);
  await tester.pumpWidget(
    RepaintBoundary(
      key: const Key('capture'),
      child: MaterialApp(
        debugShowCheckedModeBanner: false,
        theme: const SmartBadgeApp().theme(brightness),
        builder: (context, content) => MediaQuery(
          data: MediaQuery.of(context).copyWith(
            textScaler: TextScaler.linear(scale),
            viewInsets: EdgeInsets.only(bottom: keyboard),
          ),
          child: content!,
        ),
        home: Builder(
          builder: (context) => Scaffold(
            body: TextButton(
              onPressed: () => showDialog<void>(
                context: context,
                useSafeArea: false,
                builder: (_) => child,
              ),
              child: const Text('Open surface'),
            ),
          ),
        ),
      ),
    ),
  );
  await tester.runAsync(() async {
    await tester.tap(find.text('Open surface'));
    await tester.pump();
    await Future<void>.delayed(const Duration(milliseconds: 40));
  });
  await tester.pump(const Duration(milliseconds: 300));
}

class ReviewAudio extends AudioController {
  ReviewAudio() : super(channel: const MethodChannel('visual-audio'));
  @override
  bool get android => true;
  @override
  bool get available => true;
}

void main() {
  setUpAll(loadUiFonts);
  for (final brightness in [Brightness.light, Brightness.dark]) {
    final theme = brightness.name;
    testWidgets('review all pages and workspace menus $theme', (tester) async {
      final matrix = MatrixDesign(frames: [Uint8List(1024)]);
      for (var y = 6; y < 26; y++) {
        for (var x = 6; x < 26; x++) {
          if ((x - 16) * (x - 16) + (y - 16) * (y - 16) < 100) {
            setBrightness(matrix.current, x, y, (x * 6 + y * 3).clamp(0, 255));
          }
        }
      }
      matrix.add(duplicate: true);
      final store = MemoryDraftStore();
      await store.save(
        StudioDraft(
          text: const TextDesign(text: 'Hello badge'),
          matrix: matrix,
          links: [const SavedLink('Work', 'HTTPS://A.B/X')],
          library: [
            SavedDesign(
              id: 'text',
              name: 'A little hello',
              isText: true,
              bytes: const TextDesign(text: 'Hello').encode(),
            ),
            SavedDesign(
              id: 'art',
              name: 'Soft orbit',
              isText: false,
              bytes: matrix.encode(),
            ),
          ],
        ),
      );
      final link = MotionLink();
      link.values[BadgeField.artwork.uuid] = matrix.encode();
      link.values[BadgeField.mode.uuid] = Uint8List.fromList([3]);
      final badge = BadgeController();
      await badge.attach(
        link,
        capabilities: BadgeCapabilities.decode([2, 25, 32, 4, 7, 3, 128, 255]),
      );
      await showApp(
        tester,
        controller: badge,
        store: store,
        brightness: brightness,
      );
      await capture(tester, 'review-$theme-badge');
      await click(tester, find.byKey(const Key('display-mode')));
      await capture(tester, 'review-$theme-display-modes');
      await dismiss(tester);
      await click(tester, find.byTooltip('Motion settings'));
      await capture(tester, 'review-$theme-motion');
      await click(tester, find.text('Auto rotate'));
      await capture(tester, 'review-$theme-motion-manual');
      await click(tester, find.text('Done'));
      await click(tester, find.byTooltip('Badge options'));
      await capture(tester, 'review-$theme-options');
      await click(tester, find.text('Pairing help'));
      await capture(tester, 'review-$theme-pairing');
      await click(tester, find.text('Done'));
      await click(tester, find.byTooltip('Badge options'));
      await click(tester, find.text('About'));
      await capture(tester, 'review-$theme-about');
      await click(tester, find.text('View licenses'));
      await tester.pump(const Duration(seconds: 1));
      await capture(tester, 'review-$theme-licenses');
      await tester.pageBack();
      await tester.pumpAndSettle();
      await click(tester, find.text('Close'));
      await page(tester, 'Links');
      await capture(tester, 'review-$theme-links');
      await click(tester, find.byTooltip('NFC details'));
      await capture(tester, 'review-$theme-nfc');
      await click(tester, find.text('Done'));
      await click(tester, find.byTooltip('Add a custom link'));
      await capture(tester, 'review-$theme-add-link');
      await click(tester, find.text('QR limits'));
      await capture(tester, 'review-$theme-qr-limits');
      await click(tester, find.text('Cancel'));
      await click(tester, find.byTooltip('Links library'));
      await capture(tester, 'review-$theme-links-library');
      await click(tester, find.byTooltip('Edit Work').last);
      await capture(tester, 'review-$theme-edit-link');
      await click(tester, find.text('Cancel'));
      await tester.pageBack();
      await tester.pumpAndSettle();
      await page(tester, 'Text');
      await capture(tester, 'review-$theme-text');
      for (final tooltip in ['Text layout', 'Alignment', 'Text speed']) {
        await click(tester, find.byTooltip(tooltip));
        await capture(tester, 'review-$theme-${tooltip.replaceAll(' ', '-')}');
        if (tooltip == 'Text speed') {
          await click(tester, find.text('Done'));
        } else {
          await dismiss(tester);
        }
      }
      await click(tester, find.byTooltip('Text size'));
      await capture(tester, 'review-$theme-text-size');
      await dismiss(tester);
      await click(tester, find.byTooltip('Open library'));
      await capture(tester, 'review-$theme-text-library');
      await click(tester, find.byTooltip('Rename A little hello'));
      FocusManager.instance.primaryFocus?.unfocus();
      await tester.pump();
      await capture(tester, 'review-$theme-library-rename');
      await click(tester, find.text('Cancel'));
      await click(tester, find.text('A little hello'));
      await capture(tester, 'review-$theme-library-load');
      await click(tester, find.text('Cancel'));
      await tester.pageBack();
      await tester.pumpAndSettle();
      await page(tester, 'Studio');
      await capture(tester, 'review-$theme-studio');
      await click(tester, find.byTooltip('Brush size'));
      await capture(tester, 'review-$theme-brush-size');
      await dismiss(tester);
      for (final category in ['Brush', 'Actions', 'Media']) {
        await click(tester, find.byTooltip(category));
        await capture(tester, 'review-$theme-studio-$category');
        await dismiss(tester);
      }
      await click(tester, find.text('150 ms'));
      await capture(tester, 'review-$theme-animation');
      await click(tester, find.byTooltip('Playback'));
      await capture(tester, 'review-$theme-animation-playback');
      await dismiss(tester);
      await click(tester, find.text('Done'));
      await click(tester, find.byTooltip('Open library'));
      await capture(tester, 'review-$theme-design-library');
      await tester.pageBack();
      await tester.pumpAndSettle();
      await page(tester, 'Audio');
      await capture(tester, 'review-$theme-audio-offline');
      await click(tester, find.byTooltip('Audio help'));
      await capture(tester, 'review-$theme-audio-help');
      await click(tester, find.text('Done'));
      await click(tester, find.text('Recordings (0)'));
      await capture(tester, 'review-$theme-recordings-empty');
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
      badge.dispose();
      await link.dispose();
    });

    testWidgets('review audio states and recording menus $theme', (
      tester,
    ) async {
      const native = MethodChannel('visual-audio');
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(native, (_) async => null);
      final audio = ReviewAudio()
        ..message = 'Ready to record'
        ..state = AudioState(
          35,
          0,
          Uint8List(16),
          Uint8List(16),
          Uint8List(16),
          Uint8List(65),
        );
      final id =
          '${DateTime(2026, 10, 17, 13, 30).millisecondsSinceEpoch}-clip.pba';
      audio.recordings = [
        {'id': id, 'seconds': 65.0},
      ];
      Widget recorder() => Dialog.fullscreen(
        child: Scaffold(
          appBar: AppBar(toolbarHeight: 48, title: const Text('Pixel Badge')),
          body: Padding(
            padding: const EdgeInsets.all(12),
            child: AudioPage(
              audio: audio,
              compact: true,
              openLibrary: () {},
              selectVisualization: (_) async {},
            ),
          ),
        ),
      );
      await surface(tester, recorder(), brightness);
      await capture(tester, 'review-$theme-audio-ready');
      audio.receiving = true;
      await tester.pumpWidget(const SizedBox());
      await surface(tester, recorder(), brightness);
      await capture(tester, 'review-$theme-audio-recording');
      audio.receiving = false;
      audio.playback = PlaybackState(
        id: id,
        playing: true,
        positionMs: 24000,
        durationMs: 65000,
      );
      await tester.pumpWidget(const SizedBox());
      await surface(tester, recorder(), brightness);
      await capture(tester, 'review-$theme-audio-playback');
      await tester.pumpWidget(const SizedBox());
      await surface(
        tester,
        Dialog.fullscreen(
          child: Scaffold(
            appBar: AppBar(title: const Text('Recordings')),
            body: ListView(
              padding: const EdgeInsets.all(12),
              children: [
                Builder(
                  builder: (c) => AudioPage(
                    audio: audio,
                    selectVisualization: (_) async {},
                  ).recordingTile(c, audio.recordings.first),
                ),
              ],
            ),
          ),
        ),
        brightness,
      );
      await capture(tester, 'review-$theme-recordings');
      await click(tester, find.byTooltip('Recording options'));
      await capture(tester, 'review-$theme-recording-menu');
      await click(tester, find.text('Delete'));
      await capture(tester, 'review-$theme-recording-delete');
      await click(tester, find.text('Cancel'));
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
      audio.dispose();
      await tester.pump();
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(native, null);
    });

    testWidgets('review media and secondary dialogs $theme', (tester) async {
      for (final entry in <(String, Widget)>[
        ('data', const StudioDataDialog()),
        ('raster-text', const RasterTextDialog()),
        ('video', const VideoClipDialog(duration: 12)),
        ('save-library', const LibraryNameDialog(initial: 'Soft orbit')),
      ]) {
        await surface(tester, entry.$2, brightness);
        FocusManager.instance.primaryFocus?.unfocus();
        await tester.pump();
        await capture(tester, 'review-$theme-${entry.$1}');
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      }
      final source = ImportedRaster([dotGrid()], 1);
      await surface(tester, ImportDesignDialog(image: source), brightness);
      await waitForImport(tester);
      for (final tab in ['Source', 'Tone', 'Place']) {
        await click(tester, find.byTooltip('$tab adjustments'));
        await capture(tester, 'review-$theme-import-$tab');
      }
      await click(tester, find.text('Source').first);
      await click(tester, find.byTooltip('Source type'));
      await click(tester, find.text('Image / photo').last);
      await waitForImport(tester);
      await capture(tester, 'review-$theme-import-image');
      await click(tester, find.text('Place'));
      await capture(tester, 'review-$theme-import-position');
      await click(tester, find.text('Tone'));
      await click(tester, find.text('Threshold'));
      await waitForImport(tester);
      await capture(tester, 'review-$theme-import-threshold');
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    });
  }

  testWidgets(
    'import fits a narrow phone and large text with accessible controls',
    (tester) async {
      await surface(
        tester,
        ImportDesignDialog(image: ImportedRaster([dotGrid()], 1)),
        Brightness.dark,
        width: 320,
        height: 640,
        scale: 2,
      );
      await waitForImport(tester);
      for (final tab in ['Source', 'Tone', 'Place']) {
        await click(tester, find.byTooltip('$tab adjustments'));
        await capture(tester, 'review-narrow-import-$tab');
        final bottom = find.text(
          tab == 'Source'
              ? '25 x 25 pixels, centered on the badge.'
              : tab == 'Tone'
              ? 'Invert'
              : 'Reset adjustments',
        );
        await tester.ensureVisible(bottom);
        await tester.pump();
        await capture(tester, 'review-narrow-import-$tab-bottom');
        expect(tester.takeException(), isNull);
      }
      await tester.pumpWidget(const SizedBox());
      await surface(
        tester,
        const RasterTextDialog(),
        Brightness.dark,
        width: 320,
        height: 640,
        scale: 1.5,
        keyboard: 260,
      );
      await capture(tester, 'review-keyboard-raster-text');
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    },
  );

  const mediaRoot = String.fromEnvironment('GLYPH_REVIEW');
  if (mediaRoot.isNotEmpty) {
    testWidgets('review actual Cat and Eye imports', (tester) async {
      for (final i in [2, 3]) {
        final source = await tester.runAsync(
          () async => decodeRaster(
            await File('$mediaRoot/source-$i.gif').readAsBytes(),
          ),
        );
        await surface(
          tester,
          ImportDesignDialog(image: source!),
          Brightness.dark,
        );
        await waitForImport(tester);
        await tester.pump(const Duration(milliseconds: 500));
        await capture(tester, 'review-actual-glyph-$i');
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      }
    });
  }
}
