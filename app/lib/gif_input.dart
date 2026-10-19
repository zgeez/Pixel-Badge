// SPDX-License-Identifier: MIT
import 'dart:typed_data';

abstract class GifInput {
  int byte();
  void close() {}
  int word() => byte() | byte() << 8;
  void skip(int count) {
    for (var i = 0; i < count; i++) {
      byte();
    }
  }
}

class MemoryGifInput extends GifInput {
  MemoryGifInput(this.bytes);
  final Uint8List bytes;
  int at = 0;
  @override
  int byte() {
    if (at == bytes.length) throw const FormatException('Incomplete GIF.');
    return bytes[at++];
  }
}
