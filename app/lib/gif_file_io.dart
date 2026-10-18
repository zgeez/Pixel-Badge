// SPDX-License-Identifier: MIT
import 'dart:io';
import 'dart:typed_data';
import 'gif_input.dart';

GifInput openGifFile(String path) => _FileInput(File(path).openSync());
bool isGifFile(String path) {
  final file = File(path).openSync();
  try {
    return String.fromCharCodes(file.readSync(6)).startsWith('GIF8');
  } finally {
    file.closeSync();
  }
}

class _FileInput extends GifInput {
  _FileInput(this.file);
  final RandomAccessFile file;
  final buffer = Uint8List(65536);
  int at = 0, length = 0;
  @override
  int byte() {
    if (at == length) {
      length = file.readIntoSync(buffer);
      at = 0;
      if (length == 0) throw const FormatException('Incomplete GIF.');
    }
    return buffer[at++];
  }

  @override
  void close() => file.closeSync();
}
