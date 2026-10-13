# Pixel Badge launcher artwork

The mint LED badge and QR-inspired pattern match the app accent #72E2BA.
Android launcher background is #111A22. This is decorative artwork, not a
scannable code.

- badge-icon-source.png: original transparent raster, 1254 x 1254.
- badge-icon.png: 512 x 512 opaque preview/export.
- Android references mipmap/ic_badge; five legacy density PNGs plus adaptive
  foreground PNGs and an API26 adaptive drawable cover launcher masks.
- Regenerate resource sizes with scripts/app/export_icons.ps1 on Windows,
  using built-in System.Drawing. The source artwork is not bundled as a
  Flutter runtime asset.

Created 2026-10-12 with the built-in imagegen tool. Final generation prompt:

```text
Use case: logo-brand. Asset type: Android launcher icon foreground for the Smart Badge Flutter companion app. Create one polished, original, instantly recognizable app mark: a compact wearable LED identity badge, front-on, with a small integrated lanyard tab at the top. Its rounded square badge outline is thick and clean, with a simple asymmetric arrangement of large luminous square LED cells inside suggesting the three corner motifs of a QR code. Strong silhouette, generous negative space, deliberate geometric symmetry of the badge body, minimal detail that remains legible at 48 pixels. Match the existing app accent mint green #72E2BA and soft white; intended background dark navy #111A22. Flat graphic finish with very restrained LED glow, crisp edges, no perspective, no lettering, no actual scannable QR, no Bluetooth logo, no text or watermark, no mockup or frame. Deliver a square 1024x1024 PNG with a genuinely transparent background. All visible artwork including the tab and glow must fit inside the central 58% width and 58% height, centered, leaving the outer 21% completely transparent on every side so Android adaptive icon masks cannot crop the symbol. One mark only.
```
