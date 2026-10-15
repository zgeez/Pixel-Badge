// SPDX-License-Identifier: MIT
import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'protocol.dart';

abstract interface class BadgeLink {
  Future<Uint8List> read(String uuid);
  Future<void> write(String uuid, Uint8List data);
  Stream<Uint8List> get statuses;
  Stream<bool> get connections;
  Future<void> close();
}

class BadgeController extends ChangeNotifier {
  static const nfcRequiredMessage =
      'Contact links need NFC verification. Run the NFC verifier on your computer, complete both phone checks, then refresh settings.';
  BadgeLink? _link;
  StreamSubscription<Uint8List>? _statusSub;
  StreamSubscription<bool>? _connectionSub;
  Completer<BadgeStatus>? _completion;
  int _generation = 0, _request = 0;
  bool _disposed = false;
  BadgeField? _field;
  BadgeCapabilities capabilities = BadgeCapabilities.legacy;
  double uploadProgress = 0;
  int nfcResult = 0;
  bool nfcBusy = false;
  bool connected = false,
      busy = false,
      confirmed = false,
      writable = false,
      nfcVerified = false;
  String message =
      'Hold the badge Action button, then connect and enter its displayed code.';
  Map<BadgeField, Uint8List> values = {};
  String get url => ascii.decode(values[BadgeField.url] ?? []);
  String get name => ascii.decode(values[BadgeField.name] ?? []);
  int get brightness => values[BadgeField.brightness]?.first ?? 128;
  int get mode => values[BadgeField.mode]?.first ?? 0;

  Future<void> attach(
    BadgeLink link, {
    BadgeCapabilities capabilities = BadgeCapabilities.legacy,
  }) async {
    if (busy) throw StateError('Wait for the current operation.');
    await disconnect();
    _link = link;
    this.capabilities = capabilities;
    connected = true;
    final generation = _generation;
    _connectionSub = link.connections.listen((online) {
      if (generation != _generation || online) return;
      connected = confirmed = writable = false;
      message =
          'Disconnected. Refresh after reconnecting; an accepted change may have completed.';
      if (_completion != null && !_completion!.isCompleted) {
        _completion!.completeError(
          StateError('Connection lost before confirmation.'),
        );
      }
      notifyListeners();
    });
    _statusSub = link.statuses.listen((bytes) {
      if (generation != _generation) return;
      try {
        final status = BadgeStatus.decode(bytes);
        if (status.version == capabilities.version &&
            status.id == _request &&
            status.field == _field?.wire &&
            status.phase == 2 &&
            _completion != null &&
            !_completion!.isCompleted) {
          _completion!.complete(status);
        }
      } on FormatException {
        // Invalid/stale notifications never confirm a setting; status read is the fallback.
      }
    });
    try {
      await refresh();
    } catch (_) {
      await disconnect();
      rethrow;
    }
  }

  Future<void> refresh() async {
    if (busy) throw StateError('Wait for the current operation.');
    busy = true;
    confirmed = false;
    message = 'Reading settings from the badge…';
    notifyListeners();
    try {
      await _refresh();
    } catch (_) {
      confirmed = writable = false;
      message =
          'Readback could not be confirmed. Refresh or reconnect before applying.';
      rethrow;
    } finally {
      busy = false;
      notifyListeners();
    }
  }

  Future<void> _refresh() async {
    final link = _link;
    final generation = _generation;
    if (link == null || !connected) {
      throw StateError('Connect to the badge first.');
    }
    final before = BadgeStatus.decode(await link.read(statusUuid));
    if (before.phase == 1) {
      throw StateError('Badge is still applying a setting.');
    }
    final next = <BadgeField, Uint8List>{};
    for (final field in capabilities.settings) {
      final value = await link.read(field.uuid);
      validateValue(field, value);
      next[field] = value;
    }
    final after = BadgeStatus.decode(await link.read(statusUuid));
    if (before.version != capabilities.version ||
        after.version != capabilities.version) {
      throw const FormatException(
        'Badge protocol changed. Reconnect to refresh its capabilities.',
      );
    }
    if (!connected ||
        generation != _generation ||
        after.phase == 1 ||
        before.id != after.id ||
        before.phase != after.phase) {
      throw StateError('Badge changed during readback. Refresh again.');
    }
    values = next;
    _request = after.id;
    writable = after.writable;
    nfcVerified = after.nfcVerified;
    if (capabilities.hasStudio) {
      final state = await link.read(BadgeField.nfcSync.uuid);
      if (state.length != 7 || state[0] != 1 || state[1] > 1 || state[2] > 1) {
        throw const FormatException('Malformed NFC status.');
      }
      nfcVerified = state[1] == 1;
      nfcBusy = state[2] == 1;
      nfcResult = ByteData.sublistView(state).getInt32(3, Endian.little);
      if (!connected || generation != _generation) {
        throw StateError('Connection lost during readback.');
      }
    }
    confirmed = true;
    message = nfcVerified
        ? 'Settings read from the badge. NFC link is verified.'
        : 'Settings read from the badge. NFC needs verification/provisioning.';
    notifyListeners();
  }

  Future<BadgeStatus> _wait(BadgeLink link, int generation) async {
    final deadline = DateTime.now().add(const Duration(seconds: 8));
    while (DateTime.now().isBefore(deadline)) {
      if (!connected || generation != _generation) {
        throw StateError('Connection lost.');
      }
      final notification = await Future.any<BadgeStatus?>([
        _completion!.future,
        Future<BadgeStatus?>.delayed(
          const Duration(milliseconds: 200),
          () => null,
        ),
      ]);
      if (notification != null) return notification;
      final status = BadgeStatus.decode(await link.read(statusUuid));
      if (status.version == capabilities.version &&
          status.id == _request &&
          status.field == _field?.wire &&
          status.phase == 2) {
        return status;
      }
    }
    throw TimeoutException(
      'Apply could not be confirmed. Refresh before trying again.',
    );
  }

  Future<void> apply(BadgeField field, List<int> value) async {
    if (busy || !connected || !confirmed || !writable || _link == null) {
      throw StateError('Badge is not ready to apply settings.');
    }
    if (field == BadgeField.url && !nfcVerified) {
      message = nfcRequiredMessage;
      notifyListeners();
      throw StateError(nfcRequiredMessage);
    }
    if (!capabilities.settings.contains(field) &&
        !(capabilities.hasStudio && field == BadgeField.nfcSync)) {
      throw StateError('Update badge firmware to use this feature.');
    }
    if (field == BadgeField.mode &&
        value.isNotEmpty &&
        value[0] >= capabilities.modeCount) {
      throw StateError('Update badge firmware to use Studio modes.');
    }
    final packets = stagePackets(field, value);
    final link = _link!;
    final generation = _generation;
    busy = true;
    uploadProgress = 0;
    confirmed = false;
    message = 'Applying… waiting for badge confirmation.';
    notifyListeners();
    try {
      final status = BadgeStatus.decode(await link.read(statusUuid));
      if (status.version != capabilities.version) {
        throw const FormatException(
          'Badge protocol changed. Reconnect before applying.',
        );
      }
      if (status.phase == 1) throw StateError('Badge is busy.');
      if (field == BadgeField.url && !status.nfcVerified) {
        nfcVerified = false;
        throw StateError(nfcRequiredMessage);
      }
      _request = status.id + 1;
      _field = field;
      _completion = Completer<BadgeStatus>();
      // Attach an error listener immediately, including while staging/write awaits.
      unawaited(
        _completion!.future.then<void>(
          (_) {},
          onError: (Object error, StackTrace stack) {},
        ),
      );
      for (var index = 0; index < packets.length; ++index) {
        if (!connected || generation != _generation) {
          throw StateError('Connection lost.');
        }
        await link.write(field.uuid, packets[index]);
        uploadProgress = (index + 1) / packets.length;
        notifyListeners();
      }
      if (!connected || generation != _generation) {
        throw StateError('Connection lost before Apply.');
      }
      await link.write(
        applyUuid,
        applyPacket(_request, field, version: capabilities.version),
      );
      final completion = await _wait(link, generation);
      if (completion.result != 0) {
        if ((field == BadgeField.url || field == BadgeField.nfcSync) &&
            completion.result == -13) {
          nfcVerified = false;
          throw StateError(nfcRequiredMessage);
        }
        throw StateError(
          'Badge rejected the change (${completion.result}). Refresh its current settings.',
        );
      }
      await _refresh();
      if ((field != BadgeField.nfcSync && !sameBytes(values[field]!, value)) ||
          (field == BadgeField.nfcSync && !nfcVerified) ||
          (field == BadgeField.url && !nfcVerified)) {
        confirmed = false;
        throw StateError('Readback does not confirm the requested setting.');
      }
      message = 'Applied and confirmed by the badge.';
    } catch (error) {
      confirmed = false;
      message = '$error';
      rethrow;
    } finally {
      busy = false;
      _completion = null;
      _field = null;
      notifyListeners();
    }
  }

  Future<void> disconnect() async {
    ++_generation;
    connected = confirmed = writable = false;
    await _statusSub?.cancel();
    await _connectionSub?.cancel();
    final previous = _link;
    _link = null;
    if (previous != null) await previous.close();
    notifyListeners();
  }

  @override
  void notifyListeners() {
    if (!_disposed) super.notifyListeners();
  }

  @override
  void dispose() {
    _disposed = true;
    ++_generation;
    connected = false;
    unawaited(_statusSub?.cancel());
    unawaited(_connectionSub?.cancel());
    unawaited(_link?.close());
    super.dispose();
  }
}
