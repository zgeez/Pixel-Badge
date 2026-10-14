// SPDX-License-Identifier: MIT
import 'package:flutter/services.dart';
import 'studio_model.dart';

abstract interface class DraftStore {
  Future<StudioDraft> load();
  Future<void> save(StudioDraft draft);
}

class DeviceDraftStore implements DraftStore {
  static const channel = MethodChannel('com.ziadg.badge_app/drafts');
  @override
  Future<StudioDraft> load() async {
    final raw = await channel.invokeMethod<String>('read');
    return raw == null || raw.isEmpty ? StudioDraft() : StudioDraft.decode(raw);
  }

  @override
  Future<void> save(StudioDraft draft) =>
      channel.invokeMethod<void>('write', draft.encode());
}

class MemoryDraftStore implements DraftStore {
  String? _value;
  @override
  Future<StudioDraft> load() async =>
      _value == null ? StudioDraft() : StudioDraft.decode(_value!);
  @override
  Future<void> save(StudioDraft draft) async {
    _value = draft.encode();
  }
}
