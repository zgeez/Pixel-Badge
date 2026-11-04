# NFC

Rev A uses an NXP NT3H2111 (NTAG I²C plus, 1 kB) with a coil on the PCB. The
link lives in the tag's EEPROM, so phones can read it with the badge off. The
MCU updates it over I²C. The DK uses a breakout board instead.

## Protection

Normal boot only checks the tag's policy and link; it never creates
credentials. A new tag needs one wired setup ([how](nfc-prototype.md)).

| Setting | Value |
|---|---|
| CC, page `03h` | `E1 10 6D 00`, static CC locks `09 00` |
| NDEF | URI record from page `04h`; user pages `04h–E1h` |
| `AUTH0` | `04h`: RF writes to user and config pages need the password |
| `ACCESS` | `NFC_PROT=0` (reads stay public), `AUTHLIM=000` |
| `PT_I2C` | `I2C_PROT=00`, `SRAM_PROT=1`, `2K_PROT=0` |
| Mirroring, pass-through | Off |
| I²C address | `02h` (factory `55h`) |

The password stops casual rewriting from a phone. It isn't anti-cloning.
Credentials never appear in logs, dumps or the repository. Nothing writes the
irreversible `REG_LOCK`, so development tags stay unlockable.

## Safe writes

The [datasheet](https://www.nxp.com/docs/en/data-sheet/NT3H2111_2211.pdf) and
[errata](https://www.nxp.com/docs/en/errata/ES_NT3H2111_2211.pdf) warn that
an RF field arriving during I²C access can corrupt a transfer. So the driver:

1. Latches field-detect edges and guards each I²C frame (20 µs plus FD latency).
2. Separates frames by at least 50 µs and drops any frame an RF edge touched.
3. Polls `EEPROM_WR_BUSY` after each write, within 5 ms, then reads back.
4. On `EEPROM_WR_ERR`, reports failure, clears that bit and checks the clear.
5. Retries a bounded number of times, then resets VCC, then asks for the
   phone to be taken away.

A link change only commits once the display shows the new QR and the tag
reads back correctly. On failure it restores the display and rolls the tag
back. The settings and the tag can't be written atomically, so after a power
cut the badge checks both against the saved link at boot.

## Code

`services/ntag.cpp` holds the transaction logic, and
`platform/nfc_transport.cpp` handles GPIO, timers and bus locking. An
unchanged link causes no EEPROM write. The setup mailbox only exists in setup
builds.

On Rev A the tag has its own 100 kHz bus and GPIO-switched supply. VCC stays
on once the transport starts; cycling it dynamically isn't done yet.

Still to test: read range in the carrier, field-detect timing, and RF-write
rejection on Rev A. [NFC tests](testing/wireless.md#nfc).
