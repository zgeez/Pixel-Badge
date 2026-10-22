# Passive NFC

Rev A uses an NXP NT3H2111W0FHKH: NTAG I²C plus 1 kB with a PCB coil.
The URL lives in EEPROM, remains publicly readable without power, and is
updated by the MCU over I²C. The DK uses an external breakout.

## Protection policy

Current firmware checks policy and URL locally; normal boot never generates
credentials. Fresh tags use explicit wired setup. See [setup/checks](nfc-prototype.md).

| Setting | Value / purpose |
|---|---|
| CC, page `03h` | `E1 10 6D 00`; static CC locks `09 00` (`L_CC` + `BL_CC`) |
| NDEF | URI TLV from page `04h`; user pages `04h–E1h` |
| `AUTH0` | `04h`, covering user/configuration writes from RF |
| `ACCESS` | `NFC_PROT=0`, public reads; `AUTHLIM=000`, no permanent attempt lockout |
| `PT_I2C` | `I2C_PROT=00`, `SRAM_PROT=1`, `2K_PROT=0` |
| Mirroring/pass-through | Off |
| Address byte | `04h`, giving seven-bit I²C address `02h`; factory address is `55h` |

AUTH0 does not cover the CC or dynamic lock bits. CC locks protect public
interpretation of the message; MCU updates clear other RF lock bits while
retaining those CC locks. Session registers are read-only over RF.

PWD/PACK readback is masked, so local checks cannot prove RF authentication.
The optional two-session diagnostic in [ADR 0014](decisions/0014-positive-nfc-provisioning-proof.md)
tests generated-PWD/PACK authentication. Unauthenticated payload/metadata-write
rejection needs separate tests. The password mechanism is not anti-cloning.
Credentials stay transient and out of logs, dumps, and source control.
No irreversible `REG_LOCK` write is implemented; development tags stay unlocked.

## Provisioning and recovery gate

The NXP [datasheet](https://www.nxp.com/docs/en/data-sheet/NT3H2111_2211.pdf)
and [errata Rev. 4.1](https://www.nxp.com/docs/en/errata/ES_NT3H2111_2211.pdf)
define the transaction sequence. RF edges during I²C can corrupt reads, writes,
configuration, or the address.

1. Latch both field-detect edges. Guard each frame by 20 µs plus FD latency.
2. Use START/STOP-separated frames, at least 50 µs apart. Reject a frame if an
   RF edge occurred; recheck address, policy, and NDEF before continuing.
3. After EEPROM STOP, poll `EEPROM_WR_BUSY` within a fixed 5 ms hardware deadline.
   Send no further EEPROM command or power-off until completion; then read back.
4. On `EEPROM_WR_ERR`, report failure, clear only that bit, and verify the clear.
   Unconfirmed completion/clear blocks further EEPROM commands.
5. Bound retries. Recovery order is retries, VCC reset, then RF-field removal.

The transport accepts FD margins of 0–500 µs; the current 500 µs value is
unmeasured. Poll scheduling must fit the original 5 ms deadline. A larger
measured FD latency needs a transport change, not a relaxed write deadline.

Setup writes address, CC/NDEF, credentials, and access policy, then verifies
readable state and applies reversible CC locks. A failed run stays out of
service. Apply commits settings only after display submission and guarded tag
verification; failure restores the display and attempts tag rollback. Settings
and external EEPROM are not a single atomic transaction: check both after
interruption and reconcile with the persisted URL on restart.

## DK implementation and limits

`services/ntag.cpp` owns the portable transaction engine;
`platform/nfc_transport.cpp` supplies GPIO/timers and bus locking. The NFC
worker coordinates display confirmation and link updates. Unchanged URLs avoid
EEPROM writes. The setup mailbox is absent from normal builds.

Rev A has a dedicated 100 kHz bus and GPIO-powered VCC/pull-ups. The current
implementation leaves VCC on after transport preparation; dynamic power cycling
is unfinished. Probe C1101/R1101/R1102/R1103 for VCC/SCL/SDA/FD.
The DK shares its 400 kHz IMU bus and has no switched supply; recovery may need
reader removal and a whole-fixture power cycle.

FD timing, coil range with the carrier, RF-write rejection, and recovery remain
bench work. The URI encoder implements the minimal AN11786 TLV initialization;
complete NFC Forum Type 2 mapping conformance is not claimed.
[NFC test plan](testing/wireless.md#6-nfc) · [Sources](references.md#component-datasheets)
