# NFC bench tools

See [the P1 bench procedure](../../docs/nfc-prototype.md) for build, installation,
two-session RF proof and passive-read acceptance. This is a temporary SWD/USB
bench interface, not the P2 BLE companion app.

`badge_nfc.py` uses the existing Nordic SDK Python's `pylink` and `pyelftools`.
It resolves `badge_nfc_mailbox` from the matching provisioning ELF, connects
to the DK application core without resetting it and exchanges bounded commands.
Use `--jlink-serial` when multiple probes are attached. `--android-serial`
selects the single authorized USB phone for provisioning. `--elf` must match
the image actually flashed. `status` reads cached non-secret data only.

The firmware mailbox is version 2, 120 bytes. Little-endian offsets:

| Offset | Field |
|---|---|
| 0 / 4 | Magic `424E4643h` / version |
| 8 / 12 | Host request / firmware response sequence |
| 16 / 20 | Command / signed errno result |
| 24 / 28 | Verified-and-ready flag / seven-bit I2C address |
| 32 | UID, seven bytes plus padding |
| 40 / 64 | Desired / last verified URL, 24-byte slots |
| 88 | Command input, 24 bytes |
| 112 | Transient credentials, eight bytes (PWD4/PACK2/padding2) |

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
challenge and non-secret proof. It stops on lost/incorrect tags, failed PACK,
factory-password acceptance or missing healthy public reads. Physical field
removal between checks is required. Pausing the activity abandons an unfinished
verification. Host and phone retain transient credential buffers only; the
host clears mutable buffers and the firmware wipes secret buffers explicitly.
Runtime-managed temporary string copies are not a secure-erasure guarantee.

Helper v0.3 uses a synchronized workflow shared with JVM tests. Reader mode
stays active between checks; the removal listener enables the second-check
button after a 750 ms removal debounce. Pausing cancels visibly and late
callbacks cannot revive the session. The UI reports armed/detected stages for
each check and supports USB or wireless ADB. Physical field removal is still
an operator requirement; Android presence checks alone are not an RF timing
measurement. `VerificationFlowTest` covers removal gating, duplicate discovery,
pause/callback races and one-time tap claiming.

The factory-password check requires the explicit `00h` wrong-password NAK
specified by [NXP Table 17 / PWD_AUTH](https://www.nxp.com/docs/en/data-sheet/NT3H2111_2211.pdf).
Only this response, bracketed by healthy public reads in the fresh RF session,
qualifies. Other NAKs and malformed replies fail; transport exceptions are
inconclusive and never produce proof. A phone that translates every NAK into an
I/O error cannot pass this check through `NfcA`; a reader path exposing the NAK
is required. The helper reports that limit instead of confirming provisioning.

HTTP proofs require integer `proof_version: 2`; old helper proofs are rejected.
Firmware also rejects old proof receipts and uses mailbox version 2 so an old
host cannot finalize new firmware. Install v0.3 with matching firmware/host and
repeat explicit provisioning. No credentials or prior URI are recovered from
an old receipt; the tag's previous EEPROM content remains available to readers.

`build_android.ps1` uses existing SDK/JDK tools and creates a locally signed
APK under ignored `build/`. It neither installs tools nor installs the app.
Host regression tests run with `python scripts/nfc/test_badge_nfc.py`.
The pure Java RF checks run with `javac --release 8` and
`java -cp build/nfc-jvm NfcProofTest`, as in CI.
