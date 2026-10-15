// SPDX-License-Identifier: MIT
import 'dart:async';
import 'package:flutter/material.dart';
import 'audio_controller.dart';

String audioTime(int ms) {
  final seconds = ms ~/ 1000, hours = ms ~/ 3600000;
  final minutes = (seconds ~/ 60) % 60;
  return '${hours > 0 ? '$hours:' : ''}${hours > 0 ? minutes.toString().padLeft(2, '0') : seconds ~/ 60}:${(seconds % 60).toString().padLeft(2, '0')}';
}

class AudioPage extends StatelessWidget {
  const AudioPage({
    super.key,
    required this.audio,
    required this.selectVisualization,
  });
  final AudioController audio;
  final Future<void> Function(int) selectVisualization;
  void help(BuildContext context) => showDialog<void>(
    context: context,
    builder: (context) => AlertDialog(
      title: const Text('Audio'),
      scrollable: true,
      content: const Text(
        'Level and spectrum run on the badge. Recordings are encrypted on this phone and excluded from backups. Export saves a WAV where you choose.\n\nKeep the app open while recording and keep REC visible on the badge. Leaving the app stops recording; Bluetooth stays connected. Hold Mode on the badge to request recording, or press either button to stop.\n\nRecording ends when you stop, the connection ends, storage runs low, or the WAV file limit is reached (about 37 hours).',
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text('Done'),
        ),
      ],
    ),
  );
  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    final enabled =
        audio.authenticated &&
        !audio.busy &&
        audio.state?.recordingEnabled == true;
    final status = !audio.android
        ? 'Recording requires Android'
        : !audio.available
        ? 'Connect your badge to record'
        : audio.state?.recordingEnabled != true
        ? 'Recording unavailable in this firmware'
        : audio.recording
        ? 'Keep the app open'
        : audio.message;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Expanded(
              child: Text(
                'Visualizers',
                style: Theme.of(context).textTheme.titleLarge,
              ),
            ),
            IconButton(
              onPressed: () => help(context),
              icon: const Icon(Icons.help_outline),
              tooltip: 'Audio help',
            ),
          ],
        ),
        const SizedBox(height: 8),
        Row(
          children: [
            for (final item in [
              (4, Icons.graphic_eq, 'Level meter'),
              (5, Icons.equalizer, 'Spectrum'),
            ]) ...[
              if (item.$1 == 5) const SizedBox(width: 12),
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: audio.available && !audio.recording
                      ? () => unawaited(selectVisualization(item.$1))
                      : null,
                  icon: Icon(item.$2),
                  label: Text(item.$3),
                ),
              ),
            ],
          ],
        ),
        const SizedBox(height: 20),
        Card(
          child: Padding(
            padding: const EdgeInsets.all(20),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Icon(
                  audio.recording ? Icons.fiber_manual_record : Icons.mic_none,
                  size: 40,
                  color: audio.recording ? colors.error : colors.primary,
                ),
                const SizedBox(height: 8),
                Text(
                  audio.recording
                      ? audioTime(audio.recordingWatch.elapsedMilliseconds)
                      : 'Voice recorder',
                  textAlign: TextAlign.center,
                  style: Theme.of(context).textTheme.headlineSmall,
                ),
                const SizedBox(height: 8),
                Text(
                  status,
                  textAlign: TextAlign.center,
                  style: Theme.of(context).textTheme.bodySmall,
                ),
                const SizedBox(height: 16),
                FilledButton.icon(
                  onPressed: audio.recording
                      ? () => unawaited(audio.stop())
                      : enabled
                      ? () => unawaited(audio.start())
                      : null,
                  icon: Icon(audio.recording ? Icons.stop : Icons.mic),
                  label: Text(audio.recording ? 'Stop' : 'Record'),
                ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 20),
        Row(
          children: [
            Expanded(
              child: Text(
                'Recordings',
                style: Theme.of(context).textTheme.titleLarge,
              ),
            ),
            const Tooltip(
              message: 'Encrypted on this phone',
              child: Icon(Icons.lock_outline, size: 18),
            ),
            IconButton(
              onPressed: audio.busy
                  ? null
                  : () => unawaited(
                      audio.refreshLibrary().catchError((Object _) {}),
                    ),
              icon: const Icon(Icons.refresh),
              tooltip: 'Refresh recordings',
            ),
          ],
        ),
        if (audio.recordings.isEmpty)
          const Padding(
            padding: EdgeInsets.symmetric(vertical: 32),
            child: Center(
              child: Column(
                children: [
                  Icon(Icons.multitrack_audio, size: 40),
                  SizedBox(height: 12),
                  Text('No recordings yet.'),
                ],
              ),
            ),
          ),
        for (final recording in audio.recordings)
          recordingTile(context, recording),
      ],
    );
  }

  Widget recordingTile(BuildContext context, Map<Object?, Object?> recording) {
    final id = recording['id'] as String;
    final date = DateTime.fromMillisecondsSinceEpoch(
      int.tryParse(id.split('-').first) ?? 0,
    ).toLocal();
    final selected = audio.playback.id == id;
    final enabled = !audio.busy && !audio.recording;
    final seconds = ((recording['seconds'] as num) * 1000).round();
    return Card(
      child: Column(
        children: [
          ListTile(
            leading: IconButton.filledTonal(
              tooltip: selected && audio.playback.playing
                  ? 'Pause recording'
                  : 'Play recording',
              onPressed: !enabled
                  ? null
                  : () => unawaited(
                      selected ? audio.togglePlayback() : audio.play(id),
                    ),
              icon: Icon(
                selected && audio.playback.playing
                    ? Icons.pause
                    : Icons.play_arrow,
              ),
            ),
            title: Text(
              '${date.day}/${date.month}/${date.year} · ${TimeOfDay.fromDateTime(date).format(context)}',
            ),
            subtitle: Text(
              selected && audio.playback.error != null
                  ? audio.playback.error!
                  : audioTime(seconds),
            ),
            trailing: PopupMenuButton<String>(
              enabled: enabled,
              tooltip: 'Recording options',
              onSelected: (method) async {
                if (method == 'delete') {
                  final yes = await showDialog<bool>(
                    context: context,
                    builder: (context) => AlertDialog(
                      title: const Text('Delete recording?'),
                      actions: [
                        TextButton(
                          onPressed: () => Navigator.pop(context, false),
                          child: const Text('Cancel'),
                        ),
                        FilledButton(
                          onPressed: () => Navigator.pop(context, true),
                          child: const Text('Delete'),
                        ),
                      ],
                    ),
                  );
                  if (yes != true) return;
                }
                await audio.libraryAction(method, id);
              },
              itemBuilder: (_) => const [
                PopupMenuItem(value: 'export', child: Text('Export WAV')),
                PopupMenuItem(value: 'delete', child: Text('Delete')),
              ],
            ),
          ),
          if (selected) RecordingControls(audio: audio),
        ],
      ),
    );
  }
}

class RecordingControls extends StatefulWidget {
  const RecordingControls({super.key, required this.audio});
  final AudioController audio;
  @override
  State<RecordingControls> createState() => _RecordingControlsState();
}

class _RecordingControlsState extends State<RecordingControls> {
  double? scrub;
  @override
  Widget build(BuildContext context) {
    final player = widget.audio.playback;
    final duration = player.durationMs.toDouble();
    final position = (scrub ?? player.positionMs.toDouble()).clamp(
      0.0,
      duration,
    );
    return Padding(
      padding: const EdgeInsets.fromLTRB(12, 0, 12, 12),
      child: Column(
        children: [
          Slider(
            label: audioTime(position.round()),
            value: position,
            max: duration > 0 ? duration : 1,
            semanticFormatterCallback: (value) => audioTime(value.round()),
            onChanged: duration > 0
                ? (value) => setState(() => scrub = value)
                : null,
            onChangeEnd: (value) async {
              await widget.audio.seek(value.round());
              if (mounted) setState(() => scrub = null);
            },
          ),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 12),
            child: Row(
              children: [
                Text(audioTime(position.round())),
                const Spacer(),
                Text(audioTime(player.durationMs)),
              ],
            ),
          ),
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              IconButton(
                tooltip: 'Back 10 seconds',
                icon: const Icon(Icons.replay_10),
                onPressed: () =>
                    unawaited(widget.audio.seek(player.positionMs - 10000)),
              ),
              IconButton(
                tooltip: 'Stop playback',
                icon: const Icon(Icons.stop),
                onPressed: () => unawaited(widget.audio.stopPlayback()),
              ),
              IconButton(
                tooltip: 'Forward 10 seconds',
                icon: const Icon(Icons.forward_10),
                onPressed: () =>
                    unawaited(widget.audio.seek(player.positionMs + 10000)),
              ),
            ],
          ),
        ],
      ),
    );
  }
}
