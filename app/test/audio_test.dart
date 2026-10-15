// SPDX-License-Identifier: MIT
import 'dart:async';
import 'dart:typed_data';
import 'package:flutter/services.dart';
import 'package:flutter/material.dart';
import 'package:badge_app/audio_page.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:badge_app/audio_controller.dart';
import 'package:badge_app/protocol.dart';

Uint8List statePacket({int flags = 33, int reserved = 0}) =>
    (ByteData(125)
          ..setUint8(0, 2)
          ..setUint8(1, flags)
          ..setUint16(2, reserved, Endian.little))
        .buffer
        .asUint8List();

class AudioFake implements AudioTransport {
  Uint8List state = statePacket();
  final commands = <Uint8List>[];
  final messages = StreamController<Uint8List>.broadcast();
  final staging = <int>[];
  bool subscribed = false;
  Completer<Uint8List>? delayNextRead;
  @override
  String get audioDeviceId => 'fixture';
  @override
  Future<Uint8List> readAudioState() async {
    final delayed = delayNextRead;
    delayNextRead = null;
    return delayed == null ? Uint8List.fromList(state) : delayed.future;
  }

  @override
  Future<void> subscribeAudio() async {
    subscribed = true;
  }

  @override
  Future<void> unsubscribeAudio() async {
    subscribed = false;
  }

  @override
  Stream<Uint8List> get audioPackets => messages.stream;
  @override
  Future<void> writeAudioControl(Uint8List value) async {
    if (value[0] == 255) {
      final command = Uint8List.fromList(staging);
      commands.add(command);
      staging.clear();
      if (command[0] == 2) state[1] |= 2;
      if (command[0] == 3 || command[0] == 5) state[1] |= 8;
      if (command[0] == 4) state[1] &= ~8;
      if (command[0] == 1) state[1] |= 1;
    } else {
      expect(value[0], staging.length);
      staging.addAll(value.sublist(1));
    }
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  const channel = MethodChannel('audio-test');
  final nativeCalls = <String>[];
  Completer<void>? prepareBarrier, packetBarrier;
  setUp(() {
    nativeCalls.clear();
    prepareBarrier = packetBarrier = null;
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (call) async {
          nativeCalls.add(call.method);
          if (call.method == 'prepareRecording') await prepareBarrier?.future;
          if (call.method == 'packet') await packetBarrier?.future;
          if (call.method == 'list') return [];
          if (call.method == 'beginSession') return Uint8List(65)..[0] = 4;
          if (call.method == 'control') {
            final args = call.arguments as Map;
            expect((args['nonce'] as Uint8List).length, 16);
            // Stub the native crypto boundary; firmware PSA vector tests exercise
            // the actual authenticator. Flutter receives only authenticated wire data.
            return Uint8List.fromList([
              ...args['body'] as Uint8List,
              ...List.filled(16, 0),
            ]);
          }
          return null;
        });
  });
  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, null);
  });
  testWidgets(
    'Audio offers Record and Stop without enrollment or duration selection',
    (tester) async {
      final controller = AudioController(channel: channel);
      final link = AudioFake();
      controller.transport = link;
      controller.state = AudioState.decode(statePacket(flags: 35));
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SingleChildScrollView(
              child: AudioPage(
                audio: controller,
                selectVisualization: (_) async {},
              ),
            ),
          ),
        ),
      );
      expect(find.text('Enroll this app'), findsNothing);
      expect(find.byType(DropdownButtonFormField<int>), findsNothing);
      expect(find.text('Record'), findsOneWidget);
      controller.receiving = true;
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SingleChildScrollView(
              child: AudioPage(
                audio: controller,
                selectVisualization: (_) async {},
              ),
            ),
          ),
        ),
      );
      expect(find.text('Stop'), findsOneWidget);
      controller.receiving = false;
      controller.transport = null;
      controller.state = null;
      final detached = controller.detach();
      await tester.pump();
      await detached;
      await tester.pumpWidget(const SizedBox());
      controller.dispose();
      final closed = link.messages.close();
      await tester.pump();
      await closed;
    },
    timeout: const Timeout(Duration(seconds: 15)),
  );
  testWidgets(
    'recording player offers pause, seeking and ten-second skips',
    (tester) async {
      final calls = <MethodCall>[];
      var position = 2000, playing = true;
      final id = '1700000000000-12345678.pba';
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(channel, (call) async {
            calls.add(call);
            if (call.method == 'seekPlayback') {
              position = (call.arguments as Map)['positionMs'] as int;
            }
            if (call.method == 'pausePlayback') playing = false;
            if (call.method == 'resumePlayback') playing = true;
            if (call.method == 'list') return [];
            if ([
              'play',
              'playbackState',
              'pausePlayback',
              'resumePlayback',
              'seekPlayback',
            ].contains(call.method)) {
              return {
                'id': id,
                'positionMs': position,
                'durationMs': 30000,
                'playing': playing,
              };
            }
            return null;
          });
      final controller = AudioController(channel: channel);
      controller.recordings = [
        {'id': id, 'seconds': 30},
      ];
      await controller.play(id);
      Widget view() => MaterialApp(
        home: Scaffold(
          body: SingleChildScrollView(
            child: AnimatedBuilder(
              animation: controller,
              builder: (_, _) => AudioPage(
                audio: controller,
                selectVisualization: (_) async {},
              ),
            ),
          ),
        ),
      );
      await tester.pumpWidget(view());
      expect(find.byType(Slider), findsOneWidget);
      await tester.tap(find.byTooltip('Pause recording'));
      await tester.pump();
      expect(controller.playback.playing, false);
      await tester.tap(find.byTooltip('Forward 10 seconds'));
      await tester.pump();
      expect(controller.playback.positionMs, 12000);
      final slider = tester.widget<Slider>(find.byType(Slider));
      slider.onChanged!(23000);
      slider.onChangeEnd!(23000);
      await tester.pump();
      expect(controller.playback.positionMs, 23000);
      expect(controller.playback.playing, false);
      await controller.seek(999999);
      expect(controller.playback.positionMs, 30000);
      await controller.seek(-100);
      expect(controller.playback.positionMs, 0);
      expect(controller.busy, false);
      controller.background();
      await tester.pump();
      expect(calls.map((call) => call.method), contains('pausePlayback'));
      await tester.pumpWidget(const SizedBox());
      controller.dispose();
      await tester.pump();
    },
    timeout: const Timeout(Duration(seconds: 15)),
  );

  test('stale playback poll cannot undo a seek or pause', () async {
    final response = Completer<Map<String, Object?>>();
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (call) async {
          if (call.method == 'playbackState') return response.future;
          if (call.method == 'seekPlayback') {
            return {
              'id': 'fixture',
              'positionMs': 15000,
              'durationMs': 30000,
              'playing': false,
            };
          }
          return null;
        });
    final controller = AudioController(channel: channel);
    final poll = controller.pollPlayback();
    await controller.playbackAction('seekPlayback', {'positionMs': 15000});
    response.complete({
      'id': 'fixture',
      'positionMs': 1000,
      'durationMs': 30000,
      'playing': true,
    });
    await poll;
    expect(controller.playback.positionMs, 15000);
    expect(controller.playback.playing, false);
    controller.dispose();
  });

  test('audio state bounds and P3 capability compatibility', () {
    final state = AudioState.decode(statePacket());
    expect(state.recordingEnabled, true);
    expect(state.keyed, true);
    expect(
      () => AudioState.decode(statePacket(reserved: 1)),
      throwsFormatException,
    );
    expect(
      () => AudioState.decode(statePacket(flags: 128)),
      throwsFormatException,
    );
    expect(() => AudioState.decode(Uint8List(124)), throwsFormatException);
    expect(BadgeCapabilities.decode([2, 20, 32, 4, 6, 3, 16, 15]).modeCount, 6);
    expect(
      BadgeCapabilities.decode([2, 20, 32, 4, 4, 3, 16, 7]).hasAudio,
      false,
    );
    expect(
      () => BadgeCapabilities.decode([2, 20, 32, 4, 6, 3, 16, 7]),
      throwsFormatException,
    );
  });
  test(
    'connect prepares audio automatically without enrollment or a duration command',
    () async {
      final link = AudioFake()..state = statePacket(flags: 32);
      final controller = AudioController(channel: channel);
      await controller.attach(link);
      expect(controller.authenticated, true);
      expect(controller.message, 'Ready to record.');
      expect(
        nativeCalls,
        containsAllInOrder(['beginSession', 'deriveSession', 'control']),
      );
      expect(link.commands.map((command) => command[0]), [1, 2]);
      expect(
        nativeCalls.any((method) => method.toLowerCase().contains('enroll')),
        false,
      );
      await controller.detach();
      controller.dispose();
      await link.messages.close();
    },
  );
  test('control fragment ordering, bounded size and counter overflow', () {
    final message = Uint8List.fromList(List.generate(66, (i) => i));
    final fragments = audioFragments(message);
    expect(fragments.map((fragment) => fragment.first), [0, 19, 38, 57, 255]);
    expect(fragments.every((fragment) => fragment.length <= 20), true);
    expect(fragments.take(4).expand((fragment) => fragment.skip(1)), message);
    expect(() => audioFragments(Uint8List(97)), throwsFormatException);
    expect(() => audioControlBody(6, 1, 30), throwsFormatException);
    expect(() => audioControlBody(3, 0, 60), throwsFormatException);
    expect(() => audioControlBody(3, 0x100000000, 60), throwsFormatException);
  });
  test(
    'release gate refuses recording before subscribing or starting',
    () async {
      final link = AudioFake()..state = statePacket(flags: 1);
      final controller = AudioController(channel: channel);
      await controller.attach(link);
      expect(controller.authenticated, true);
      await controller.start();
      expect(link.commands.where((command) => command[0] == 3), isEmpty);
      expect(nativeCalls.contains('beginRecording'), false);
      expect(controller.receiving, false);
      await controller.detach();
      controller.dispose();
      await link.messages.close();
    },
  );
  test(
    'serialized controls have increasing counters; manual stop saves locally',
    () async {
      final link = AudioFake();
      final controller = AudioController(channel: channel);
      await controller.attach(link);
      await Future.wait([controller.control(7), controller.control(7)]);
      final counters = link.commands
          .where((command) => command[0] != 1)
          .map(
            (command) =>
                ByteData.sublistView(command).getUint32(1, Endian.little),
          )
          .toList();
      expect(counters, [1, 2, 3]);
      await controller.start();
      expect(controller.receiving, true);
      expect(link.subscribed, true);
      expect(nativeCalls.contains('beginRecording'), true);
      await controller.stop();
      expect(controller.receiving, false);
      expect(link.commands.last[0], 4);
      expect(nativeCalls.contains('finishRecording'), true);
      await controller.detach();
      controller.dispose();
      await link.messages.close();
    },
  );
  test('file setup completes before capture is requested', () async {
    final link = AudioFake();
    final controller = AudioController(channel: channel);
    await controller.attach(link);
    prepareBarrier = Completer<void>();
    final start = controller.start();
    await Future<void>.delayed(Duration.zero);
    expect(link.commands.where((command) => command[0] == 3), isEmpty);
    expect(controller.receiving, false);
    prepareBarrier!.complete();
    await start;
    expect(controller.receiving, true);
    expect(
      nativeCalls.indexOf('prepareRecording'),
      lessThan(nativeCalls.indexOf('beginRecording')),
    );
    await controller.detach();
    controller.dispose();
    await link.messages.close();
  });
  test(
    'bounded phone queue tolerates a burst and still stops on sustained overload',
    () async {
      final link = AudioFake();
      final controller = AudioController(channel: channel);
      await controller.attach(link);
      await controller.start();
      packetBarrier = Completer<void>();
      for (var i = 0; i < 16; i++) {
        link.messages.add(Uint8List(105));
      }
      await Future<void>.delayed(Duration.zero);
      expect(controller.receiving, true);
      packetBarrier!.complete();
      await Future<void>.delayed(Duration.zero);
      await controller.packets;
      await controller.stop();
      await controller.start();
      packetBarrier = Completer<void>();
      for (var i = 0; i < 33; i++) {
        link.messages.add(Uint8List(105));
      }
      await Future<void>.delayed(Duration.zero);
      expect(controller.receiving, false);
      packetBarrier!.complete();
      await Future<void>.delayed(Duration.zero);
      for (var i = 0; i < 20 && controller.stopping; i++) {
        await Future<void>.delayed(Duration.zero);
      }
      expect(controller.stopping, false);
      expect(controller.message, contains('Phone could not keep up'));
      await controller.detach();
      controller.dispose();
      await link.messages.close();
    },
  );
  test('background sends stop and refuses fresh capture', () async {
    final link = AudioFake();
    final controller = AudioController(channel: channel);
    await controller.attach(link);
    await controller.start();
    controller.background();
    await controller.stop();
    await Future<void>.delayed(Duration.zero);
    expect(controller.foreground, false);
    final starts = link.commands.where((command) => command[0] == 3).length;
    await controller.start();
    expect(link.commands.where((command) => command[0] == 3).length, starts);
    await controller.detach();
    controller.dispose();
    await link.messages.close();
  });
  test('a stale idle poll cannot stop a newly started recording', () async {
    final link = AudioFake();
    final controller = AudioController(channel: channel);
    await controller.attach(link);
    final idle = Uint8List.fromList(link.state);
    final delayed = Completer<Uint8List>();
    link.delayNextRead = delayed;
    final poll = controller.poll();
    await controller.start();
    delayed.complete(idle);
    await poll;
    expect(controller.receiving, true);
    expect(controller.state!.recording, true);
    expect(link.subscribed, true);
    await controller.detach();
    controller.dispose();
    await link.messages.close();
  });
  test('device failures and startup errors survive local cleanup', () async {
    final link = AudioFake();
    final controller = AudioController(channel: channel);
    await controller.attach(link);
    await controller.start();
    link.state[1] &= ~8;
    ByteData.sublistView(link.state).setInt32(8, -11, Endian.little);
    await controller.poll();
    expect(controller.receiving, false);
    expect(controller.message, contains('status -11'));
    await controller.start();
    await controller.stop(reason: 'Audio authentication failed.');
    expect(controller.message, 'Audio authentication failed.');
    await controller.detach();
    controller.dispose();
    await link.messages.close();
  });
}
