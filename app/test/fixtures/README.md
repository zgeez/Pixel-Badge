# Synthetic GIF decoder fixtures

These fixtures were generated locally from simple pixel patterns using Pillow;
no third-party artwork or personal media is included.

- gif-disposal.gif: transparent16x16 patches, four delays, disposal1/3/2/1;
  global and local color tables, restore-previous and restore-background.
- gif-dictionary.gif: two seeded64x64 indexed noise frames; crosses LZW code-size
  boundaries and fills the4096-entry dictionary.
- gif-interlaced.gif: a64x64 coordinate pattern encoded in four interlace passes.
- gif-large-frame.gif: a5000x100 all-white frame for bounded sampling, not only a
  small patch inside a large logical canvas.

polish_regression_test.dart compares small-fixture RGBA and durations against
Flutter's independent native decoder. The large logical canvas fixture is built
in the test and is never passed to the native decoder.
