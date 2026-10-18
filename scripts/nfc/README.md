# NFC bench tools

See [the P1 bench procedure](../../docs/nfc-prototype.md) for build, installation,
two-session RF proof and passive-read acceptance. This is a temporary SWD/USB
bench interface, not the P2 BLE companion app.

Helper v0.5/proof3 implements accepted
[ADR 0014](../../docs/decisions/0014-positive-nfc-provisioning-proof.md).
Owner accepted the amendment on 2026-10-12; positive phone checks, finalization
and receipt retention passed. Broader phone/protection acceptance is pending.
Matching firmware/host/helper are
required; current proof2 deployments cannot use this host version.

`badge_nfc.py` uses the existing Nordic SDK Python's `pylink` and `pyelftools`.
It resolves `badge_nfc_mailbox` from the matching provisioning ELF, connects
to the DK application core without resetting it and exchanges bounded commands.
Use `--jlink-serial` when multiple probes are attached. `--android-serial`
selects the single authorized USB phone for provisioning. `--elf` must match
the image actually flashed. `status` reads cached non-secret data only.

The firmware mailbox is version 4, 132 bytes. Little-endian offsets:

| Offset | Field |
|---|---|
| 0 / 4 | Magic `424E4643h` / version |
| 8 / 12 | Host request / firmware response sequence |
| 16 / 20 | Command / signed errno result |
| 24 / 28 | Verified-and-ready flag / seven-bit I2C address |
| 32 | UID, seven bytes plus padding |
| 40 / 68 | Desired / last verified URL, 28-byte slots |
| 96 | Command input, 28 bytes |
| 124 | Transient credentials, eight bytes (PWD4/PACK2/padding2) |

Commands are status=1, set-url=2, begin-provision=3, RF-proof-confirm=4,
sync=5. The host writes the sequence last and waits for the matching response.
Firmware clears input/credential slots before executing a command. The host
never reads the credential slot and clears it after completion or failure.
Confirm requires the pending UID and expires after 120 seconds. It is issued
only by the trusted host after successful one-use Android RF proof; the mailbox
is not a security boundary against a debugger owner.

`set-url` previews QR content while settings remain unchanged, verifies the tag,
then persists the URL. Display/tag failure or timeout restores the old display
state and attempts guarded tag rollback; the desired status URL remains the
persisted value. A failed rollback leaves the tag unverified and the command
unsuccessful. Physical power loss across separate settings/tag writes still
requires restart reconciliation or explicit provisioning after receipt loss.

The Android helper uses native `NfcA`, checks the exact 1 kB GET_VERSION reply,
and never displays/stores/logs tag credentials. A localhost-only HTTP session
through `adb reverse`, protected by a random bearer nonce, carries a one-use
challenge and non-secret proof. Both RF sessions check the UID/type and public
CC, authenticate the generated non-factory password with an exact PACK match,
then re-read the CC. It stops on lost/incorrect tags, failed PACK, transport
errors or missing healthy public reads. Physical field
removal between checks is required. Pausing the activity abandons an unfinished
verification. Host and phone retain transient credential buffers only; the
host clears mutable buffers and the firmware wipes secret buffers explicitly.
Runtime-managed temporary string copies are not a secure-erasure guarantee.

Helper v0.5 uses a synchronized workflow shared with JVM tests. Reader mode
stays active between checks; the removal listener enables the second-check
button after a 750 ms removal debounce. Pausing cancels visibly and late
callbacks cannot revive the session. The UI reports armed/detected stages for
each check and supports USB or wireless ADB. Physical field removal is still
an operator requirement; Android presence checks alone are not an RF timing
measurement. `VerificationFlowTest` covers removal gating, duplicate discovery,
pause/callback races and one-time tap claiming.

Reader mode also stays active after failure or completion while the helper is
in the foreground, so the tag cannot immediately launch a browser and hide the
result. The first failure owns the diagnostic; a subsequent pause does not
replace it. Reader mode is released on pause. This follows
[Android's reader-mode semantics](https://developer.android.com/reference/android/nfc/NfcAdapter#enableReaderMode(android.app.Activity,%20android.nfc.NfcAdapter.ReaderCallback,%20int,%20android.os.Bundle)).
Proof version 3 claims two successful generated-password sessions; it does not
claim directly observed factory-password rejection. Credentials are retained
transiently between checks and wiped after check2, on failure or on pause.

The retained negative-response bench utility requires the explicit `00h` wrong-password NAK
specified by [NXP Table 17 / PWD_AUTH](https://www.nxp.com/docs/en/data-sheet/NT3H2111_2211.pdf).
Only this response, bracketed by healthy public reads in the fresh RF session,
qualifies. Other NAKs and malformed replies fail; transport exceptions are
inconclusive and never produce proof. A phone that translates every NAK into an
I/O error cannot pass this check through `NfcA`; a reader path exposing the NAK
is required to directly observe it. That utility is no longer a provisioning
criterion; no I/O error is accepted as either type of proof.
Unauthenticated RF-write/metadata/bypass rejection remains a release bench gate.

HTTP proofs require integer `proof_version: 3`, true `generated_ok`,
`repeated_generated_ok` and `fresh_sessions`, matching UID and a consumed
one-use challenge; old helper proofs are rejected. Firmware also rejects old
proof receipts and uses mailbox version 4 so an old host cannot finalize new
firmware. Install v0.5 with matching firmware/host and
repeat explicit provisioning. No credentials or prior URI are recovered from
an old receipt; the tag's previous EEPROM content remains available to readers.

`build_android.ps1` uses existing SDK/JDK tools and creates a locally signed
APK under ignored `build/`. It neither installs tools nor installs the app.
Host regression tests run with `python scripts/nfc/test_badge_nfc.py`.
Every mailbox command attempts to zero the credential slot after setup, polling
or completion, including partially failed SWD setup writes. A cleanup error
preserves an existing command error; if the command otherwise succeeded, it
reports a fixed cleanup failure instead of success. Zeroization is best-effort
when the target becomes unreachable; a failed SWD cleanup cannot prove RAM was
cleared.
The pure Java RF checks run with `javac --release 8` and
`java -cp build/nfc-jvm NfcProofTest`, as in CI.

Mailbox v4 accommodates up to 25 URL characters plus NUL. RF proof stays at
version 3; use matching new host tools and firmware for the wider mailbox.
