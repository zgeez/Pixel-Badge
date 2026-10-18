# Synchronized bounce lines and expanded editor - 2026-10-17

Pixel Badge0.10.1/build29 and matching nRF5340 DK firmware refine layout3;
the wire format, capability bit, storage schema and partitions are unchanged.

Overflowing lines share one triangular elapsed-time phase. A line travels from
left=+3 to left=-(its overflow+3). The longest line sets the half-cycle duration:
(longest overflow+6) times the configured interval. Shorter overflowing lines
move more slowly and reverse at the same instant. There is no intentional
endpoint hold. Position rounds to the nearest physical pixel; discrete pixel
steps remain visible, especially for slower lines. Fitting lines retain alignment.
Extra rows page after a full cycle; all-fitting pages retain their40-interval hold.

Opening the keyboard expands the text field into the preview space, with text
aligned at the top. Controls and Send remain available. The field retains the
same controller/focus node while its constraints change; closing the keyboard
restores the preview. [Regular screen](keyboard-editor.png) and
[small screen with2x text](keyboard-large-text.png) use synthetic fixture text.

## Verification

- Full Flutter suite133 cases passes without optional private-media fixtures.
- Targeted editor suite6 cases passes after the final keyboard alignment change,
  including field growth, focus/controller/text preservation and small/large-text
  layout checks. Analyzer and formatting pass.
- Native content suite8 cases passes, one executed native_sim/native configuration,
  no warnings. Both languages check every pixel against manually specified line
  positions at0/100/750/1500/1600/2250/3000ms for different line lengths. This
  exercises shared reversals, slower short-line motion,3-pixel margins and
  departure from both endpoints. Empty-line/max-elapsed safety is also checked.
- Normal, recording and NFC-setup DK builds pass. Recording allocation is206776
  flash bytes and104505 RAM bytes; this is not nRF52840 acceptance.
- Repository C/C++ formatting, YAML, hygiene and whitespace checks pass.

Local ignored logs: bounce-app-tests.log, bounce-editor-verified.log,
bounce-analyze.log, bounce-native-verified.log, bounce-content-native/,
bounce-*-build.log and bounce-final-apk.log. The native check used the repository's
pinned SDK container and host dependencies; no global tools were installed.

## Deployment

The app was installed in place, package version0.10.1/code29 verified, and a fresh
draft/library hash was unchanged across installation. Local contact presets were
included by the existing whitelisted build-define generator. The tested recording
image was flashed without erase after a fresh encrypted persistent-region backup.
Post-flash persistent storage had zero changed or newly programmed bytes versus
the backup. NFC ready/result0, loaded bond, healthy boot and running CPU were
observed. A bonded reconnect restored enabled settings controls without a new
code. Details are recorded in [deployment.json](deployment.json).
No owner text/design was applied to exercise the new renderer. Panel motion and
readability remain a physical follow-up; software rendering tests and builds do
not establish optical acceptance. Other grayscale/combined-load and nRF52840
port gates remain open. No new design decision, commit or push.
