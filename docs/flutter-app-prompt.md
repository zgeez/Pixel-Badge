# Flutter app implementation prompt

Copy this into the app-focused coding session. Continue the existing `app/`;
[ble-protocol.md](ble-protocol.md) is the authoritative wire contract.

```text
Build and refine Pixel Badge, the smart LED badge companion app, in this repository's
app/ directory using Flutter, Android first. Follow AGENTS.md, read the
private status/requirements/phone-app plan in order, then inspect app/.
Use docs/ble-protocol.md v2 exactly, retaining basic v1 badge compatibility.
Coordinate firmware changes through that shared contract. Do not commit/push,
install/upgrade global tools or accept licenses
unless explicitly authorized by the owner.

The app securely configures a nearby nRF5340 badge, which then works offline.
Create a polished Material 3 interface for connection, active contact link,
name, four brightness levels and QR/name mode. Use readable spacing, strong
status contrast, large controls and useful pending/empty/error states.
Pairing is through the phone OS: hold the badge Action button and enter the
six-digit passkey displayed on the badge in two rows of three digits.
Never collect or log passkeys, bond keys or NFC credentials in app UI/logs.

Use pinned universal_ble with a testable BLE adapter. Scan only for the badge
service, stop scanning before connect, discover services, validate protocol
info/version, pair, read protected status and subscribe to completion.
Firmware requires authenticated LE Secure Connections; an OS bonded flag
alone does not establish configuration readiness. Handle denied permission,
Bluetooth off, no badge, rejected/cancelled pairing, unsupported firmware,
disconnection and reconnect clearly. P2 is a foreground client, no background
service or unrelated permissions; no location request on Android 12+.

Service: f3b00001-6e9b-4f4c-a8f8-731a9d46e1c0. Characteristic prefixes:
02 info, 03 name, 04 URL, 05 brightness, 06 mode, 07 Apply, 08 Status.
Read the complete contract for exact fields, flags, security and limits.
Stage one field then Apply. Name fragments are [offset,total,ASCII...] with
up to 18 data bytes; URL/brightness/mode writes are raw. Support ATT MTU 23,
not just a larger negotiated MTU. No prepared/write-without-response requests.

Offer Portfolio, LinkedIn, GitHub and Resume. Obtain these from the four
BADGE_URL_* root .env entries through scripts/app/dev.ps1. Bundle only those
public-link build defines, never the entire dotenv file/unrelated secrets.
Do not commit local define JSON or personal links. Builds without defines
show unavailable presets rather than fake working links. Uppercase ONLY the
HTTP(S) scheme, preserve path case and all remaining bytes. Validate bounded
ASCII/QR fit and name before sending, but firmware remains authoritative.
No automatic destination cycling; Button 1 on the badge cycles modes.

A successful BLE write response means request acceptance, not application.
Use a nonzero ID increasing within this connection, match status version,
ID, field, complete phase and result zero, then read exact applied bytes.
For URL also require the NFC-verified flag before showing Active. Keep the
requested selection separate from the last read/confirmed link. Pending,
rejected, mismatched, timed-out or disconnected requests never look applied.
Read Status if notifications are dropped. Ignore stale completions, serialize
GATT operations, prevent double submission and clean up subscriptions.
Never automatically replay Apply: an accepted operation can finish after
disconnect. Refresh/reconnect before another Apply after uncertain results.

Studio adds custom short links, guarded NFC status/resync, smarter text and
32x32 monochrome editing with up to 16 animation frames. Follow the phone's
light/dark setting, save offline drafts separately from badge readback and
never replay an interrupted upload. Use the shared case-sensitive bitmap
font/layout for previews, default-MTU staging and paged artwork readback.
See app/README.md for current limits, content-to-mode Apply and draft behavior.

Do not add recording, app-key enrollment, DFU, cloud accounts or analytics.
Audio belongs to P3 and accepted ADR 0011. Keep it possible later
without claiming those features exist now. Android is the initial bench
target; iOS packaging/testing needs a later macOS/Xcode run.

Run dart format, flutter analyze and meaningful tests: stale/wrong IDs,
dropped notification with status fallback, rejection, success with incorrect
readback, disconnect after Apply, fragmented names and path preservation.
Build a debug APK with installed tools; do not install SDK packages or accept
licenses without approval. Report actual evidence separately from physical
BLE pairing, QR/NFC and phone acceptance. Finish with a concise change summary,
exact run/build commands, artifacts and unresolved assumptions.
```
