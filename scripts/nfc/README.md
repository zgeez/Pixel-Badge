# NFC tools

`badge_nfc.py` controls the development mailbox over J-Link SWD on the
**nRF5340 DK**. Run in the NCS Python environment with `pylink-square` and
`pyelftools` available. The supplied ELF must match the flashed setup image.
The helper does not target Rev A yet.

## Commands

```powershell
python scripts/nfc/badge_nfc.py configure --elf build/nfc-setup/nrf5340_prototype/zephyr/zephyr.elf
python scripts/nfc/badge_nfc.py status --elf build/nfc-setup/nrf5340_prototype/zephyr/zephyr.elf
```

`configure` is explicit wired setup without phone taps. `status` returns cached
non-secret state. `sync` checks the current URL; `set-url --url HTTPS://EXAMPLE.COM`
stages a URL change and verifies QR/NFC before committing settings.
[Build/flash and physical checks](../../docs/nfc-prototype.md).

## Optional RF diagnostic

`provision` adds the older two-session PWD/PACK check; it is not needed for
boot, Apply, or Sync again. Build/install the Android helper with existing
SDK/JDK tools:

```powershell
.\scripts\nfc\build_android.ps1 -Jdk 'C:/Program Files/Android/Android Studio/jbr'
adb -s PHONE_SERIAL install -r build/p1-nfc-helper/badge-nfc-helper.apk
python scripts/nfc/badge_nfc.py provision --elf build/nfc-setup/nrf5340_prototype/zephyr/zephyr.elf --android-serial PHONE_SERIAL
```

Replace `PHONE_SERIAL` using `adb devices` (USB or wireless ADB). Keep readers
away during setup. Authenticate once, remove the phone completely, arm check 2,
and authenticate again. Only after **Both RF checks passed** / **RF proof accepted**,
remove the field and press Enter to finalize. Both sessions use the generated
password and exact PACK; a transport error never proves rejection. The host
waits 100 s; firmware finalization expires after 120 s. Restore normal firmware
afterwards to remove the mailbox.

## Interface and cleanup

Mailbox v4 is a 132-byte ELF symbol, `badge_nfc_mailbox`; it supports 25-byte
URLs plus NUL. Commands are status=1, set-url=2, begin=3, RF confirm=4,
sync=5, local confirm=6. RF proof is version 3/helper v0.5; do not mix versions.
See [source layout](badge_nfc.py) and [NFC policy](../../docs/nfc.md).

Credentials stay in transient buffers. Tools never print them; command cleanup
attempts to zero the mailbox even on failure. Unreachable SWD cannot guarantee
zeroization. No irreversible configuration lock is offered. Reader mode stays
active until the helper pauses, preventing ordinary URI dispatch from hiding
an error.

Host tests: `python scripts/nfc/test_badge_nfc.py`. Pure Java helper tests run
with `javac --release 8` / `java` as configured in CI.
