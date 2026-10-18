// SPDX-License-Identifier: MIT
import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:universal_ble/universal_ble.dart';
import 'badge_controller.dart';
import 'ble_link.dart';
import 'draft_store.dart';
import 'matrix_widgets.dart';
import 'protocol.dart';
import 'studio_model.dart';
import 'audio_controller.dart';
import 'audio_page.dart';
import 'motion_panel.dart';
import 'link_editor.dart';
import 'image_import.dart';
import 'artwork_codec.dart';
import 'studio_import.dart';
import 'selection_control.dart';

part 'compact_home.dart';

const modeLabels = [
  'QR link',
  'Text',
  'Drawing',
  'Animation',
  'Audio level',
  'Audio spectrum',
  'Fluid',
];
const pageLabels = ['Badge', 'Links', 'Text', 'Studio', 'Audio'];
const pageIcons = [
  Icons.badge_outlined,
  Icons.link,
  Icons.text_fields,
  Icons.grid_on,
  Icons.mic_none,
];

class BadgeHome extends StatefulWidget {
  const BadgeHome({
    super.key,
    this.controller,
    this.scanResults,
    this.draftStore,
  });
  final BadgeController? controller;
  final Stream<BleDevice>? scanResults;
  final DraftStore? draftStore;
  @override
  State<BadgeHome> createState() => _BadgeHomeState();
}

class _BadgeHomeState extends State<BadgeHome> with WidgetsBindingObserver {
  final audio = AudioController();
  late final badge = widget.controller ?? BadgeController();
  late final store =
      widget.draftStore ??
      (widget.controller == null ? DeviceDraftStore() : MemoryDraftStore());
  StudioDraft draft = StudioDraft();
  final textEditor = TextEditingController();
  final textFocus = FocusNode();
  final devices = <String, BleDevice>{};
  final scanState = ValueNotifier<int>(0), clock = ValueNotifier<int>(0);
  final watch = Stopwatch()..start();
  StreamSubscription<BleDevice>? scanSubscription;
  Timer? scanTimer, previewTimer, saveTimer;
  Future<void> saves = Future.value();
  final libraryChanges = ValueNotifier<int>(0);
  int page = 0;
  int studioToolPage = 0;
  int _budgetRevision = -1, _budgetBytes = 132;
  MatrixDesign? _budgetDesign;
  final _framePresence = <bool>[];
  bool _artworkHasPixels = false, drawing = false, strokeEdit = false;
  bool disposed = false;
  Timer? budgetTimer;
  Uint8List? _appliedArtworkBytes;
  MatrixDesign? _appliedArtwork;
  MatrixDesign? get appliedArtwork {
    final value = badge.values[BadgeField.artwork];
    if (!identical(value, _appliedArtworkBytes)) {
      _appliedArtworkBytes = value;
      _appliedArtwork = value == null ? null : MatrixDesign.decode(value);
    }
    return _appliedArtwork;
  }

  bool get artworkHasPixels {
    artworkBudgetBytes;
    return _artworkHasPixels;
  }

  int get artworkBudgetBytes {
    if (_budgetRevision != draftRevision ||
        !identical(_budgetDesign, draft.matrix)) {
      final matrix = draft.matrix;
      if (strokeEdit &&
          identical(_budgetDesign, matrix) &&
          _framePresence.length == matrix.frames.length) {
        _framePresence[matrix.selected] = matrix.current.any((v) => v != 0);
      } else {
        _framePresence
          ..clear()
          ..addAll(matrix.frames.map((f) => f.any((v) => v != 0)));
      }
      _artworkHasPixels = _framePresence.any((v) => v);
      // Conservative raw size until the settled draft is compressed off-thread.
      // Build/pointer updates never encode the whole animation.
      _budgetBytes = 6 + matrix.frames.fold<int>(0, (n, f) => n + 5 + f.length);
      _budgetRevision = draftRevision;
      _budgetDesign = matrix;
      if (!drawing) scheduleBudget();
    }
    return _budgetBytes;
  }

  void scheduleBudget() {
    budgetTimer?.cancel();
    budgetTimer = Timer(const Duration(milliseconds: 150), () async {
      if (!mounted || drawing) return;
      final revision = draftRevision, matrix = draft.matrix;
      final snapshot = MatrixDesign(
        frames: matrix.frames,
        durations: List.of(matrix.durations),
        interval: matrix.interval,
        playback: matrix.playback,
      );
      final int size;
      try {
        size = await compute(_artworkSize, snapshot);
      } catch (_) {
        // The conservative raw budget remains valid if compression fails.
        return;
      }
      if (mounted &&
          !drawing &&
          revision == draftRevision &&
          identical(matrix, draft.matrix)) {
        setState(() => _budgetBytes = size);
      }
    });
  }

  void finishStroke() {
    if (disposed) return;
    drawing = false;
    scheduleBudget();
    scheduleSave();
  }

  void update(VoidCallback work) {
    if (mounted) setState(work);
  }

  int draftRevision = 0;
  bool scanning = false,
      connecting = false,
      loadingDraft = true,
      playing = false,
      navigateCanvas = false,
      importing = false,
      foreground = true;
  bool onionSkin = false;
  double onionOpacity = .25;
  int playEpoch = 0;
  bool? connectionBond;
  PixelTool tool = PixelTool.pen;
  String? draftMessage, scanError;
  bool get ready =>
      badge.connected &&
      badge.confirmed &&
      badge.writable &&
      !badge.busy &&
      !connecting;
  bool get studioReady => ready && badge.capabilities.hasStudio;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    textFocus.addListener(changed);
    badge.addListener(changed);
    audio.addListener(changed);
    unawaited(audio.refreshLibrary().catchError((Object _) {}));
    scanSubscription = (widget.scanResults ?? UniversalBle.scanStream).listen((
      device,
    ) {
      if (mounted && scanning) {
        devices[device.deviceId] = device;
        scanState.value++;
      }
    });
    previewTimer = Timer.periodic(const Duration(milliseconds: 50), (_) {
      if (foreground) clock.value = watch.elapsedMilliseconds;
    });
    unawaited(loadDraft());
  }

  Future<void> loadDraft() async {
    final revision = draftRevision;
    try {
      final value = await store.load();
      if (!mounted) return;
      if (draftRevision != revision) return;
      draft = value;
      textEditor.text = draft.text.text;
    } catch (_) {
      draftMessage =
          'Your saved draft could not be opened. New edits stay here until you save a new draft.';
    } finally {
      if (mounted) setState(() => loadingDraft = false);
    }
  }

  void changed() {
    if (mounted) {
      if (!badge.connected && audio.transport != null) {
        unawaited(audio.detach());
      }
      setState(() {});
    }
  }

  void moveCursor(int direction) {
    final selection = textEditor.selection;
    final next = !selection.isValid
        ? textEditor.text.length
        : !selection.isCollapsed
        ? (direction < 0 ? selection.start : selection.end)
        : (selection.extentOffset + direction).clamp(0, textEditor.text.length);
    textEditor.value = textEditor.value.copyWith(
      selection: TextSelection.collapsed(offset: next),
      composing: TextRange.empty,
    );
    textFocus.requestFocus();
  }

  void editDraft({bool pixelsOnly = false}) {
    strokeEdit = pixelsOnly;
    if (!mounted) return;
    draftRevision++;
    libraryChanges.value++;
    setState(() => draftMessage = 'Draft updated');
    saveTimer?.cancel();
    budgetTimer?.cancel();
    if (!drawing) scheduleSave();
  }

  void scheduleSave() {
    saveTimer?.cancel();
    saveTimer = Timer(
      const Duration(milliseconds: 500),
      () => unawaited(saveDraft()),
    );
  }

  Future<void> saveDraft() async {
    saveTimer?.cancel();
    try {
      final snapshot = StudioDraft.decode(draft.encode());
      final revision = draftRevision;
      saves = saves.catchError((Object _) {}).then((_) => store.save(snapshot));
      await saves;
      if (mounted && revision == draftRevision) {
        setState(() => draftMessage = 'Saved on this phone');
      }
    } catch (_) {
      if (mounted) {
        setState(
          () => draftMessage = 'Draft not saved. Check the text or try again.',
        );
      }
    }
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    foreground = state == AppLifecycleState.resumed;
    if (foreground) unawaited(audio.resume());
    if (state == AppLifecycleState.paused ||
        state == AppLifecycleState.inactive) {
      audio.background();
    }
    if (state == AppLifecycleState.paused) {
      unawaited(stopScan());
      // Keep the secure BLE link; only audio capture stops on background.
      if (!loadingDraft) unawaited(saveDraft());
    }
  }

  String friendly(Object error) {
    if (error is FormatException) return error.message;
    if (error is StateError) return error.message;
    final text = '$error'.toLowerCase();
    if (text.contains('permission')) {
      return 'Allow Bluetooth access in your phone settings, then try again.';
    }
    if (text.contains('pair') || text.contains('bond')) {
      return 'Pairing failed. Hold Action on the badge, reconnect and enter the code shown on its display.';
    }
    return 'Could not reach the badge. Reconnect and refresh before trying again.';
  }

  Future<void> action(Future<void> Function() work) async {
    try {
      await work();
    } catch (error) {
      if (mounted) {
        notice(friendly(error));
      }
    }
  }

  Future<void> stopScan() async {
    scanTimer?.cancel();
    if (!scanning) return;
    scanning = false;
    if (mounted) {
      scanState.value++;
      setState(() {});
    }
    await UniversalBle.stopScan();
  }

  Future<void> scan() async {
    try {
      await UniversalBle.requestPermissions(withAndroidFineLocation: false);
      if (await UniversalBle.getBluetoothAvailabilityState() !=
          AvailabilityState.poweredOn) {
        throw StateError('Turn on Bluetooth to find your badge.');
      }
      devices.clear();
      scanning = true;
      scanError = null;
      scanState.value++;
      await UniversalBle.startScan(
        scanFilter: ScanFilter(withServices: [serviceUuid]),
      );
      scanTimer = Timer(
        const Duration(seconds: 12),
        () => unawaited(action(stopScan)),
      );
    } catch (error) {
      scanning = false;
      scanError = friendly(error);
      scanState.value++;
    }
  }

  Future<void> connect(BleDevice device) async {
    if (connecting) return;
    setState(() {
      connecting = true;
      connectionBond = null;
    });
    try {
      await stopScan();
      // Dispose the old GATT client before reconnecting the same device ID.
      await audio.detach();
      await badge.disconnect();
      final link = await UniversalBadgeLink.connect(
        device,
        onBondState: (paired) {
          if (mounted) setState(() => connectionBond = paired);
        },
      );
      if (!mounted) {
        await link.close();
        return;
      }
      await badge.attach(link, capabilities: link.capabilities);
      await audio.attach(link.capabilities.hasAudio ? link : null);
    } finally {
      if (mounted) setState(() => connecting = false);
    }
  }

  Future<void> findBadge() async {
    if (connecting || scanning) return;
    await scan();
    if (mounted) setState(() {});
    if (!scanning) {
      if (mounted && scanError != null) notice(scanError!);
      return;
    }
    await Future<void>.delayed(const Duration(seconds: 4));
    if (!mounted || !scanning || !foreground) return;
    await stopScan();
    if (!mounted) return;
    if (devices.length == 1) {
      await action(() => connect(devices.values.single));
      return;
    }
    await showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      showDragHandle: true,
      builder: (sheetContext) => SafeArea(
        child: Padding(
          padding: const EdgeInsets.fromLTRB(24, 8, 24, 24),
          child: ValueListenableBuilder<int>(
            valueListenable: scanState,
            builder: (context, _, _) => Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'Find your badge',
                  style: Theme.of(context).textTheme.headlineSmall,
                ),
                const SizedBox(height: 12),
                const Text(
                  'For first pairing, hold Action for 1 second after the display starts, then release. Connect within 60 seconds and enter the code shown on the badge. A paired phone can reconnect directly.',
                ),
                const SizedBox(height: 20),
                if (scanError != null)
                  Padding(
                    padding: const EdgeInsets.only(bottom: 12),
                    child: Text(
                      scanError!,
                      style: TextStyle(
                        color: Theme.of(context).colorScheme.error,
                      ),
                    ),
                  ),
                FilledButton.icon(
                  onPressed: () =>
                      unawaited(scanning ? action(stopScan) : scan()),
                  icon: Icon(scanning ? Icons.stop : Icons.bluetooth_searching),
                  label: Text(scanning ? 'Stop scanning' : 'Find badge'),
                ),
                if (scanning)
                  const Padding(
                    padding: EdgeInsets.only(top: 16),
                    child: LinearProgressIndicator(),
                  ),
                if (devices.isEmpty)
                  Padding(
                    padding: const EdgeInsets.symmetric(vertical: 20),
                    child: Text(
                      scanning
                          ? 'Looking for nearby badges...'
                          : 'Your badge will appear here. Keep it nearby and Bluetooth on.',
                    ),
                  ),
                ConstrainedBox(
                  constraints: BoxConstraints(
                    maxHeight: MediaQuery.sizeOf(context).height * .35,
                  ),
                  child: ListView(
                    shrinkWrap: true,
                    children: [
                      for (final device in devices.values)
                        ListTile(
                          leading: const Icon(Icons.badge_outlined),
                          title: Text(
                            device.name?.isNotEmpty == true
                                ? device.name!
                                : 'Smart LED Badge',
                          ),
                          subtitle: Text('Signal ${device.rssi ?? '-'} dBm'),
                          trailing: const Icon(Icons.chevron_right),
                          onTap: () {
                            Navigator.pop(sheetContext);
                            unawaited(action(() => connect(device)));
                          },
                        ),
                    ],
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
    await action(stopScan);
  }

  Widget preview(Uint8List Function(int) render, {bool animate = false}) =>
      Center(
        child: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 360),
          child: animate
              ? ValueListenableBuilder<int>(
                  valueListenable: clock,
                  builder: (context, value, _) =>
                      MatrixPreview(frame: render(value)),
                )
              : MatrixPreview(frame: render(0)),
        ),
      );
  void notice(String text, {SnackBarAction? action}) {
    if (!mounted) return;
    ScaffoldMessenger.of(context)
      ..hideCurrentSnackBar()
      ..showSnackBar(
        SnackBar(
          content: Text(text),
          action: action,
          persist: false,
          duration: const Duration(seconds: 4),
          showCloseIcon: true,
        ),
      );
  }

  Future<void> editLink({int? index}) async {
    final result = await showDialog<SavedLink>(
      context: context,
      builder: (_) =>
          LinkEditorDialog(original: index == null ? null : draft.links[index]),
    );
    if (result == null || !mounted) return;
    if (index == null) {
      if (draft.links.length >= 32) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text(
              'Keep up to 32 saved links. Remove one to add another.',
            ),
          ),
        );
        return;
      }
      draft.links.add(result);
    } else {
      draft.links[index] = result;
    }
    editDraft();
  }

  Future<void> importStudioSource({
    bool text = false,
    bool video = false,
  }) async {
    if (loadingDraft || importing) return;
    final editorContext = context;
    setState(() => importing = true);
    try {
      MatrixDesign? design;
      if (text || video) {
        ImportedRaster? image;
        if (text) {
          image = await showDialog<ImportedRaster>(
            context: editorContext,
            builder: (_) => const RasterTextDialog(),
          );
        } else {
          image = await pickVideo(editorContext);
        }
        final source = image;
        if (source == null || !editorContext.mounted) return;
        design = await showDialog<MatrixDesign>(
          context: editorContext,
          builder: (_) => ImportDesignDialog(image: source),
        );
      } else {
        design = await importStudioData(editorContext);
      }
      if (design == null || !mounted) return;
      draft.matrix = design;
      playing = false;
      editDraft();
    } finally {
      if (mounted) setState(() => importing = false);
    }
  }

  String? textError() {
    try {
      draft.text.encode();
      return null;
    } catch (error) {
      return friendly(error);
    }
  }

  Future<void> loadBadgeDraft({required bool text}) async {
    final field = text ? BadgeField.text : BadgeField.artwork;
    final value = badge.values[field];
    if (value == null) return;
    Uint8List? current;
    try {
      current = text ? draft.text.encode() : draft.matrix.encode();
    } on FormatException {
      // An invalid local draft can still be replaced with valid badge content.
    }
    if (current != null && sameBytes(current, value)) return;
    final replacement = Uint8List.fromList(value);
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Replace this draft?'),
        content: Text(
          'Load the ${text ? 'text' : 'design'} last read from your badge. This replaces the draft saved on this phone.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Keep draft'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Load draft'),
          ),
        ],
      ),
    );
    if (confirmed != true || !mounted) return;
    if (text) {
      draft.text = TextDesign.decode(replacement);
      textEditor.text = draft.text.text;
    } else {
      draft.matrix = MatrixDesign.decode(replacement);
      playing = false;
    }
    editDraft();
  }

  Future<void> saveToLibrary(bool text) async {
    if (draft.library.length >= 64) {
      throw StateError('Your library holds 64 items. Remove one first.');
    }
    final bytes = text ? draft.text.encode() : draft.matrix.encode();
    final name = await showDialog<String>(
      context: context,
      builder: (_) =>
          LibraryNameDialog(initial: text ? 'Text design' : 'Pixel design'),
    );
    if (name == null || !mounted) return;
    draft.library.add(
      SavedDesign(
        id: DateTime.now().microsecondsSinceEpoch.toString(),
        name: name,
        isText: text,
        bytes: bytes,
      ),
    );
    editDraft();
    await saveDraft();
  }

  Future<void> restoreDesign(SavedDesign item) async {
    final replace = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text('Load ${item.name}?'),
        content: const Text(
          'This replaces the current editor. Saved library items stay unchanged.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Load'),
          ),
        ],
      ),
    );
    if (replace != true || !mounted) return;
    if (item.isText) {
      draft.text = TextDesign.decode(item.bytes);
      textEditor.text = draft.text.text;
    } else {
      draft.matrix = MatrixDesign.decode(item.bytes);
      playing = false;
    }
    editDraft();
  }

  Future<void> renameDesign(SavedDesign item) async {
    final name = await showDialog<String>(
      context: context,
      builder: (_) => LibraryNameDialog(initial: item.name, renaming: true),
    );
    if (name == null || !mounted) return;
    final index = draft.library.indexOf(item);
    if (index < 0) return;
    draft.library[index] = SavedDesign(
      id: item.id,
      name: name,
      isText: item.isText,
      bytes: item.bytes,
    );
    editDraft();
    await saveDraft();
  }

  Widget libraryPanel(bool text) {
    final items = draft.library.where((item) => item.isText == text).toList();
    return Column(
      children: [
        if (items.isEmpty)
          const Padding(
            padding: EdgeInsets.symmetric(vertical: 16),
            child: Text('Keep your favourites here.'),
          ),
        for (final item in items)
          ListTile(
            contentPadding: EdgeInsets.zero,
            leading: SizedBox(
              width: 52,
              height: 52,
              child: MatrixPreview(
                frame: text
                    ? item.textDesign.render(
                        0,
                        fallback: badge.name.isEmpty ? 'BADGE' : badge.name,
                      )
                    : item.matrixDesign.frames.first,
              ),
            ),
            title: Text(item.name),
            subtitle: text
                ? null
                : Text(
                    '${item.bytes[1]} frame${item.bytes[1] == 1 ? '' : 's'}',
                  ),
            onTap: () => unawaited(action(() => restoreDesign(item))),
            trailing: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                IconButton(
                  tooltip: 'Rename ${item.name}',
                  icon: const Icon(Icons.edit_outlined),
                  onPressed: () => unawaited(action(() => renameDesign(item))),
                ),
                IconButton(
                  tooltip: 'Remove ${item.name}',
                  icon: const Icon(Icons.delete_outline),
                  onPressed: () {
                    final index = draft.library.indexOf(item);
                    draft.library.removeAt(index);
                    editDraft();
                    notice(
                      '${item.name} removed',
                      action: SnackBarAction(
                        label: 'Undo',
                        onPressed: () {
                          if (draft.library.length < 64) {
                            draft.library.insert(
                              index.clamp(0, draft.library.length),
                              item,
                            );
                            editDraft();
                          }
                        },
                      ),
                    );
                  },
                ),
              ],
            ),
          ),
        if (items.isEmpty)
          TextButton.icon(
            onPressed: loadingDraft || (text && textError() != null)
                ? null
                : () => unawaited(action(() => saveToLibrary(text))),
            icon: const Icon(Icons.bookmark_add_outlined),
            label: const Text('Save to library'),
          ),
      ],
    );
  }

  Future<void> importImage() async {
    if (importing) return;
    setState(() => importing = true);
    try {
      final image = await pickRaster();
      if (image == null || !mounted) return;
      final design = await showDialog<MatrixDesign>(
        context: context,
        useSafeArea: false,
        builder: (_) => ImportDesignDialog(image: image),
      );
      if (design == null || !mounted) return;
      draft.matrix = design;
      playing = false;
      editDraft();
    } finally {
      if (mounted) setState(() => importing = false);
    }
  }

  @override
  Widget build(BuildContext context) => compactBuild(context);

  @override
  void dispose() {
    disposed = true;
    WidgetsBinding.instance.removeObserver(this);
    scanTimer?.cancel();
    previewTimer?.cancel();
    budgetTimer?.cancel();
    saveTimer?.cancel();
    unawaited(scanSubscription?.cancel());
    if (scanning) unawaited(UniversalBle.stopScan());
    audio.removeListener(changed);
    audio.dispose();
    badge.removeListener(changed);
    textFocus.removeListener(changed);
    textFocus.dispose();
    textEditor.dispose();
    scanState.dispose();
    clock.dispose();
    libraryChanges.dispose();
    if (widget.controller == null) badge.dispose();
    super.dispose();
  }
}

int _artworkSize(MatrixDesign design) => design.encode().length;
