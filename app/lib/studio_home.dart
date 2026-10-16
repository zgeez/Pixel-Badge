// SPDX-License-Identifier: MIT
import 'dart:async';
import 'dart:typed_data';
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
  final devices = <String, BleDevice>{};
  final scanState = ValueNotifier<int>(0), clock = ValueNotifier<int>(0);
  final watch = Stopwatch()..start();
  StreamSubscription<BleDevice>? scanSubscription;
  Timer? scanTimer, previewTimer, saveTimer;
  Future<void> saves = Future.value();
  int page = 0;
  int draftRevision = 0;
  bool scanning = false,
      connecting = false,
      loadingDraft = true,
      playing = false,
      navigateCanvas = false;
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
      clock.value = watch.elapsedMilliseconds;
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

  void editDraft() {
    if (!mounted) return;
    draftRevision++;
    setState(() => draftMessage = 'Draft updated');
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
    if (state == AppLifecycleState.resumed) unawaited(audio.resume());
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
    return 'Couldn’t reach the badge. Reconnect and refresh before trying again.';
  }

  Future<void> action(Future<void> Function() work) async {
    try {
      await work();
    } catch (error) {
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text(friendly(error))));
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
    setState(() => connecting = true);
    try {
      await stopScan();
      // Dispose the old GATT client before reconnecting the same device ID.
      await audio.detach();
      await badge.disconnect();
      final link = await UniversalBadgeLink.connect(device);
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
                          ? 'Looking for nearby badges…'
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
                          subtitle: Text('Signal ${device.rssi ?? '—'} dBm'),
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
  Widget offlineNote() => Padding(
    padding: const EdgeInsets.only(bottom: 12),
    child: Row(
      children: [
        Icon(
          badge.connected ? Icons.phone_android : Icons.cloud_off_outlined,
          size: 16,
        ),
        const SizedBox(width: 6),
        Text(
          badge.connected ? 'Local draft' : 'Offline draft',
          style: Theme.of(context).textTheme.labelMedium,
        ),
        const Spacer(),
        IconButton(
          tooltip: 'Draft help',
          icon: const Icon(Icons.help_outline, size: 20),
          onPressed: () => showDialog<void>(
            context: context,
            builder: (context) => AlertDialog(
              title: const Text('Your drafts'),
              content: Text(
                !badge.connected
                    ? 'Design and save on this phone. Connect to send your design to the badge.'
                    : !badge.capabilities.hasStudio
                    ? 'Update badge firmware to send Studio designs.'
                    : 'Drafts stay on this phone. Send a design to save and display it on the badge.',
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.pop(context),
                  child: const Text('Done'),
                ),
              ],
            ),
          ),
        ),
      ],
    ),
  );
  Widget button(
    String label,
    IconData icon,
    VoidCallback? tap, {
    bool primary = true,
  }) => primary
      ? FilledButton.icon(onPressed: tap, icon: Icon(icon), label: Text(label))
      : FilledButton.tonalIcon(
          onPressed: tap,
          icon: Icon(icon),
          label: Text(label),
        );

  Widget dashboard() {
    final mode = badge.mode.clamp(0, modeLabels.length - 1);
    final colors = Theme.of(context).colorScheme;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        if (!badge.connected)
          StudioPanel(
            title: 'Ready when you are',
            child: Column(
              children: [
                Icon(Icons.badge_outlined, size: 72, color: colors.primary),
                const SizedBox(height: 16),
                button(
                  'Find badge',
                  Icons.bluetooth_searching,
                  connecting ? null : () => unawaited(findBadge()),
                ),
              ],
            ),
          ),
        if (connecting || badge.busy)
          StudioPanel(
            title: connecting ? 'Connecting' : 'Saving to badge',
            subtitle: connecting
                ? 'Enter the code on your badge in the phone pairing dialog.'
                : badge.uploadProgress < 1
                ? 'Uploading… ${(badge.uploadProgress * 100).round()}%'
                : 'Verifying the applied setting…',
            child: LinearProgressIndicator(
              value: connecting || badge.uploadProgress == 1
                  ? null
                  : badge.uploadProgress,
            ),
          ),
        if (badge.connected)
          StudioPanel(
            title: 'On your badge',
            subtitle: badge.confirmed
                ? modeLabels[mode]
                : 'Refresh to confirm the active display',
            trailing: IconButton(
              tooltip: 'Disconnect',
              onPressed: badge.busy
                  ? null
                  : () => unawaited(action(badge.disconnect)),
              icon: const Icon(Icons.bluetooth_disabled),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                if (mode == 0)
                  Center(
                    child: Padding(
                      padding: const EdgeInsets.all(20),
                      child: Column(
                        children: [
                          Icon(
                            Icons.qr_code_2,
                            size: 104,
                            color: colors.primary,
                          ),
                          const SizedBox(height: 12),
                          SelectableText(
                            badge.url,
                            textAlign: TextAlign.center,
                          ),
                        ],
                      ),
                    ),
                  )
                else if (mode == 1 &&
                    badge.capabilities.hasStudio &&
                    badge.values[BadgeField.text] != null)
                  preview(
                    (t) => TextDesign.decode(
                      badge.values[BadgeField.text]!,
                    ).render(t, fallback: badge.name),
                    animate: true,
                  )
                else if (mode >= 4)
                  Padding(
                    padding: const EdgeInsets.all(40),
                    child: Icon(
                      mode == 6
                          ? Icons.water
                          : mode == 4
                          ? Icons.graphic_eq
                          : Icons.equalizer,
                      size: 104,
                      color: colors.primary,
                    ),
                  )
                else if (mode >= 2 && badge.values[BadgeField.artwork] != null)
                  preview((t) {
                    final art = MatrixDesign.decode(
                      badge.values[BadgeField.artwork]!,
                    );
                    return art.frames[mode == 3
                        ? t ~/ art.interval % art.frames.length
                        : 0];
                  }, animate: mode == 3)
                else
                  preview((t) => TextDesign(text: badge.name).render(t)),
                const SizedBox(height: 20),
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: [
                    for (var i = 0; i < badge.capabilities.modeCount; i++)
                      ChoiceChip(
                        label: Text(modeLabels[i]),
                        selected: badge.confirmed && badge.mode == i,
                        onSelected: ready
                            ? (_) => unawaited(
                                action(() => badge.apply(BadgeField.mode, [i])),
                              )
                            : null,
                      ),
                  ],
                ),
                const SizedBox(height: 20),
                const Text('Brightness'),
                const SizedBox(height: 8),
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: [
                    for (var i = 0; i < brightnessLevels.length; i++)
                      ChoiceChip(
                        label: Text(['Low', 'Medium', 'High', 'Full'][i]),
                        selected:
                            badge.confirmed &&
                            badge.brightness == brightnessLevels[i],
                        onSelected: ready
                            ? (_) => unawaited(
                                action(
                                  () => badge.apply(BadgeField.brightness, [
                                    brightnessLevels[i],
                                  ]),
                                ),
                              )
                            : null,
                      ),
                  ],
                ),
              ],
            ),
          ),
        if (badge.connected &&
            badge.capabilities.hasMotion &&
            badge.values[BadgeField.motion] != null)
          MotionPanel(
            value: badge.values[BadgeField.motion]!,
            status: badge.motionStatus,
            enabled: ready && !audio.recording && !audio.stopping,
            apply: (value) =>
                action(() => badge.apply(BadgeField.motion, value)),
            refresh: () => action(badge.refresh),
          ),
        StudioPanel(
          title: 'Create',
          child: Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              for (var i = 1; i < pageLabels.length; i++)
                button(
                  pageLabels[i],
                  pageIcons[i],
                  () => setState(() => page = i),
                  primary: false,
                ),
            ],
          ),
        ),
      ],
    );
  }

  Widget aboutButton() => TextButton.icon(
    icon: const Icon(Icons.info_outline),
    label: const Text('About Pixel Badge'),
    onPressed: () => showAboutDialog(
      context: context,
      applicationName: 'Pixel Badge',
      applicationVersion: '0.5.4 · Motion',
      applicationIcon: const Icon(Icons.grid_view_rounded, size: 40),
      children: const [
        Text(
          'Create on your phone. Save to your badge. Phone drafts stay in app-private storage and are deleted when you uninstall.',
        ),
      ],
    ),
  );

  Future<void> editLink({int? index}) async {
    final original = index == null ? null : draft.links[index];
    final label = TextEditingController(text: original?.label ?? ''),
        url = TextEditingController(text: original?.url ?? 'https://');
    final key = GlobalKey<FormState>();
    final result = await showDialog<SavedLink>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(index == null ? 'Add a custom link' : 'Edit link'),
        content: SizedBox(
          width: 420,
          child: Form(
            key: key,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                TextFormField(
                  controller: label,
                  maxLength: 32,
                  decoration: const InputDecoration(
                    labelText: 'Label',
                    hintText: 'My portfolio',
                  ),
                  validator: (v) => v == null || v.trim().isEmpty
                      ? 'Give this link a label.'
                      : null,
                ),
                const SizedBox(height: 12),
                TextFormField(
                  controller: url,
                  keyboardType: TextInputType.url,
                  autocorrect: false,
                  decoration: const InputDecoration(
                    labelText: 'URL',
                    hintText: 'https://EXAMPLE.COM',
                  ),
                  validator: (v) {
                    try {
                      validateValue(
                        BadgeField.url,
                        encodeAscii(normalizeUrl((v ?? '').trim())),
                      );
                      return null;
                    } catch (error) {
                      return friendly(error);
                    }
                  },
                ),
                const SizedBox(height: 12),
                const Text(
                  'The 32 × 32 QR needs a short link. Letter case in the path stays exactly as entered.',
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
              if (key.currentState!.validate()) {
                Navigator.pop(
                  context,
                  SavedLink(label.text.trim(), normalizeUrl(url.text.trim())),
                );
              }
            },
            child: const Text('Save link'),
          ),
        ],
      ),
    );
    label.dispose();
    url.dispose();
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

  Widget linksPage() {
    final presets = [
      for (var i = 0; i < configuredLinks.length; i++)
        if (configuredLinks[i].isNotEmpty)
          SavedLink(linkLabels[i], normalizeUrl(configuredLinks[i])),
    ];
    Widget linkRow(SavedLink link, {int? index}) => Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Expanded(
                child: Text(
                  link.label,
                  style: Theme.of(context).textTheme.titleMedium,
                ),
              ),
              if (badge.confirmed && badge.nfcVerified && badge.url == link.url)
                const Chip(
                  label: Text('Active'),
                  avatar: Icon(Icons.check, size: 16),
                ),
              if (index != null)
                IconButton(
                  tooltip: 'Edit ${link.label}',
                  onPressed: () => unawaited(editLink(index: index)),
                  icon: const Icon(Icons.edit_outlined),
                ),
              if (index != null)
                IconButton(
                  tooltip: 'Remove ${link.label}',
                  onPressed: () {
                    final removed = draft.links.removeAt(index);
                    editDraft();
                    ScaffoldMessenger.of(context).showSnackBar(
                      SnackBar(
                        content: Text('${removed.label} removed'),
                        action: SnackBarAction(
                          label: 'Undo',
                          onPressed: () {
                            if (draft.links.length < 32) {
                              draft.links.insert(
                                index.clamp(0, draft.links.length),
                                removed,
                              );
                              editDraft();
                            }
                          },
                        ),
                      ),
                    );
                  },
                  icon: const Icon(Icons.delete_outline),
                ),
            ],
          ),
          SelectableText(link.url),
          const SizedBox(height: 8),
          button(
            'Apply link',
            Icons.send_outlined,
            ready
                ? () => unawaited(
                    action(
                      () => badge.apply(BadgeField.url, encodeAscii(link.url)),
                    ),
                  )
                : null,
          ),
          const SizedBox(height: 12),
          const Divider(height: 1),
        ],
      ),
    );
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        offlineNote(),
        StudioPanel(
          title: 'Contact link',
          help: 'Your QR code and NFC tap always share the same destination.',
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                badge.connected
                    ? badge.confirmed && badge.nfcVerified
                          ? 'Confirmed active link'
                          : 'Last read link · not verified'
                    : 'Connect to read the active link',
              ),
              if (badge.connected) ...[
                const SizedBox(height: 8),
                SelectableText(badge.url),
              ],
              if (badge.connected && !badge.nfcVerified) ...[
                const SizedBox(height: 12),
                Text(badge.nfcMessage),
              ],
            ],
          ),
        ),
        StudioPanel(
          title: 'Your links',
          help: 'Save a short link once, then choose it whenever you need it.',
          trailing: IconButton(
            tooltip: 'Add a custom link',
            onPressed: loadingDraft ? null : () => unawaited(editLink()),
            icon: const Icon(Icons.add),
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              if (draft.links.isEmpty)
                const Padding(
                  padding: EdgeInsets.only(bottom: 16),
                  child: Text(
                    'Add your portfolio, social profile or any custom destination.',
                  ),
                ),
              for (var i = 0; i < draft.links.length; i++)
                linkRow(draft.links[i], index: i),
              button(
                'Add custom link',
                Icons.add_link,
                loadingDraft ? null : () => unawaited(editLink()),
                primary: false,
              ),
            ],
          ),
        ),
        if (presets.isNotEmpty)
          StudioPanel(
            title: 'Quick links',
            child: Column(children: presets.map(linkRow).toList()),
          ),
        StudioPanel(
          title: 'NFC settings',
          help:
              'Passive taps work from the tag’s saved link, even when the badge is off.',
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              ListTile(
                contentPadding: EdgeInsets.zero,
                leading: Icon(
                  badge.nfcVerified && badge.confirmed
                      ? Icons.verified_outlined
                      : Icons.nfc,
                ),
                title: Text(
                  !badge.connected
                      ? 'Connect to check NFC'
                      : badge.nfcBusy
                      ? 'Synchronizing'
                      : badge.nfcVerified && badge.confirmed
                      ? 'Link synchronized'
                      : badge.nfcResult == -13
                      ? 'Setup needs repair'
                      : 'Sync needed',
                ),
                subtitle: Text(
                  !badge.connected
                      ? 'Read the tag’s status before changing a contact link.'
                      : badge.nfcVerified
                      ? 'QR + NFC synchronized'
                      : badge.nfcMessage,
                ),
              ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 10,
                runSpacing: 10,
                children: [
                  button(
                    'Sync again',
                    Icons.sync,
                    studioReady && !badge.nfcBusy
                        ? () => unawaited(
                            action(() => badge.apply(BadgeField.nfcSync, [1])),
                          )
                        : null,
                  ),
                  button(
                    'Refresh status',
                    Icons.refresh,
                    badge.connected && !badge.busy
                        ? () => unawaited(action(badge.refresh))
                        : null,
                    primary: false,
                  ),
                ],
              ),
            ],
          ),
        ),
      ],
    );
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

  Widget textPage() {
    final error = textError();
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        offlineNote(),
        StudioPanel(
          title: 'Text composer',
          help:
              'Uppercase, lowercase, numbers and punctuation. Add line breaks for a deliberate layout.',
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              TextFormField(
                controller: textEditor,
                minLines: 2,
                maxLines: 5,
                maxLength: 96,
                enabled: !loadingDraft,
                decoration: InputDecoration(
                  labelText: 'Message',
                  hintText: 'Hello!\nI’m Ziad',
                  helperText: 'Leave empty to use your badge name.',
                  errorText: error,
                ),
                onChanged: (v) {
                  draft.text = draft.text.copyWith(text: v);
                  editDraft();
                },
              ),
              const SizedBox(height: 16),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  for (var i = 0; i < 3; i++)
                    ChoiceChip(
                      label: Text(['Auto fit', 'Word wrap', 'Scroll'][i]),
                      selected: draft.text.layout == i,
                      onSelected: (_) {
                        draft.text = draft.text.copyWith(layout: i);
                        editDraft();
                      },
                    ),
                ],
              ),
              const SizedBox(height: 16),
              DropdownButtonFormField<int>(
                key: ValueKey('text-scale-${draft.text.scale}'),
                isExpanded: true,
                initialValue: draft.text.scale,
                decoration: const InputDecoration(labelText: 'Text size'),
                items: [
                  for (var i = 0; i < 4; i++)
                    DropdownMenuItem(
                      value: i,
                      child: Text(
                        [
                          'Automatic',
                          'Small · 5 × 8',
                          'Medium · 10 × 16',
                          'Large · 15 × 24',
                        ][i],
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                ],
                onChanged: (v) {
                  if (v != null) {
                    draft.text = draft.text.copyWith(scale: v);
                    editDraft();
                  }
                },
              ),
              const SizedBox(height: 16),
              Wrap(
                spacing: 8,
                children: [
                  for (var i = 0; i < 3; i++)
                    ChoiceChip(
                      label: Icon(
                        [
                          Icons.format_align_left,
                          Icons.format_align_center,
                          Icons.format_align_right,
                        ][i],
                      ),
                      tooltip: ['Align left', 'Center', 'Align right'][i],
                      selected: draft.text.alignment == i,
                      onSelected: (_) {
                        draft.text = draft.text.copyWith(alignment: i);
                        editDraft();
                      },
                    ),
                ],
              ),
              const SizedBox(height: 12),
              Text(
                draft.text.layout == 2
                    ? 'Scroll pace · ${draft.text.interval} ms per pixel'
                    : 'Page duration · ${(draft.text.interval * 20 / 1000).toStringAsFixed(1)} seconds',
              ),
              Slider(
                min: 50,
                max: 500,
                divisions: 18,
                value: draft.text.interval.toDouble(),
                onChanged: (v) {
                  draft.text = draft.text.copyWith(interval: v.round());
                  editDraft();
                },
              ),
            ],
          ),
        ),
        StudioPanel(
          title: 'Live preview',
          help: 'This uses the same bitmap font and layout as Studio firmware.',
          child: Column(
            children: [
              error == null
                  ? preview(
                      (t) => draft.text.render(
                        t,
                        fallback: badge.name.isEmpty ? 'BADGE' : badge.name,
                      ),
                      animate: true,
                    )
                  : preview((_) => Uint8List(frameBytes)),
              const SizedBox(height: 20),
              Wrap(
                spacing: 10,
                runSpacing: 10,
                children: [
                  button(
                    'Send text',
                    Icons.send_outlined,
                    studioReady && error == null
                        ? () => unawaited(
                            action(() async {
                              await badge.apply(
                                BadgeField.text,
                                draft.text.encode(),
                              );
                              await badge.apply(BadgeField.mode, [1]);
                            }),
                          )
                        : null,
                  ),
                  button(
                    'Load from badge',
                    Icons.download_outlined,
                    badge.confirmed &&
                            badge.capabilities.hasStudio &&
                            !badge.busy
                        ? () => unawaited(
                            action(() => loadBadgeDraft(text: true)),
                          )
                        : null,
                    primary: false,
                  ),
                  button(
                    'Save draft',
                    Icons.save_outlined,
                    error == null && !loadingDraft
                        ? () => unawaited(saveDraft())
                        : null,
                    primary: false,
                  ),
                ],
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget studioPage() {
    final design = draft.matrix;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        offlineNote(),
        StudioPanel(
          title: 'Pixel canvas',
          help:
              '32 × 32 monochrome pixels. Draw directly or switch to Move to zoom.',
          child: Column(
            children: [
              Wrap(
                spacing: 4,
                runSpacing: 6,
                children: [
                  for (var i = 0; i < PixelTool.values.length; i++)
                    ChoiceChip(
                      label: Icon(
                        [
                          Icons.edit_outlined,
                          Icons.auto_fix_normal,
                          Icons.show_chart,
                          Icons.crop_square,
                          Icons.format_color_fill,
                        ][i],
                      ),
                      tooltip: [
                        'Pen',
                        'Eraser',
                        'Line',
                        'Rectangle',
                        'Fill',
                      ][i],
                      selected: tool == PixelTool.values[i] && !navigateCanvas,
                      onSelected: (_) => setState(() {
                        tool = PixelTool.values[i];
                        navigateCanvas = false;
                        playing = false;
                      }),
                    ),
                  ChoiceChip(
                    label: const Icon(Icons.pan_tool_outlined),
                    tooltip: 'Move and zoom',
                    selected: navigateCanvas,
                    onSelected: (v) => setState(() => navigateCanvas = v),
                  ),
                  IconButton(
                    tooltip: 'Undo',
                    onPressed: design.canUndo
                        ? () {
                            design.undo();
                            editDraft();
                          }
                        : null,
                    icon: const Icon(Icons.undo),
                  ),
                  IconButton(
                    tooltip: 'Redo',
                    onPressed: design.canRedo
                        ? () {
                            design.redo();
                            editDraft();
                          }
                        : null,
                    icon: const Icon(Icons.redo),
                  ),
                ],
              ),
              const SizedBox(height: 16),
              ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 460),
                child: AspectRatio(
                  aspectRatio: 1,
                  child: ClipRRect(
                    borderRadius: BorderRadius.circular(20),
                    child: InteractiveViewer(
                      panEnabled: navigateCanvas,
                      scaleEnabled: navigateCanvas,
                      minScale: 1,
                      maxScale: 4,
                      child: playing
                          ? ValueListenableBuilder<int>(
                              valueListenable: clock,
                              builder: (context, value, _) => MatrixPreview(
                                frame:
                                    design.frames[value ~/
                                        design.interval %
                                        design.frames.length],
                              ),
                            )
                          : MatrixEditor(
                              design: design,
                              tool: tool,
                              onChanged: editDraft,
                              enabled: !navigateCanvas && !loadingDraft,
                            ),
                    ),
                  ),
                ),
              ),
              const SizedBox(height: 14),
              Wrap(
                spacing: 4,
                runSpacing: 4,
                children: [
                  TextButton.icon(
                    onPressed: () {
                      design.clear();
                      editDraft();
                    },
                    icon: const Icon(Icons.clear),
                    label: const Text('Clear'),
                  ),
                  TextButton.icon(
                    onPressed: () {
                      design.invert();
                      editDraft();
                    },
                    icon: const Icon(Icons.contrast),
                    label: const Text('Invert'),
                  ),
                  TextButton.icon(
                    onPressed: () {
                      design.flip(horizontal: true);
                      editDraft();
                    },
                    icon: const Icon(Icons.flip),
                    label: const Text('Flip H'),
                  ),
                  TextButton.icon(
                    onPressed: () {
                      design.flip(horizontal: false);
                      editDraft();
                    },
                    icon: const Icon(Icons.flip_camera_android),
                    label: const Text('Flip V'),
                  ),
                ],
              ),
            ],
          ),
        ),
        StudioPanel(
          title: 'Animation timeline',
          subtitle:
              '${design.frames.length} of 16 frames · ${design.interval} ms per frame',
          trailing: IconButton(
            tooltip: playing ? 'Pause preview' : 'Play preview',
            onPressed: () => setState(() => playing = !playing),
            icon: Icon(
              playing ? Icons.pause_circle_outline : Icons.play_circle_outline,
            ),
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              SizedBox(
                height: 92,
                child: ListView.separated(
                  scrollDirection: Axis.horizontal,
                  itemCount: design.frames.length,
                  separatorBuilder: (_, _) => const SizedBox(width: 8),
                  itemBuilder: (context, index) => InkWell(
                    onTap: () => setState(() {
                      design.selected = index;
                      playing = false;
                    }),
                    child: Container(
                      width: 76,
                      padding: const EdgeInsets.all(3),
                      decoration: BoxDecoration(
                        borderRadius: BorderRadius.circular(12),
                        border: Border.all(
                          color: design.selected == index
                              ? Theme.of(context).colorScheme.primary
                              : Theme.of(context).colorScheme.outlineVariant,
                          width: 2,
                        ),
                      ),
                      child: Column(
                        children: [
                          Expanded(
                            child: MatrixPreview(
                              frame: design.frames[index],
                              label: 'Frame ${index + 1}',
                            ),
                          ),
                          Text('${index + 1}'),
                        ],
                      ),
                    ),
                  ),
                ),
              ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 4,
                runSpacing: 4,
                children: [
                  TextButton.icon(
                    onPressed: design.frames.length < 16
                        ? () {
                            design.add();
                            editDraft();
                          }
                        : null,
                    icon: const Icon(Icons.add),
                    label: const Text('Add frame'),
                  ),
                  TextButton.icon(
                    onPressed: design.frames.length < 16
                        ? () {
                            design.add(duplicate: true);
                            editDraft();
                          }
                        : null,
                    icon: const Icon(Icons.copy),
                    label: const Text('Duplicate'),
                  ),
                  IconButton(
                    tooltip: 'Move frame earlier',
                    onPressed: design.selected > 0
                        ? () {
                            design.reorder(-1);
                            editDraft();
                          }
                        : null,
                    icon: const Icon(Icons.chevron_left),
                  ),
                  IconButton(
                    tooltip: 'Move frame later',
                    onPressed: design.selected < design.frames.length - 1
                        ? () {
                            design.reorder(1);
                            editDraft();
                          }
                        : null,
                    icon: const Icon(Icons.chevron_right),
                  ),
                  IconButton(
                    tooltip: 'Remove frame',
                    onPressed: design.frames.length > 1
                        ? () {
                            design.remove();
                            editDraft();
                          }
                        : null,
                    icon: const Icon(Icons.delete_outline),
                  ),
                ],
              ),
              Text(
                'Playback · ${(1000 / design.interval).toStringAsFixed(1)} frames per second',
              ),
              Slider(
                min: 50,
                max: 2000,
                divisions: 39,
                value: design.interval.toDouble(),
                onChangeStart: (_) => design.checkpoint(),
                onChanged: (v) {
                  design.interval = v.round();
                  editDraft();
                },
              ),
            ],
          ),
        ),
        StudioPanel(
          title: 'Save and show',
          subtitle:
              'Sending saves this design on the badge. It keeps playing without your phone.',
          child: Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              button(
                design.frames.length > 1 ? 'Send animation' : 'Send drawing',
                Icons.send_outlined,
                studioReady
                    ? () => unawaited(
                        action(() async {
                          final bytes = design.encode();
                          await badge.apply(BadgeField.artwork, bytes);
                          await badge.apply(BadgeField.mode, [
                            bytes[1] > 1 ? 3 : 2,
                          ]);
                        }),
                      )
                    : null,
              ),
              button(
                'Save draft',
                Icons.save_outlined,
                loadingDraft ? null : () => unawaited(saveDraft()),
                primary: false,
              ),
              button(
                'Load from badge',
                Icons.download_outlined,
                badge.confirmed && badge.capabilities.hasStudio && !badge.busy
                    ? () => unawaited(action(() => loadBadgeDraft(text: false)))
                    : null,
                primary: false,
              ),
            ],
          ),
        ),
      ],
    );
  }

  @override
  Widget build(BuildContext context) {
    final wide = MediaQuery.sizeOf(context).width >= 850;
    final body = ListView(
      key: PageStorageKey('studio-page-$page'),
      padding: const EdgeInsets.all(20),
      children: [
        Row(
          children: [
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    [
                      'Your badge',
                      'Links',
                      'Text',
                      'Pixel Studio',
                      'Audio',
                    ][page],
                    key: const Key('page-heading'),
                    style: Theme.of(context).textTheme.headlineLarge,
                  ),
                ],
              ),
            ),
            if (draftMessage != null && page != 0)
              Tooltip(
                message: draftMessage!,
                child: Icon(
                  draftMessage == 'Saved on this phone'
                      ? Icons.check_circle_outline
                      : Icons.save_outlined,
                ),
              ),
          ],
        ),
        const SizedBox(height: 24),
        if (loadingDraft && page != 0)
          const Padding(
            padding: EdgeInsets.only(bottom: 16),
            child: LinearProgressIndicator(),
          ),
        if (badge.busy && page != 0)
          Padding(
            padding: const EdgeInsets.only(bottom: 16),
            child: LinearProgressIndicator(
              value: badge.uploadProgress < 1 ? badge.uploadProgress : null,
            ),
          ),
        if (draftMessage != null &&
            (draftMessage!.startsWith('Draft not saved') ||
                draftMessage!.startsWith('Your saved draft'))) ...[
          MaterialBanner(
            content: Text(draftMessage!),
            actions: [
              TextButton(
                onPressed: loadingDraft ? null : () => unawaited(saveDraft()),
                child: const Text('Save again'),
              ),
            ],
          ),
          const SizedBox(height: 16),
        ],
        AbsorbPointer(
          absorbing: loadingDraft && page != 0,
          child: [
            dashboard,
            linksPage,
            textPage,
            studioPage,
            () => AudioPage(
              audio: audio,
              selectVisualization: (mode) =>
                  action(() => badge.apply(BadgeField.mode, [mode])),
            ),
          ][page](),
        ),
        if (page == 0) aboutButton(),
        const SizedBox(height: 12),
      ],
    );
    return Scaffold(
      appBar: AppBar(
        title: const Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.grid_view_rounded),
            SizedBox(width: 10),
            Flexible(
              child: Text('Pixel Badge', overflow: TextOverflow.ellipsis),
            ),
          ],
        ),
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 16),
            child: ActionChip(
              avatar: Icon(
                badge.connected ? Icons.bluetooth_connected : Icons.bluetooth,
                size: 16,
              ),
              label: Text(
                connecting
                    ? 'Connecting…'
                    : badge.connected
                    ? 'Connected'
                    : 'Connect',
              ),
              onPressed: connecting || badge.busy
                  ? null
                  : () => unawaited(
                      badge.connected ? action(badge.refresh) : findBadge(),
                    ),
            ),
          ),
        ],
      ),
      body: SafeArea(
        child: Row(
          children: [
            if (wide)
              NavigationRail(
                selectedIndex: page,
                labelType: NavigationRailLabelType.all,
                onDestinationSelected: (v) => setState(() => page = v),
                destinations: [
                  for (var i = 0; i < pageLabels.length; i++)
                    NavigationRailDestination(
                      icon: Icon(pageIcons[i]),
                      label: Text(pageLabels[i]),
                    ),
                ],
              ),
            Expanded(
              child: Align(
                alignment: Alignment.topCenter,
                child: ConstrainedBox(
                  constraints: const BoxConstraints(maxWidth: 760),
                  child: body,
                ),
              ),
            ),
          ],
        ),
      ),
      bottomNavigationBar: wide
          ? null
          : NavigationBar(
              selectedIndex: page,
              onDestinationSelected: (v) => setState(() => page = v),
              destinations: [
                for (var i = 0; i < pageLabels.length; i++)
                  NavigationDestination(
                    icon: Icon(pageIcons[i]),
                    label: pageLabels[i],
                  ),
              ],
            ),
    );
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    scanTimer?.cancel();
    previewTimer?.cancel();
    saveTimer?.cancel();
    unawaited(scanSubscription?.cancel());
    if (scanning) unawaited(UniversalBle.stopScan());
    audio.removeListener(changed);
    audio.dispose();
    badge.removeListener(changed);
    textEditor.dispose();
    scanState.dispose();
    clock.dispose();
    if (widget.controller == null) badge.dispose();
    super.dispose();
  }
}
