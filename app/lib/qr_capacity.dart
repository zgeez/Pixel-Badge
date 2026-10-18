// SPDX-License-Identifier: MIT
/// Model 2 V1 is 21 modules; its four-module quiet zone makes 29 square.
/// V2 needs 33 square and cannot fit the 32-square panel without cropping.
class QrCapacity {
  const QrCapacity(this.correction, this.limit, this.alphanumeric);
  final String correction;
  final int limit;
  final bool alphanumeric;
  String get encoding => alphanumeric ? 'QR alphanumeric' : 'ASCII';
  static bool isAlphanumeric(String value) =>
      RegExp(r'^[0-9A-Z $%*+./:\-]+$').hasMatch(value);
  static QrCapacity? fit(String value, {bool extended = true}) {
    final alpha = isAlphanumeric(value);
    const levels = ['H', 'Q', 'M', 'L'];
    const alphanumeric = [10, 16, 20, 25], bytes = [7, 11, 14, 17];
    for (var i = extended ? 0 : 2; i < (extended ? 4 : 3); i++) {
      final maximum = alpha ? alphanumeric[i] : bytes[i];
      if (value.isNotEmpty && value.length <= maximum) {
        return QrCapacity(levels[i], maximum, alpha);
      }
    }
    return null;
  }
}
