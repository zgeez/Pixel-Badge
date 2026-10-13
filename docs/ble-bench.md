# P2 configuration bench checklist

Use the normal BLE image and Pixel Badge on the owner's Nothing phone. Keep
the provisioned tag and existing bond for checks 1–4. Use
[protocol v1](ble-protocol.md) for builds and NFC receipt restoration.

For security checks5–6, a fresh pairing must have a free bond slot. The
development image allows one stored bond without overwriting it; a second
central may be refused for capacity before passkey entry. That refusal cannot
establish wrong-passkey or pairing-window-expiry behavior. If only the bonded
Nothing phone is available, combine checks5–6 with check7: first record public
settings, forget the phone's badge bond and perform the controlled factory
reset; exercise closed-window/wrong-passkey/expiry before successful pairing;
then restore NFC verification and normal firmware, pair and restore settings.
Reset erases settings, Bluetooth keys and the NFC receipt; external tag EEPROM
remains. Keep it a controlled bench operation until configuration is restored.

Record firmware commit or local base plus ELF hash, app version/APK hash,
hardware, phone, exact actions, observed outcomes and raw log locations.
Distinguish owner observations from direct status/log evidence. A software
test or failed connection alone does not prove an RF security rejection.

| Check | Actions | Passing evidence |
|---|---|---|
| 1. Identity/display | Apply `ZIAD GHANEM`; tap Name mode; tap Low then High brightness. Mode and brightness apply immediately on selection. | Each reports Applied and confirmed; correct name appears and visible brightness changes. |
| 2. Settings persistence | Note the chosen name and brightness; leave Name mode selected. Disconnect Pixel Badge; restart the DK with both buttons released. Reconnect without opening a new pairing window and Refresh. | Name/mode/brightness retained, name displayed offline, existing bond reconnects. |
| 3. Fragmented name | Apply `ABCDEFGHIJKLMNOPQRSTUVWXYZ123456` (32 characters), then Refresh. Restore the preferred name. | Exact 32-character readback, usable scrolling display, no truncation or failed confirmation. |
| 4. Connection interruption | From a confirmed link, Apply another contact link and immediately background Pixel Badge. Return, reconnect and Refresh. Compare QR and NFC with the read-back link. | No automatic retry; interrupted/unconfirmed result stays uncertain until Refresh. The actual retained link agrees with QR/NFC. If Apply completed before backgrounding, record that the interruption case was not exercised. |
| 5. BLE access rejection | Use a fresh, unbonded central to attempt protected reads/writes outside the pairing window; then test wrong passkey inside a newly opened window. | Protected access/pairing rejected with recorded non-secret ATT/security outcome and applied settings unchanged. An existing bonded phone cannot establish this result. |
| 6. Pairing expiry | With a fresh central, start pairing inside the window but leave the passkey unanswered until it expires. | Pairing cancels, passkey display clears, configuration remains protected and original settings/bond are retained. |
| 7. Controlled reset/recovery | After recording all settings, test factory reset with Button 2 held at boot. Restore provisioning with the accepted two-check workflow, then restore normal BLE firmware and pair/configure again. | Defaults restored, old bond and NFC receipt cleared; URL Apply blocked until fresh verification; normal verified configuration restored. Perform this last because reset deliberately erases the receipt and bond. |

GitHub/LinkedIn QR/NFC agreement and LinkedIn off-state/restart retention
passed on 2026-10-12; [record](../validation/results/2026-10-12-dk-positive-nfc-proof/README.md).
Identity/display check1, settings/bond persistence check2,32-character
fragmented-name check3 and functional post-interruption agreement check4
passed by owner
report on 2026-10-13;
[settings record](../validation/results/2026-10-13-dk-p2-settings/README.md).
Checks5–7 remain pending until recorded. Closed-window new-pairing rejection
and deliberate wrong-passkey rejection are now corroborated by direct logs and
passed. Controlled pairing-window expiry check6 also passed with the badge's
explicit expired event during code display and owner automatic-clear/failure
report. Actual unauthorized ATT-write checks and NFC/configuration restoration
after reset remain pending. The recovery attempt accepted both phone checks
but firmware finalization failed with -13. Normal firmware was restored with
default settings and NFC unverified; the owner ended additional bench testing.
Check7 is incomplete. Earlier functional passes remain historical evidence.
For expiry, connect late in the window (about35 seconds after opening) and
leave the code unanswered; an earlier Bluetooth protocol timeout cannot count
as badge-window expiry.
Stage-disconnect cancellation, dropped notifications and stale completions have
portable test coverage. Exact cancellation/rollback timing, physical power-loss rollback and security
rejection need their own evidence. Unauthorized NFC writes/metadata bypasses and measured FD timing
remain separate NFC gates, as do Rev A coil/enclosure acceptance.
