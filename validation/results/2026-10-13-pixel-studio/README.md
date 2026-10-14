# Pixel Badge Studio — software validation

Date: 2026-10-13. Target: existing nRF5340 DK/HUB75 prototype, NCS3.4.1,
Flutter3.38.5. Work is isolated on `app/pixel-studio`, based on merged PR22
(`51b2d7f`). App0.2.0/build3 is installed on the Nothing phone and matching
normal firmware is flashed to both DK cores. The first owner-reported upload
is recorded below; broader physical acceptance remains pending. The separate
E73 hardware-module change and
single-core port are outside this implementation.

## Implemented behavior

- System light/dark Material3 interface with Badge, Links, Text and Studio.
- Atomic offline Android draft storage, custom short links, confirmed shared
  QR/NFC destination, verification status and guarded resync.
- Shared case-sensitive bitmap font, auto fit, wrapping/pages, scrolling,
  alignment and scale; up to 96 printable ASCII characters plus LF.
- Monochrome pixel editor, reversible tools, 1–16 frames and elapsed-time
  animation playback persisted on the badge.
- Authenticated BLE v2 content staging and paged readback at default MTU;
  v1 firmware compatibility for the app's basic controls.

## Software results

- Flutter analyzer: no issues. All **36** protocol, controller, model and
  widget tests pass. Coverage includes default-MTU maximum artwork, final-chunk
  and mid-upload disconnects, no automatic replay, v1 feature gating, malformed
  NFC state, offline links/drafts, undo and vertical canvas gestures.
- Native firmware: **16 configurations / 92 cases** pass across the full run
  and targeted reruns. The initial run had one obsolete fixture that requested
  a two-byte brightness record; the positive short-read check now uses a valid
  two-byte name record and still requires a full-load I/O failure. The latest
  renderer suite additionally checks five app/firmware frame hashes, including
  auto sizing, page boundaries, scaled scroll and empty-message name fallback.
- NVS flash simulation retains a complete old/new maximum animation at sampled
  interruption positions. Existing schema-recovery, CRC and read-error tests
  continue to pass. These are simulated flash checks, not hardware power cuts.
- Normal BLE and BLE/provisioning sysbuilds pass for both DK cores, producing
  `merged.hex` and `merged_CPUNET.hex`. Normal application usage: 204760 bytes
  flash / 84900 bytes RAM (62.59% / 18.51% of these partition/linker regions).
  Network core: 148408 bytes flash / 46624 bytes RAM. These linker allocations
  do not establish runtime stack high-water values or the Rev A port budget.
- Debug Android APK builds and APK Signature Scheme v2 verification passes.
  Version0.2.0/build3. Release signing no longer defaults to the debug key;
  an owner-signed store artifact has not been produced or tested.
  Final debug APK SHA256:
  `6a41adca96841f772172e8f362bafb1c147f15cb132614b2d81fe3f65ed392f3`.
- Repository format, YAML, hygiene and `git diff --check` pass. Phone-size
  light/dark previews and a tablet with enlarged text were visually inspected.
  Local logs/screenshots are under ignored `build/` and `app/build/ui/`.

Artifacts live in the isolated worktree:

```powershell
Set-Location E:\Embedded\Nordic\smart-led-badge\build\pixel-studio
# Run these when ready for the physical bench checklist below.
adb install -r app/build/app/outputs/flutter-apk/app-debug.apk
west flash -d build/studio-dk --runner jlink --no-erase
```

Use the **normal** Studio image for app testing. `build/studio-provision`
contains the explicit recovery variant and is not the normal application image.
The four public `.env` presets were imported through the existing whitelist;
no full dotenv file or NFC credentials are included in the APK.

## Follow-up corrections (2026-10-13)

- Direct mode selection now uses the same content requirements as button
  cycling: Drawing requires nonblank valid artwork and Animation additionally
  requires at least two frames. Apply rejects unavailable modes before saving;
  boot falls back to QR if saved artwork no longer supports its default mode.
- Text size follows asynchronous draft loading and badge-draft replacement.
  Invalid local text can be replaced through the normal confirmation dialog;
  load errors use the existing action handler. All valid text previews run
  their clock, including short multiline and scaled multi-page messages.
- Saved link labels use the editor's 32-grapheme limit, including emoji,
  combining marks and joined emoji. The already-pinned characters1.4.0 package
  is now a direct dependency; no package version was changed.
- Follow-up verification: Flutter **40 tests** and clean analyzer; native modes
  and URL-update suites **2 configurations / 14 cases**; debug APK build;
  normal DK sysbuild for both cores; repository format, YAML and hygiene
  checks. Application flash204808B/RAM84900B; network flash148408B/RAM46624B.
  Follow-up APK SHA256:
  `a1228acd938a027bad7c41b0dbef95dd97be4738de4e62f76efa42df53252818`.
  Corrected artifacts have not
  been installed or flashed; the earlier upload evidence remains historical.

## Confirmed-name preview correction (2026-10-14)

Empty-text preview uses the confirmed badge name, with BADGE as the offline
default, rather than the unapplied identity editor. Send text still applies
only text and mode. The widget regression checks unapplied edits, Send text,
refreshed names and explicit Apply name. Flutter41 tests and clean analyzer
pass; the debug APK builds. No firmware change or new physical acceptance is
claimed, and the corrected APK has not been installed.

## Short physical acceptance checklist — pending

Deployment evidence: both normal-image cores were flashed with `--no-erase`;
the HEX files exclude the32KiB settings partition, and application readback
matches all153504 executable bytes checked against the ELF. No factory reset
or NFC provisioning was performed. Boot reports BLE ready and NFC unverified
(-13).

The owner reports **"Applied and confirmed; panel changed"** after a prompt
to send text or drawing. The specific upload kind was not supplied, so this
confirms one Studio upload rather than independent acceptance of both paths,
animation playback or persistence.

Nordic debug API memory reads halted the CPU and caused watchdog resets during
diagnostics. Explicit resume verified continuous uptime65.31->77.33s over12s;
the probe was then disconnected with the CPU running. Those restarts were
diagnostic interference and do not establish an idle firmware fault.

1. Install the matching app and normal BLE image. Reconnect an existing bond,
   refresh and confirm the displayed mode/settings. Re-discover services if
   Android retains the previous GATT table; do not factory-reset a working
   NFC receipt simply to clear a cache.
2. Save a custom short URL; Apply and confirm exact app/QR/NFC agreement.
   With a verified tag, remove the phone and run Sync again. With no receipt,
   confirm the setup gate; resync must not bypass it.
3. Send mixed-case text and multiline text. Compare fit, pages, alignment and
   scrolling against the app preview at each scale. Empty text should show
   the separately stored badge name.
4. Send a drawing with distinguishable corners, then a 16-frame animation.
   Compare bit layout, frame order and playback; Button1 should cycle modes
   and skip blank content appropriately.
5. Disconnect, restart the app and power-cycle the badge. Phone drafts and
   badge content should persist independently; animation should run offline.
6. Interrupt an upload and interrupt content→mode Apply. Reconnect/Refresh;
   no automatic replay, partial content activation or false confirmation.
7. Recheck authenticated pairing/writes on the new GATT table. Record real
   upload duration, stack high-water values and panel timing under BLE/NFC
   activity; software tests cannot establish those hardware properties.

Prior P2 NFC recovery remains unresolved: phone proof succeeded but firmware
finalization returned -13. This upgrade preserves that verification policy.
