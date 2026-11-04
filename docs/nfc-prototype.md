# NFC setup and checks

Normal boot, **Apply link** and **Sync again** check the tag's protection and
link locally. No phone verifier is needed. A new tag needs one wired setup.

## Set up a new tag

Run from the repository root with the NCS 3.4.1 Python environment loaded.
Keep phones away from the tag. Back up settings first if they matter.

On the DK:

```powershell
.\scripts\dev.ps1 build -Image Prototype -BuildDir build/nfc-setup -ExtraConfFile firmware/nrf5340_prototype/configs/nfc-provisioning.conf -Pristine
.\scripts\dev.ps1 flash -Image Prototype -BuildDir build/nfc-setup
python scripts/nfc/badge_nfc.py configure --elf build/nfc-setup/nrf5340_prototype/zephyr/zephyr.elf
python scripts/nfc/badge_nfc.py status --elf build/nfc-setup/nrf5340_prototype/zephyr/zephyr.elf
```

On Rev A, build Main with `firmware/rev_a_nrf52840/configs/nfc-provisioning.conf`
and add `--target rev-a` ([NFC tools](../scripts/nfc/README.md)). A new board's
tag fails the boot sync with `NFC boot result -13` until `configure` has run.

`status` should show `result: 0`, ready, address `2`, and matching desired and
verified links. Reflash the normal image afterwards to remove the setup
mailbox.

## Checks

1. Apply a short link in the app. Scan the QR code and read the NFC URI, and
   compare them byte for byte, path case included.
2. Remove all power and read the URI again.
3. Power up and repeat with a second link.
4. From a fresh, unauthenticated RF session, try to rewrite the URI. It must
   fail, and reads and the next MCU update must still work.
5. Move a phone in and out during a link update. An error must never show
   as a successful Apply. Refresh and check the QR and the tag afterwards.

A phone tap only proves reading. Use a raw Type 2 reader for write and
configuration tests.

## Recovery

The setup address is `0x02` (factory `0x55`). A power cut can land between
the settings write and the tag write, so check both afterwards. The DK can't
switch the tag's supply: if retries fail, take the phone away and power-cycle
the whole bench. Don't scan random I²C addresses on the shared IMU bus.

[Protection policy](nfc.md), [tests](testing/wireless.md#nfc),
[DK provisioning result](../validation/results/2026-10-12-dk-positive-nfc-proof/README.md).
