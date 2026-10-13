# P1 NFC prototype bench

Status: **proof3 provisioning completed and retained after normal BLE restore; powered/off/restore reads owner-reported passed;
remaining acceptance pending**. The owner has
an NFC Tag 2 Click connected to the nRF5340 DK, a Nothing 4a Pro and an iPhone
13. On 2026-10-12 the owner reported LinkedIn URL reads work before power-off,
while powered off, and after power restoration. The report does not identify
the phone for each case or provide fresh raw captures. After the helper v0.2 correction,
provisioning confirmation succeeded; direct SWD/RTT evidence is in the
[provisioning result](../validation/results/2026-10-11-dk-p1-provisioning/README.md).
Full acceptance remains pending. These steps use the
existing DK wiring and the HUB75 panel; they do not validate the Rev A coil,
carrier or switched tag supply.

On 2026-10-12, helper v0.5 completed both positive checks, the host accepted
proof3 and firmware finalized. Readytrue and matching Portfolio URL were
confirmed; normal BLE restoration retained proof/ready1/result0 and healthy
boot. See [the positive-proof result](../validation/results/2026-10-12-dk-positive-nfc-proof/README.md).
Broader protection/timing and new contact-link acceptance remain pending.

The 2026-10-11 workflow used helper v0.2, which could mistake a transport error
for factory-password rejection. Its recorded completion is historical evidence,
not proof of that rejection. The corrected firmware, host and helper v0.3 used
proof version 2 and rejected old receipts/protocols. That workflow required
fresh explicit provisioning; ordinary URL taps cannot supply verification.
During the P2 follow-up, helper v0.4 was installed and the owner reported
**Check 2 is inconclusive**. No host proof was accepted or receipt restored.
The explicit rejection response remains unverified; FD timing tests are pending.
Helper v0.4 retains reader mode after a check failure or completion until the
activity pauses, and preserves the first error instead of replacing it with
focus loss caused by ordinary URL dispatch.

## Build and install the bench tools

The commands below use helper v0.5/proof version 3, accepted by the owner in
[ADR 0014](decisions/0014-positive-nfc-provisioning-proof.md) on 2026-10-12.
Previously installed v0.4/proof2 images do not match these new host tools;
build, flash and install the matching versions together. The two-session
provisioning run passed; repeat it after a lost receipt/factory reset.

From the repository root in PowerShell:

```powershell
.\scripts\dev.ps1 build -Pristine -BuildDir build/p1-provisioning -ExtraConfFile firmware/configs/nfc-provisioning.conf
west flash -d build/p1-provisioning --runner jlink
.\scripts\nfc\build_android.ps1 -Jdk 'C:/Program Files/Java/jdk-25'
adb devices
```

Use the existing SDK environment for `west`. Flash the sysbuild directory,
which includes MCUboot and the merged image. Keep phones away from the tag
while flashing and booting. Close DK COM-port terminals; use RTT. On a tag
without a verified receipt, `NFC boot result -13` is expected: normal boot
does not provision an untrusted tag. QR/name and brightness controls still work.

Enable USB debugging on the Nothing phone, connect it by USB, unlock it and
accept its computer authorization prompt. Replace `PHONE_SERIAL` below with
the serial shown by `adb devices`:

```powershell
adb -s PHONE_SERIAL install -r build/p1-nfc-helper/badge-nfc-helper.apk
$nfcPython = 'C:/ncs/toolchains/4f5b6ad6dd/opt/bin/python.exe'
& $nfcPython scripts/nfc/badge_nfc.py provision --android-serial PHONE_SERIAL
```

The helper is a temporary native Android RF verifier, separate from the P2
BLE app. It requires the existing Android SDK 35 tools and a JDK; its build
script installs nothing. Its local development APK signing key is unrelated
to tag credentials. Do not install the helper on the iPhone.

If the USB cable prevents tapping, connect the phone over wireless ADB instead:
enable Wireless debugging on the same Wi-Fi network as the computer, use
`adb pair IP:PAIRING_PORT` with the displayed code, then
`adb connect IP:CONNECTION_PORT`. Use the connected endpoint from `adb devices`
as `--android-serial`. The helper's USB wording also applies to this ADB
connection. Keep the DK connected by USB.

The computer waits for the helper's proof before prompting for Enter. Opening
a link with an ordinary NFC tap does not supply that proof. The helper must
show **Both RF checks passed** and the computer must print **RF proof accepted**.
Then remove both phones and press Enter. Host failures distinguish an unfetched
challenge, missing/rejected proof and a numbered firmware-command failure.

## Provision once, then check the status

1. Leave the DK powered and keep the iPhone away. The host generates a random
   per-run PWD/PACK in memory and sends them over SWD. Never dump target RAM,
   credentials, HTTP bodies or password-bearing registers during provisioning.
2. On Android, press **Authenticate tag**, then tap the Click. The helper
   checks GET_VERSION for the NTAG I2C plus 1 kB, checks the UID and verifies
   that the generated password returns the generated PACK.
3. Lift the phone completely away until the RF field is absent. Helper v0.5
   waits for removal detection before enabling **Start check 2 of 2**. Press
   that button and tap again. Expect **Check 2 of 2 ready** before the tap and
   **Check 2 of 2: tag detected** while it runs. In this fresh session the
   generated password must authenticate again with an exact PACK match and
   healthy public CC reads before and after authentication. Physical
   removal is essential; closing an NFC connection alone does not reset RF
   authentication. Keep the helper in the foreground throughout the two checks.
   If it loses focus, it now shows a stopped-session error; restart from the
   computer. It keeps reader mode active between checks to prevent an ordinary
   NDEF tap from opening a browser, and keeps the screen awake. Removal detection
   helps the workflow; manual RF-field removal remains required for acceptance.
   A timeout, lost/mismatched PACK, `IOException`, NAK or malformed response
   never qualifies. The credential remains transiently in memory for the two
   checks and is cleared after check 2 or on failure/pause. This workflow makes
   no claim of directly observed factory-password rejection. Unauthenticated
   write rejection remains a separate required bench check.
4. Remove both phones. Press Enter at the computer promptly: firmware allows
   120 seconds from the first provisioning command to final confirmation.
   The host allows 100 seconds for the two phone checks. The firmware sets the
   reversible CC lock pair, verifies the profile and NDEF, then saves a
   non-secret UID/profile receipt. Success is reported only after this finishes.
5. Check the non-secret status:

   ```powershell
   & $nfcPython scripts/nfc/badge_nfc.py status
   ```

   Expect `result: 0`, `state: 1`, address `2`, and the desired/last-verified
   URL. Provisioning deliberately changes the factory address `0x55` to `0x02`
   by writing address byte `04h`; UID byte 0 reads back as `04h` independently
   of the address. RTT reports command numbers/results and verification state.
   Neither tool prints PWD/PACK. No REG_LOCK or irreversible configuration
   locking command is available.

The QR default comes from `.env` at build time; valid saved settings take
precedence. Button 1 still toggles QR/name. There is no automatic link cycling.
Use the explicit bench `set-url` command below to select another destination.

## Powered and passive reads on both phones

1. Exit the Android helper so ordinary NFC reading is enabled. Decode the QR
   in QR mode and read the stored NDEF URI on each phone while the DK is powered.
   Inspect the actual URI record with a tag reader; a browser redirect alone
   does not prove the stored URL. Record the reader app/version used. URI scheme
   comparison may ignore case; host/path/percent escapes must match exactly.
2. Remove every DK power source, including USB and external supply. The Click
   must have no external supply or separately powered connection. Tap with
   each phone and verify the same NDEF URI. The phone powers the passive tag.
3. Remove both phones, reconnect DK power and check normal boot. In RTT expect
   `NFC boot result 0; QR URL verified`. Check controls and the restored QR.
4. With phones away, change to another configured short link:

   ```powershell
   & $nfcPython scripts/nfc/badge_nfc.py set-url --url https://ZIADG.COM/L
   & $nfcPython scripts/nfc/badge_nfc.py status
   ```

   This validates/persists the URL, enters QR mode, waits for a successful
   display submission, writes/verifies tag EEPROM and reports success. Repeat
   powered/off reads on both phones, then restore the desired link with the
   same command (for example `https://ZIADG.COM`). A display submission is not
   optical evidence; independently decode the displayed QR.

## Failure and protection checks

Run the full [validation section 6](validation.md#6-nfc). A normal phone tap
only establishes reading; it does not establish RF-write protection.

- In a fresh unauthenticated RF session, use an NFC writer to attempt another
  NDEF URL. Expect rejection and verify the exact old URI without MCU power.
  Check powered and passive reads afterwards, then perform another MCU update.
- CC, static/dynamic lock and configuration-page attack tests require a reader
  that supports raw Type 2 commands and the reviewed page-specific procedure.
  Record these as **pending** if the phone app cannot perform them. Do not
  substitute the helper's authentication proof for these tests. Extra RF locks
  are reversible via I2C; the service clears them while retaining CC locks.
- During a `sync` or `set-url`, introduce/remove a reader field. Expect a
  bounded error rather than update success on uncertain verification. Check
  the non-secret status and repeat provisioning before returning the tag to
  service. Masked PWD/PACK cannot be re-proven by I2C readback after an RF edge.
- Interrupt provisioning before confirmation, reboot, and check that firmware
  refuses normal NFC updates without a verified receipt. Repeat provisioning.
  Do not claim physical torn-write coverage from host fault-injection tests.
- A URL update previews the candidate QR with settings unchanged. Only a
  successful display submission and guarded NFC verification permit the final
  settings write. Failure or timeout restores the old QR/mode, keeps the durable
  URL and attempts guarded tag rollback if tag work began. A late acknowledgement
  cannot commit a cancelled request. Check that a display error, a consumed
  request timeout and a tag failure leave the old URL after reboot; these cases
  have software regressions, but physical fault-injection acceptance is pending.
  Failure is reported, and status retains the last read/verified NFC URL when
  available. Failed rollback or power loss between the devices' writes can
  leave the external tag with old, new, empty or incomplete data: settings and
  EEPROM are not one physical atomic transaction. Keep it out of service until
  recovered. Restart uses the persisted URL. A clean
  restart reconciles a retained trusted receipt; a lost receipt requires fresh
  provisioning. A factory reset erases the receipt but leaves tag EEPROM.

The DK shares its 400 kHz I2C1 bus with the IMU. The service and bring-up IMU
transactions use the same mutex, with STOP-separated guarded tag frames.
The driver's deferred IMU interrupt-status reads use that mutex too.
The 500 microsecond FD-latency margin is unmeasured. Measure FD latency and
SCL high/low periods before claiming errata timing compliance. The DK has no
tag power switch and cannot perform the Rev A VCC-reset recovery sequence.
For a stuck/busy tag, remove reader fields, power-cycle the entire fixture,
then provision again. Address corruption outside `02h`/`55h` needs a controlled
recovery fixture; firmware does not scan arbitrary addresses on the IMU bus.

## Restore the normal image and record evidence

After provisioning and debugging, restore a normal build with the mailbox off:

```powershell
.\scripts\dev.ps1 build -Pristine
west flash -d build/nrf5340dk --runner jlink
```

Keep the existing NVS settings when flashing. Keep phones away at boot. Repeat
powered/passive reads and the P0 control checks on this final image. Record
firmware revision and merged-image SHA-256, phone/OS/reader versions, decoded
QR and NFC URLs for both destinations, powered/off outcomes, non-secret RTT
status, protection attempts, failures and recovery. Do not collect or retain
credential-bearing logs or dumps. Hardware acceptance and risks R005/R023
remain open until those results exist.
