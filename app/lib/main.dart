// SPDX-License-Identifier: MIT
import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart' show kDebugMode;
import 'package:flutter/material.dart';
import 'package:universal_ble/universal_ble.dart';
import 'badge_controller.dart';
import 'ble_link.dart';
import 'protocol.dart';

void main() => runApp(const SmartBadgeApp());

class SmartBadgeApp extends StatelessWidget {
  const SmartBadgeApp({super.key});
  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'Pixel Badge',
    debugShowCheckedModeBanner: false,
    theme: ThemeData(
      useMaterial3: true,
      brightness: Brightness.dark,
      colorSchemeSeed: const Color(0xff72e2ba),
      scaffoldBackgroundColor: const Color(0xff111a22),
    ),
    home: const BadgeHome(),
  );
}

class BadgeHome extends StatefulWidget {
  const BadgeHome({super.key, this.controller, this.scanResults});
  final BadgeController? controller;
  final Stream<BleDevice>? scanResults;
  @override
  State<BadgeHome> createState() => _BadgeHomeState();
}

class _BadgeHomeState extends State<BadgeHome> with WidgetsBindingObserver {
  late final badge = widget.controller ?? BadgeController();
  final devices = <String, BleDevice>{};
  StreamSubscription<BleDevice>? scanSubscription;
  Timer? scanTimer;
  bool scanning = false, connecting = false;
  String? selectedLink;
  final nameEditor = TextEditingController();
  String? readbackName;
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    syncName();
    badge.addListener(changed);
    scanSubscription = (widget.scanResults ?? UniversalBle.scanStream).listen((
      device,
    ) {
      if (mounted && scanning) {
        setState(() => devices[device.deviceId] = device);
      }
    });
  }

  void changed() {
    if (mounted) {
      syncName();
      setState(() {});
    }
  }

  void syncName() {
    if (readbackName != badge.name) {
      readbackName = badge.name;
      nameEditor.text = badge.name;
    }
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.paused) {
      unawaited(stopScan());
      if (badge.connected) unawaited(badge.disconnect());
    }
  }

  Future<void> action(Future<void> Function() work) async {
    try {
      await work();
    } catch (error) {
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('$error')));
      }
    }
  }

  Future<void> stopScan() async {
    scanTimer?.cancel();
    if (!scanning) return;
    if (mounted) setState(() => scanning = false);
    await UniversalBle.stopScan();
  }

  Future<void> scan() async {
    await UniversalBle.requestPermissions(withAndroidFineLocation: false);
    if (await UniversalBle.getBluetoothAvailabilityState() !=
        AvailabilityState.poweredOn) {
      throw StateError('Turn on Bluetooth to find your badge.');
    }
    devices.clear();
    if (mounted) setState(() => scanning = true);
    try {
      await UniversalBle.startScan(
        scanFilter: ScanFilter(withServices: [serviceUuid]),
      );
      scanTimer = Timer(
        const Duration(seconds: 10),
        () => unawaited(action(stopScan)),
      );
    } catch (_) {
      if (mounted) setState(() => scanning = false);
      rethrow;
    }
  }

  Future<void> connect(BleDevice device) async {
    setState(() => connecting = true);
    try {
      await stopScan();
      final link = await UniversalBadgeLink.connect(device);
      if (!mounted) {
        await link.close();
        return;
      }
      await badge.attach(link);
      selectedLink = null;
    } finally {
      if (mounted) setState(() => connecting = false);
    }
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    scanTimer?.cancel();
    unawaited(scanSubscription?.cancel());
    if (scanning) unawaited(UniversalBle.stopScan());
    badge.removeListener(changed);
    nameEditor.dispose();
    if (widget.controller == null) badge.dispose();
    super.dispose();
  }

  Widget section(String title, Widget child) => Card(
    margin: const EdgeInsets.only(bottom: 16),
    child: Padding(
      padding: const EdgeInsets.all(20),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(title, style: Theme.of(context).textTheme.titleLarge),
          const SizedBox(height: 16),
          child,
        ],
      ),
    ),
  );
  @override
  Widget build(BuildContext context) {
    final ready =
        badge.connected &&
        badge.confirmed &&
        badge.writable &&
        !badge.busy &&
        !connecting;
    final links = configuredLinks.map(normalizeUrl).toList();
    return Scaffold(
      appBar: AppBar(
        title: const Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Pixel Badge'),
            if (kDebugMode)
              Text('BLE diagnostic D2', style: TextStyle(fontSize: 12)),
          ],
        ),
        actions: [
          if (badge.connected)
            IconButton(
              tooltip: 'Disconnect',
              icon: const Icon(Icons.bluetooth_disabled),
              onPressed: badge.busy ? null : () => action(badge.disconnect),
            ),
        ],
      ),
      body: Center(
        child: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 620),
          child: ListView(
            padding: const EdgeInsets.all(20),
            children: [
              Text(
                'Make it yours.',
                style: Theme.of(context).textTheme.headlineLarge,
              ),
              const SizedBox(height: 8),
              const Text('Your identity and links, confirmed on the badge.'),
              const SizedBox(height: 24),
              section(
                badge.connected ? 'Connected' : 'Connect your badge',
                Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      connecting
                          ? 'Connecting… enter the six-digit code shown on the badge in the phone pairing dialog.'
                          : badge.message,
                    ),
                    const SizedBox(height: 14),
                    if (badge.busy || connecting)
                      const LinearProgressIndicator(),
                    if (!badge.connected) ...[
                      const Text(
                        'Hold Action (Button 2) for 1 second after the QR appears, then release. Tap Find badge and connect within 60 seconds. Only then will the badge show the six-digit code in two rows.',
                      ),
                      const SizedBox(height: 12),
                      FilledButton.icon(
                        onPressed: connecting
                            ? null
                            : () => action(scanning ? stopScan : scan),
                        icon: Icon(
                          scanning ? Icons.stop : Icons.bluetooth_searching,
                        ),
                        label: Text(scanning ? 'Stop scanning' : 'Find badge'),
                      ),
                      for (final device in devices.values)
                        ListTile(
                          contentPadding: EdgeInsets.zero,
                          leading: const Icon(Icons.badge_outlined),
                          title: Text(
                            device.name?.isNotEmpty == true
                                ? device.name!
                                : 'Smart LED Badge',
                          ),
                          subtitle: Text('Signal ${device.rssi ?? '—'} dBm'),
                          trailing: const Icon(Icons.chevron_right),
                          onTap: connecting
                              ? null
                              : () => action(() => connect(device)),
                        ),
                    ] else
                      TextButton.icon(
                        onPressed: badge.busy
                            ? null
                            : () => action(badge.refresh),
                        icon: const Icon(Icons.refresh),
                        label: const Text('Refresh settings'),
                      ),
                  ],
                ),
              ),
              if (badge.connected) ...[
                section(
                  'Contact link',
                  Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        badge.confirmed && badge.nfcVerified
                            ? 'Active link'
                            : 'Last read link · unconfirmed',
                      ),
                      const SizedBox(height: 6),
                      SelectableText(badge.url),
                      if (badge.confirmed && !badge.nfcVerified)
                        const Padding(
                          padding: EdgeInsets.only(top: 12),
                          child: Text(BadgeController.nfcRequiredMessage),
                        ),
                      const SizedBox(height: 16),
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: [
                          for (var i = 0; i < links.length; ++i)
                            ChoiceChip(
                              label: Text(linkLabels[i]),
                              selected: selectedLink == links[i],
                              onSelected: ready && links[i].isNotEmpty
                                  ? (_) =>
                                        setState(() => selectedLink = links[i])
                                  : null,
                            ),
                        ],
                      ),
                      if (links.every((link) => link.isEmpty))
                        const Padding(
                          padding: EdgeInsets.only(top: 12),
                          child: Text(
                            'No link presets were included in this app build.',
                          ),
                        ),
                      const SizedBox(height: 16),
                      FilledButton(
                        onPressed:
                            ready && badge.nfcVerified && selectedLink != null
                            ? () => action(
                                () => badge.apply(
                                  BadgeField.url,
                                  ascii.encode(selectedLink!),
                                ),
                              )
                            : null,
                        child: const Text('Apply link'),
                      ),
                    ],
                  ),
                ),
                section(
                  'Identity',
                  Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      TextFormField(
                        controller: nameEditor,
                        enabled: ready,
                        maxLength: 32,
                        decoration: const InputDecoration(
                          labelText: 'Display name',
                        ),
                      ),
                      const SizedBox(height: 8),
                      FilledButton.tonal(
                        onPressed: ready
                            ? () => action(
                                () => badge.apply(
                                  BadgeField.name,
                                  ascii.encode(nameEditor.text),
                                ),
                              )
                            : null,
                        child: const Text('Apply name'),
                      ),
                    ],
                  ),
                ),
                section(
                  'Display',
                  Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const Text('Brightness'),
                      const SizedBox(height: 8),
                      Wrap(
                        spacing: 8,
                        children: [
                          for (var i = 0; i < brightnessLevels.length; ++i)
                            ChoiceChip(
                              label: Text(['Low', 'Medium', 'High', 'Full'][i]),
                              selected:
                                  badge.confirmed &&
                                  badge.brightness == brightnessLevels[i],
                              onSelected: ready
                                  ? (_) => action(
                                      () => badge.apply(BadgeField.brightness, [
                                        brightnessLevels[i],
                                      ]),
                                    )
                                  : null,
                            ),
                        ],
                      ),
                      const SizedBox(height: 20),
                      const Text('Mode'),
                      const SizedBox(height: 8),
                      SegmentedButton<int>(
                        segments: const [
                          ButtonSegment(
                            value: 0,
                            icon: Icon(Icons.qr_code),
                            label: Text('QR'),
                          ),
                          ButtonSegment(
                            value: 1,
                            icon: Icon(Icons.person_outline),
                            label: Text('Name'),
                          ),
                        ],
                        selected: {badge.mode},
                        onSelectionChanged: ready
                            ? (value) => action(
                                () => badge.apply(BadgeField.mode, [
                                  value.single,
                                ]),
                              )
                            : null,
                      ),
                    ],
                  ),
                ),
              ],
              const Padding(
                padding: EdgeInsets.only(bottom: 16),
                child: Text(
                  'Settings stay on your badge after disconnecting. NFC sharing works with the badge powered off.',
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
