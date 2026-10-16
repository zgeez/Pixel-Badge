// SPDX-License-Identifier: MIT
import 'dart:typed_data';
import 'protocol.dart';

class MotionDesign {
  MotionDesign({
    this.automatic = true,
    this.right = 0,
    this.down = 2,
    this.response = 3,
    this.particles = 96,
    this.rotation = 0,
  });
  bool automatic;
  int right, down, response, particles, rotation;
  Uint8List encode() {
    final bytes = [
      1,
      automatic ? 1 : 0,
      right,
      down,
      response,
      particles,
      rotation,
      0,
    ];
    validateValue(BadgeField.motion, bytes);
    return Uint8List.fromList(bytes);
  }

  factory MotionDesign.decode(List<int> bytes) {
    validateValue(BadgeField.motion, bytes);
    return MotionDesign(
      automatic: bytes[1] == 1,
      right: bytes[2],
      down: bytes[3],
      response: bytes[4],
      particles: bytes[5],
      rotation: bytes[6],
    );
  }
}

class MotionStatus {
  const MotionStatus(
    this.flags,
    this.rotation,
    this.age,
    this.x,
    this.y,
    this.error,
  );
  final int flags, rotation, age, x, y, error;
  bool get fresh => flags & 2 != 0;
  bool get interrupt => flags & 4 != 0;
  factory MotionStatus.decode(Uint8List bytes) {
    if (bytes.length != 16 ||
        bytes[0] != 1 ||
        bytes[1] & ~7 != 0 ||
        bytes[2] > 3 ||
        bytes[3] != 0 ||
        bytes[10] != 0 ||
        bytes[11] != 0) {
      throw const FormatException('Malformed motion status.');
    }
    final data = ByteData.sublistView(bytes);
    return MotionStatus(
      bytes[1],
      bytes[2],
      data.getUint16(4, Endian.little),
      data.getInt16(6, Endian.little),
      data.getInt16(8, Endian.little),
      data.getInt32(12, Endian.little),
    );
  }
}
