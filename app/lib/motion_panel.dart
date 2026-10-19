// SPDX-License-Identifier: MIT
import 'package:flutter/material.dart';
import 'motion_model.dart';
import 'matrix_widgets.dart';
import 'protocol.dart';

class MotionPanel extends StatefulWidget {
  const MotionPanel({
    super.key,
    required this.value,
    required this.status,
    required this.enabled,
    required this.apply,
    required this.refresh,
    this.framed = true,
  });
  final bool framed;
  final List<int> value;
  final MotionStatus? status;
  final bool enabled;
  final Future<void> Function(List<int>) apply;
  final Future<void> Function() refresh;
  @override
  State<MotionPanel> createState() => _MotionPanelState();
}

class _MotionPanelState extends State<MotionPanel> {
  late MotionDesign design = MotionDesign.decode(widget.value);
  @override
  void didUpdateWidget(MotionPanel oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (!sameBytes(oldWidget.value, widget.value)) {
      design = MotionDesign.decode(widget.value);
    }
  }

  @override
  Widget build(BuildContext context) {
    final content = Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Icon(
              widget.status?.fresh == true ? Icons.sensors : Icons.sensors_off,
            ),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                widget.status?.fresh == true ? 'IMU ready' : 'Waiting for IMU',
              ),
            ),
          ],
        ),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const Text('Auto rotate'),
          value: design.automatic,
          onChanged: widget.enabled
              ? (v) => setState(() => design.automatic = v)
              : null,
        ),
        if (!design.automatic)
          Wrap(
            spacing: 6,
            runSpacing: 6,
            children: [
              for (var i = 0; i < 4; i++)
                ChoiceChip(
                  showCheckmark: false,
                  labelPadding: const EdgeInsets.symmetric(horizontal: 4),
                  padding: const EdgeInsets.symmetric(horizontal: 4),
                  label: Text('${i * 90}°'),
                  selected: design.rotation == i,
                  onSelected: widget.enabled
                      ? (_) => setState(() => design.rotation = i)
                      : null,
                ),
            ],
          ),
        const SizedBox(height: 12),
        const Text('Tilt response'),
        Slider(
          value: design.response.toDouble(),
          min: 1,
          max: 5,
          divisions: 4,
          label: '${design.response}',
          onChanged: widget.enabled
              ? (v) => setState(() => design.response = v.round())
              : null,
        ),
        const Text('Fluid amount'),
        Slider(
          value: design.particles.toDouble(),
          min: 16,
          max: 192,
          divisions: 11,
          label: '${design.particles}',
          onChanged: widget.enabled
              ? (v) => setState(() => design.particles = v.round())
              : null,
        ),
        const SizedBox(height: 12),
        Row(
          children: [
            IconButton(
              tooltip: 'Refresh motion status',
              onPressed: widget.enabled ? widget.refresh : null,
              icon: const Icon(Icons.refresh),
            ),
            const SizedBox(width: 8),
            Expanded(
              child: FilledButton.icon(
                onPressed: widget.enabled
                    ? () => widget.apply(design.encode())
                    : null,
                icon: const Icon(Icons.save_outlined),
                label: const Text('Apply motion'),
              ),
            ),
          ],
        ),
      ],
    );
    return widget.framed
        ? StudioPanel(title: 'Motion', child: content)
        : content;
  }
}
