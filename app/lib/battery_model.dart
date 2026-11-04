// SPDX-License-Identifier: MIT
import 'dart:typed_data';
import 'package:flutter/material.dart';

/// Coarse battery estimate read from the badge's Battery characteristic:
/// `[1, level, flags, 0, mv:u16le]`. The badge estimates the level from the
/// pack voltage near rest, so the app shows words, not a percentage.
class BatteryStatus {
  const BatteryStatus(this.level, this.flags, this.millivolts);
  static const unknown = 0, empty = 1, low = 2, half = 3, high = 4, full = 5;
  final int level, flags, millivolts;
  bool get known => level != unknown;
  bool get usbPower => flags & 1 != 0;
  bool get charging => flags & 2 != 0;
  bool get shed => flags & 4 != 0;

  factory BatteryStatus.decode(Uint8List bytes) {
    if (bytes.length != 6 ||
        bytes[0] != 1 ||
        bytes[1] > full ||
        bytes[2] & ~7 != 0 ||
        bytes[3] != 0) {
      throw const FormatException('Malformed battery status.');
    }
    return BatteryStatus(
      bytes[1],
      bytes[2],
      ByteData.sublistView(bytes).getUint16(4, Endian.little),
    );
  }

  /// Short label for the app bar. The voltage reads full before the charger
  /// finishes topping up, so a full band while charging is "Nearly full".
  String get label => switch (level) {
    full when charging => 'Nearly full',
    full => 'Full',
    high => 'Mostly full',
    half => 'About half',
    low => 'Running low',
    empty => 'Almost empty',
    _ => 'Checking',
  };

  /// One sentence about the charge left.
  String get summary => switch (level) {
    full when charging => 'Nearly full; the charger is topping it up.',
    full => 'The battery is full.',
    high => 'Plenty of charge left.',
    half => 'About half the charge is left.',
    low => 'Running low. Charge the badge soon.',
    empty =>
      'Almost empty. Charge the badge now; the display may dim or turn off.',
    _ => 'The badge is still measuring its battery.',
  };

  /// One sentence about the power source.
  String get source => charging
      ? 'Charging over USB.'
      : usbPower && level == full
      ? 'Charged and on USB power.'
      : usbPower
      ? 'On USB power, not charging.'
      : 'Running on battery.';

  /// The fill level. While charging, [BatteryIcon] draws a bolt over it; a
  /// "nearly full" charge shows one bar short of full.
  IconData get icon => switch (level) {
    full when charging => Icons.battery_6_bar,
    full => Icons.battery_full,
    high => Icons.battery_5_bar,
    half => Icons.battery_3_bar,
    low => Icons.battery_2_bar,
    empty when charging => Icons.battery_1_bar,
    empty => Icons.battery_alert,
    _ => charging ? Icons.battery_charging_full : Icons.battery_unknown,
  };
}

/// The level icon, with a charging bolt inside while charging. Flutter's
/// icon set has only a full charging battery, so the bolt is drawn on top in
/// the background colour, outlined in the icon colour, which keeps it visible
/// over both the filled and the empty part of the battery.
class BatteryIcon extends StatelessWidget {
  const BatteryIcon(this.status, {super.key, this.size = 24, this.color});
  final BatteryStatus status;
  final double size;
  final Color? color;

  @override
  Widget build(BuildContext context) {
    final battery = Icon(status.icon, size: size, color: color);
    if (!status.charging || !status.known) return battery;
    final ink = color ?? IconTheme.of(context).color ?? Colors.black;
    return Stack(
      alignment: Alignment.center,
      children: [
        battery,
        Padding(
          padding: EdgeInsets.only(top: size * 0.08),
          child: Icon(
            Icons.bolt,
            size: size * 0.55,
            color: Theme.of(context).colorScheme.surface,
            shadows: [
              for (final offset in const [
                Offset(0.8, 0),
                Offset(-0.8, 0),
                Offset(0, 0.8),
                Offset(0, -0.8),
              ])
                Shadow(color: ink, offset: offset),
            ],
          ),
        ),
      ],
    );
  }
}

/// App bar battery label; tapping it explains the estimate. [compact] shows
/// the icon only, for narrow app bars.
class BatteryIndicator extends StatelessWidget {
  const BatteryIndicator(this.status, {super.key, this.compact = false});
  final BatteryStatus status;
  final bool compact;

  @override
  Widget build(BuildContext context) {
    final alert = status.level <= BatteryStatus.low && !status.charging;
    final color = alert ? Theme.of(context).colorScheme.error : null;
    final label = status.label;
    void details() => showDialog<void>(
      context: context,
      builder: (c) => AlertDialog(
        icon: BatteryIcon(status, color: color),
        title: Text('Battery: ${status.label}'),
        content: Text(
          '${status.summary} ${status.source}\n\n'
          'Estimated from the battery voltage '
          '(${(status.millivolts / 1000).toStringAsFixed(2)} V), '
          'so treat it as a rough guide.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(c),
            child: const Text('Done'),
          ),
        ],
      ),
    );
    if (compact) {
      return IconButton(
        tooltip: 'Battery: $label',
        onPressed: details,
        icon: BatteryIcon(status, color: color),
      );
    }
    return TextButton.icon(
      onPressed: details,
      icon: BatteryIcon(status, size: 20, color: color),
      label: Text(label, style: TextStyle(color: color)),
    );
  }
}
