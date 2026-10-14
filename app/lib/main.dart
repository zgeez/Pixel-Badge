// SPDX-License-Identifier: MIT
import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart'
    show LicenseRegistry, LicenseEntryWithLineBreaks;
import 'studio_app.dart';
import 'font_license.dart';
export 'studio_app.dart' show SmartBadgeApp;
export 'studio_home.dart' show BadgeHome;

void main() {
  LicenseRegistry.addLicense(() async* {
    yield const LicenseEntryWithLineBreaks([
      'Adafruit GFX bitmap font',
    ], fontLicense);
  });
  runApp(const SmartBadgeApp());
}
