# NFC setup and checks

The DK fixture uses an NTAG I²C plus breakout on the shared IMU bus.
Ordinary boot, **Apply link**, and **Sync again** check the tag's protection
policy and URL locally. A phone verifier is not required.

## Set up a fresh tag

Run from the repository root with NCS 3.4.1 and its Python environment loaded.
Use the matching ELF, keep RF readers away, and back up settings before flashing.
This helper targets the nRF5340 DK, not Rev A.

```powershell
.\scripts\dev.ps1 build -Image Prototype -BuildDir build/nfc-setup -ExtraConfFile firmware/nrf5340_prototype/configs/nfc-provisioning.conf -Pristine
.\scripts\dev.ps1 flash -Image Prototype -BuildDir build/nfc-setup
python scripts/nfc/badge_nfc.py configure --elf build/nfc-setup/nrf5340_prototype/zephyr/zephyr.elf
python scripts/nfc/badge_nfc.py status --elf build/nfc-setup/nrf5340_prototype/zephyr/zephyr.elf
```

`configure` generates temporary credentials and verifies readable policy/NDEF.
It does not test RF password authentication. Expected status is `result: 0`,
ready, address `2`, and matching desired/verified URLs. Restore the normal
Prototype image afterwards to remove the setup mailbox.

## Bench checks

1. Apply a short URL in Pixel Badge. Decode the QR and read the actual NFC URI;
   compare bytes, preserving path case.
2. Remove every power source from the DK and tag. Read the same URI passively.
3. Restore power and repeat with a second URL.
4. In a fresh, unauthenticated RF session, try changing the NDEF URL. The write
   must fail; public reads and a subsequent MCU update must still work.
5. Test RF-field changes during a URL update. Errors must not appear as a
   successful Apply. Reconnect/Refresh and check both QR and NFC after recovery.

Use a raw Type 2 reader for metadata/configuration bypass tests. An ordinary
phone tap proves reading only. Save the firmware/ELF hash, reader version,
non-secret status, and powered/passive outcomes.

## Recovery and limits

The setup address is `0x02`; factory address is `0x55`. An update can fail
between settings and tag writes, so check the persisted URL and external NDEF
after power loss. Keep a mismatched tag out of service until repaired.

The DK has no switched tag supply: remove readers and power-cycle the fixture
after bounded retries fail. Do not scan arbitrary I²C addresses on the IMU bus.
Rev A coil range, field-detect timing, and switched-supply recovery remain untested.

The optional two-session RF diagnostic uses `provision` and the Android helper;
see [NFC tools](../scripts/nfc/README.md). It predates local synchronization.
[Protection policy](nfc.md) · [Test plan](testing/wireless.md#6-nfc) ·
[Recorded provisioning result](../validation/results/2026-10-12-dk-positive-nfc-proof/README.md)
