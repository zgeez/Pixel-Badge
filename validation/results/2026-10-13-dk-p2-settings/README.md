# P2 remaining bench checks — 2026-10-13

Personal identifiers/URLs in excerpts are redacted; raw artifacts are unchanged.

Result: **current software checks and manually reported identity/mode/brightness,
offline settings retention, bonded reconnect,32-character name and interrupted-
connection readback/QR/NFC agreement checks passed; closed-window pairing,
wrong-passkey rejection and own-window expiry passed; reset recovery incomplete**.
This is not a full P2 acceptance result. The controlled reset has now been
performed; both recovery phone checks passed but firmware finalization failed.
I ended further bench tests and requested the commit handoff. Normal
firmware is restored with default settings and NFC unverified.

Hardware: nRF5340 DK with HUB75 and NFC Tag 2 Click; Nothing phone A069P /
Android 16 used over wireless ADB (disconnected at handoff). Source: main11cfc23 plus uncommitted
local fw/prototype-ble work. Normal BLE firmware and Pixel Badge D2 remain
installed. Initial checks used the existing bond/receipt; the later manual reset
and phone bond removal are recorded below. A later normal-image flash adds
non-secret pairing outcome logs; provisioning restoration is still pending.

Artifacts rechecked against the installed/flashed bench artifacts from the
previous run:

| Artifact | SHA256 |
|---|---|
| Normal application ELF | 5836A2E2C5F2A02EB9C02733D062F46E4E3E29649978FFD6D0CCCA5709C30A96 |
| Pixel Badge D2 APK, 0.1.1/build2 | 2B73B7286318EA336BF95E2CA93229C873BA73D32AE6DC8035334D209AD102CA |

The prior [GitHub/LinkedIn contact-link and power-cycle checks](../2026-10-12-dk-positive-nfc-proof/README.md)
remain recorded. Follow the [remaining bench checklist](../../../docs/ble-bench.md).

## Software results

- Full pinned NCS3.4.1 native_sim suite: 15 configurations / 84 cases passed,
  zero failures/errors/warnings. Full local log:
  `build/p2-final-native-20261013.log`; reports under
  `build/p2-final-native-20261013/`. This includes the target-errno variant.
- Flutter: 17 tests passed; analyzer reports no issues.
- QR link generator: 8 tests; app define whitelist: 2 tests; NFC host: 12 tests.
  Initial NFC host run on Python3.14 exposed unclosed synthetic HTTPError
  responses in the test. Closing those responses and asserting HTTP403 fixes
  the resource warning; all12 pass with ResourceWarning treated as an error
  and under the existing Nordic SDK Python. Production provisioning unchanged.

## Direct non-secret status

Matching-ELF read-only J-Link access before manual settings confirmation shows
NFC prooftrue, ready_state1, last_result0, operation_active0 and public
desired=verified [redacted URL] A later public applied-settings snapshot
reads:

```json
{"active_mode": 1, "brightness": 16, "default_mode": 1, "name": "[redacted name]", "url": "[redacted URL]"}
```

These are transcribed tool outputs, not an archived phone ATT/QR trace. They
corroborate current applied values, not the physical display, observed brightness
changes or a new restart. Subsequent manual confirmation is recorded below.
No key, passkey, provisioning credential or private phone data was read.

## Identity/display result

The recorded response was **All three settings work** to the requested name Apply, Name
mode selection and lower/higher brightness checks. This records correct name
display, visible brightness change and confirmed operations by manual observation.

After that report, matching-ELF public status reads:

```json
{"active_mode": 1, "brightness": 16, "default_mode": 1, "name": "Firmware", "url": "[redacted URL]"}
```

NFC remains prooftrue/ready1/last_result0/operation_active0. The last sampled
Apply is id3/field2/result0/not pending (name). This corroborates a completed
name operation and current public settings; the selected current name
differs from the earlier snapshot. No stored value was changed by the reader.
Separate mode/brightness request IDs were not captured.

## Persistence/reconnect result

The recorded response was **All settings retained; reconnect needs no new code** after
disconnecting, restarting with both buttons released, observing the name
offline and reconnecting/refreshing without reopening the pairing window.
This passes settings and bond persistence on the tested DK/phone setup by
manual observation.

After that report, matching-ELF read-only public snapshot shows:

```json
{"active_mode": 1, "brightness": 255, "default_mode": 1, "name": "MS student", "url": "[redacted URL]"}
```

NFC prooftrue/ready1/last_result0/operation_active0 and desired=verified /L
remain intact. Protocol is idle (id0/result0/not pending). The sampled settings differ from the earlier Firmware/Low snapshot; I chose preferred values before restart. No exact pre-restart capture of
MS student/Full is available, so persistence is manually reported and post-restart
values directly corroborated. No J-Link probe ran during the requested power
cycle; reading resumed after I confirmed restoration.

## Fragmented-name result

Manual observation: **Exact32-character readback and full scrolling work** after
applying `ABCDEFGHIJKLMNOPQRSTUVWXYZ123456` and refreshing settings. This passes
the maximum-name fragmentation/readback/display check by manual observation.

After the restore-preferred-name instruction, direct current snapshot shows
name [redacted name], active/default Name, brightness255 and /L. Last sampled Apply is
id4/field2/result0/not pending; NFC prooftrue/ready1/result0/operation_active0.
This is a successful current name command, not a captured32-character transfer.
No actual ATT name fragments or earlier long-name snapshot were captured.

## Interrupted-connection result

Manual observation: **Interrupted; refreshed app, QR and NFC agree** after the
requested QR/GitHub Apply, immediate Home while Applying and reopen/reconnect/
Refresh sequence. This passes functional post-interruption agreement by manual observation. A cancelled or already-running request can leave either the old or
new applied link; successful refresh and actual QR/NFC agreement determine the
accepted state, not automatic reapplication of the requested destination.

Direct read-only matching-ELF status afterward shows current name Ws in the
chat, active QR/default Name,brightness255, applied URL /L. NFC desired=verified
[redacted URL], prooftrue/ready1/result0/operation_active0. Protocol is idle
(id0/result0/not pending) after reconnect. Thus current retained LinkedIn is
verified; no successful GitHub commit is claimed for this interrupted request.
No request IDs or ATT traffic across disconnect were captured; cancellation
versus rollback versus completion timing is not established. Physical power-
loss atomicity and EEPROM/storage interruption remain separate gates.

Check4 functional recovery agreement passes; checks5–7 remain pending.

## Single-phone security/reset recovery preparation

Manual observation: only the Nothing phone is available. The installed firmware has
MAX_PAIRED1 and no oldest-bond overwrite; the pinned SDK allocates a key slot
before sending its security request. An occupied slot can reject a second
central before passkey entry, so that rejection cannot prove wrong-passkey or
expiry behavior.

Checks5–6 will run inside the planned controlled reset/recovery check7: preserve
public settings, forget the phone bond, factory-reset the badge, then exercise
closed-window/wrong-passkey/expiry before successful pairing. Afterward restore
positive NFC verification with helper0.5/proof3, normal firmware and settings.
No generic failed Pixel Badge connection is treated as an actual unauthorized
ATT-write rejection test.

Recovery artifacts rechecked: both normal/provisioning merged application and
network images exist; installed NFC helper is0.5/code5 with the recorded hash.
Current provisioning ELF SHA256:
E69C16108DF856EEB3C80B226186FED63747D9179CDAACC2D9790183C3DB9E68.
Public pre-reset settings saved locally in
`build/p2-pre-reset-public-settings.json` (no keys or provisioning credentials).
Factory-reset step requested; no tool-initiated reset, flash or bond deletion at
preparation time. Do not probe J-Link during the manual power-cycle operation.

Security bench constraint: Windows reports zero present Bluetooth devices.
This setup has not yet exercised fresh unbonded protected writes or wrong
passkey. The checklist describes those independent gates; no ordinary bonded
reconnect counts as their proof.

## Controlled factory reset

Manual observation: default QR appears and phone bond forgotten after the requested
Button2-at-power-on reset. Matching-ELF read-only snapshot shows:

```json
{"active_mode": 0, "brightness": 128, "default_mode": 0, "name": "BADGE", "url": "[redacted URL]"}
```

NFC prooffalse/ready_state0/last_result-13/operation_active0; protocol idle.
This is the expected erased-receipt/default-settings state, not an unexpected
loss of the earlier verification. External tag EEPROM is not erased by this
operation, so NFC/QR equality is not yet restored.

Actual RTT capture contains `factory reset: settings partition erased`, all
five defaulted fields and `badge ready: QR mode`. Local raw capture:
`build/p2-reset-closed-window-rtt-20261013.log`. This independently confirms the
badge settings-partition erase that also contains Bluetooth keys and receipt;
phone bond removal is manually reported. Reset half of check7 passes; provisioning/
normal configuration restoration remains pending. No tool-initiated reset or
setting write occurred during the read/capture.

Next requested check is one Pixel Badge connection attempt with both buttons
released and the pairing window closed. Wrong-passkey/expiry and genuine
unauthorized ATT-write evidence remain separate pending gates.

## Closed-window attempt remains inconclusive

Manual observation: a pairing dialog appeared; cancelling it produced an error.
Cancellation alone does not establish whether the badge rejected pairing.
Asked whether the panel continued showing QR or displayed a six-digit code,
and for the non-secret error text. No pass or security failure is recorded yet.

Current firmware config confirms BT_SMP_APP_PAIRING_ACCEPT enabled, SC-only/
MITM enforced and the application callback requires the physical window.
Post-cancellation read-only control flags show enabledtrue, passkey_visiblefalse,
advertising active. Those current flags do not establish what the panel showed
during the dialog. Pinned universal_ble2.3.0 Android implementation invokes
BluetoothDevice.createBond; a phone-system prompt is not itself proof of badge
authorization. No raw SMP/ATT rejection captured and no policy change made.

Bench clarification: the badge stayed on QR; the dialog was only on the phone.
No badge code display was observed. Cancellation still does not establish the
firmware's rejection outcome.

## Pairing outcome instrumentation

Added non-secret firmware logs for new-pairing policy allowed/rejected,
security level/error, pairing completion/failure and window expiry. No passkey,
keys, peer address, NFC credentials or application payload are logged. Pairing
policy/permissions remain unchanged. Normal and BLE/provisioning builds and
repository format/YAML/hygiene checks passed. Host's field-removal prompt now
says phone/other readers rather than both phones; all12 host tests pass with
ResourceWarning as error. Existing native84/app17 results predate this logging
edit; no unchanged-domain rerun is claimed.

Both normal domains flashed; app D2 unchanged. New application ELF hashes:

| Image | SHA256 |
|---|---|
| Normal BLE | 943CE439E1C76E05C27791C0985FFE5F44BD186D63718445E28B3537E2B0DBF3 |
| BLE plus provisioning recovery | 0990D67EA180377CC14BB74578D8FDFA69B2E6BEB9F2691EB5858143E84F83B6 |

Capture `build/p2-closed-window-security-rtt-20261013.log` contains fresh boot,
defaulted fields and NFC needs-verification status, but no pairing rejection
event yet. Requested retry: no Button2, accept only an initial simple phone
Pair confirmation, leave any code field blank, wait10 seconds and distinguish
automatic rejection from waiting/code display. Outcome remains pending.

## Closed-window new pairing rejected

Manual observation: accepting the phone Pair prompt, followed by Android's error
that pairing failed because of an incorrect pairing code. Actual matching-image
RTT shows repeated `BLE new pairing policy: rejected`, then security level1
and error9, pairing failure and resumed advertising. See the
[captured excerpt](closed-window-rtt.log); ANSI color escapes removed, otherwise
the first complete event sequence is preserved. Full local capture:
`build/p2-closed-window-after-attempt-rtt-20261013.log`.

In this SDK error9 is BT_SECURITY_ERR_UNSPECIFIED; it is not interpreted as
proof of an entered wrong passkey. The explicit policy-rejected log establishes
the application decision. After capture, read-only control state is enabledtrue,
writabletrue,job_activefalse,pairing_deadline0,passkey_visiblefalse. I had
not opened the window. Closed-window new-pairing rejection passes; Android's
generic error text does not indicate a firmware code mismatch in this test.
No raw unauthorized ATT setting write was sent/captured.

Next requested check: open the physical window, connect and enter an explicitly
different six-digit code, then observe failure/return to QR/inaccessible settings.
Wrong-code rejection result is recorded below; expiry is pending and NFC/
configuration restoration follows.

## Deliberate wrong-code rejection

Manual observation: wrong code rejected, badge returned to QR and settings remained
inaccessible after opening Button2's window and deliberately entering a
different code. Actual RTT first records policy allowed, then security level1/
error1, pairing failed1 and advertising resumed. In this pinned SDK error1 is
BT_SECURITY_ERR_AUTH_FAIL. See [actual excerpt](wrong-code-rtt.log), extracted
with only ANSI escapes removed from the local full capture
`build/p2-wrong-code-after-attempt-rtt-20261013.log`.

Wrong-passkey rejection passes on this DK/phone/image. No correct passkey was
recorded/read or sent to this chat; no authenticated settings access is claimed.
Actual unauthorized ATT-write rejection remains independently unexercised.

Next expiry check deliberately delays connecting until about35 seconds after
the physical window opens, then leaves the code unanswered. Starting the
unanswered procedure early can hit the Bluetooth protocol timeout first; that
would not establish the badge's own window expiry. Observe code display followed
by automatic QR return/failure and corroborate the firmware expiry event. No
expiry acceptance is claimed until observed. NFC/configuration restoration remains
pending after this controlled reset.

## Unanswered pairing hit protocol timeout first

Manual observation: code appeared, cleared automatically and pairing failed.
Actual RTT records policyallowed at00:32:38.472198, then `SMP Timeout` at
00:33:08.533569 (about30.06 seconds later), level1/error9 and pairingfailed9.
See [actual excerpt](protocol-timeout-rtt.log). This establishes safe unanswered
protocol-timeout failure, not the badge's60-second window-expiry path; that
path remains pending. The timing difference is from firmware log timestamps,
not a physical button-to-display measurement.

Prepared a read-only local monitor of pairing_deadline, passkey-visible flag,
kernel curr_tick and RTT with the matching ELF. It never reads the displayed
code or writes any policy. Announced ticks omit the driver's unannounced elapsed
ticks, so remaining milliseconds are approximate; they are used only to leave
a substantial late-connect margin. The next step was to open the Button2 window and
wait for a connect cue based on the actual deadline. No new firmware edit,
build or flash for this monitor. NFC/configuration restoration still pending.

Open decisions remain OD22 carrier fit and OD60 deferred iPhone LinkedIn
handoff; remaining settings/security/recovery, unauthorized NFC writes, FD timing and
Rev A acceptance remain verification dependencies.

## Controlled badge-window expiry passed

I opened the physical window and waited for the timed cue. Read-only
monitor observed deadline3123473ms, a declining remaining-time estimate and
passkey_visibletrue shortly before that deadline. Actual fresh RTT records
policyallowed00:52:01.575500 followed by `BLE pairing window expired` at
00:52:03.477447, level1/error9 and pairingfailed9. See
[actual excerpt](window-expiry-rtt.log); full local RTT is
`build/p2-expiry-controlled-rtt-20261013.log`. The earlier SMP-timeout records
in that capture were excluded from this excerpt.

My bench notes confirm code appeared, cleared automatically and pairing failed.
Unlike the earlier attempt, the explicit badge-expired event establishes
check6: the badge clears the in-progress code and refuses completion when its
window ends. Observed timestamps/deadline are from firmware, not an external
clock/physical-button timing measurement. No passkey read or policy write.

Normal configuration recovery has started: both matching BLE/provisioning
domains flashed (ELF0990D67E... above). Non-secret mailbox reports needs
verification with default Portfolio desired and no verified URL. The
uninitialized driver's address field is85 before explicit provisioning; it
does not establish the tag's actual RF/I2C address. Waiting for test phone
readiness before starting the fresh one-use two-tap proof session. No proof
accepted or receipt restored yet. After verification, restore normal firmware,
pair correctly and restore saved public settings.

## Recovery failed finalization; normal firmware restored

In fresh session73447, I reported both RF checks passed and phone
removed; the host printed `RF proof accepted`. After computer confirmation,
firmware command4 returned -13. The host also reported ADB reverse cleanup
failure and the phone disconnected. ADB loss does not establish the cause of
the firmware finalization error, which is sent through SWD. No fresh receipt
was finalized. An earlier session expired without fetching its challenge.

Added fixed non-secret finalization logs for missing pending state, expiry,
identity mismatch, guarded-policy failure and RF transition; no guard changed
and no UID/password/PACK/bearer is logged. Diagnostic image built and repository
checks passed, SHA256
`2C8F19ED8301B49C7383829B1A0DE0347E537A861CD4D00CA33AAA3EEE9B1D95`.
It was briefly flashed, but no further phone retry occurred after I
ended bench testing. The earlier failure's RTT buffer did not capture a reason,
so the exact cause remains unresolved.

Both normal BLE domains restored using the existing tested application ELF
`943CE439E1C76E05C27791C0985FFE5F44BD186D63718445E28B3537E2B0DBF3`,
with debug provisioning disabled and no erase. Read-only matching-ELF status:
name BADGE, URL `[redacted URL]`, brightness128, default/active QR;
NFC prooffalse, ready0, result-13, operation inactive and no Apply pending.
QR/NFC agreement and a working bond are not claimed for this reset state.
Recovery check7 remains incomplete; prior functional results remain historical.

The two host failure-path corrections received targeted regression checks:
app defines3 passed (missing file emits four empty presets; bad present input
still fails) and NFC host15 passed with ResourceWarning treated as an error
(including partial setup writes, original-error preservation and cleanup-only
failure). No additional full firmware/Twister/Flutter or phone bench run.
