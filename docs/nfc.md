# Passive NFC content and provisioning

Status: **design decided; no hardware/security test results**. Part:
**NXP NT3H2111W0FTT** (1 kB, TSSOP8; availability to confirm); NT3H2211W0FTT
(2 kB) is the alternate. The configured contact URL is
stored as NDEF in EEPROM, publicly readable while powered or entirely unpowered,
and updateable by the MCU over I²C. Unauthenticated RF devices must not overwrite
that URL or alter its interpretation through metadata/configuration writes.

## Protection policy

NXP **NT3H2111/NT3H2211 Rev.3.6 (2023-07-21)**, §8.3.11/Table 10 and §8.7,
states that default `AUTH0 = FFh` disables password protection. The intended
provisioning policy is:

- Enable password protection with an `AUTH0` boundary covering the complete
  contact NDEF/TLV/URI region; leave `NFC_PROT = 0b` for password-protected RF
  writes with public RF reads.
- Retain `I2C_PROT = 00b` so the MCU can read/write user EEPROM over I²C without
  RF password authentication. Do not permanently lock the updateable URL data.
- Review the capability container, lock bytes and configuration as well as the
  payload. Protect metadata that could redirect/hide the NDEF or disable the
  policy; inspect protection exclusions rather than assuming AUTH0 covers all
  memory. Dynamic lock bits, session registers and mirrored SRAM are excluded
  from password protection. Keep the contact URL out of SRAM mirroring and
  pass-through, and prevent RF configuration changes from enabling a bypass.
- Memory map (Sector 0, §8.3.1/§8.3.9/Table 8): CC at page 03h initialized to
  `E1 10 6D 00`; NDEF TLV from page 04h; user memory pages 04h–E1h (888 bytes)
  holds the URI. The 1 kB part has no Sector 1. If the 2 kB alternate is used,
  set `NFC_DIS_SEC1 = 1b`.
- Register values: `AUTH0 = 04h`, the first user page, so user memory and the
  configuration pages (04h–EBh) are write-protected from NFC (§8.3.11, Table 9);
  `ACCESS`: `NFC_PROT = 0b`, `AUTHLIM = 000b`; `PT_I2C`: `I2C_PROT = 00b`,
  `SRAM_PROT = 1b`, `2K_PROT = 0b`; pass-through and SRAM mirroring off.
  `AUTHLIM = 000b` avoids a failed-attempt counter that could make the protected
  area permanently inaccessible (§8.7.2).
- The capability container (page 03h) is **not** covered by AUTH0: Tables 4–5
  list it as read/write outside the protected range, and an NFC device can set
  CC bits (for example its read-access or size fields) without the password
  (§8.3.8). Provisioning therefore locks it with the static lock bits for page
  03h, `L_CC` and its block-locking bit `BL_CC` (static lock byte 0, written over
  I²C in block 00h bytes 10–11; §8.3.6, Figure 8). After that, NFC cannot change
  the CC at all, so passive reads stay correct even if the badge is never
  powered again; the MCU can still reset the lock bits over I²C if the CC ever
  has to change. Record the exact lock-byte value on a development tag first.
- Other static lock bits (pages 04h–0Fh) and the dynamic lock bits (excluded
  from password protection) can still be set by an NFC device. They only make
  pages read-only to NFC; public reads are unaffected. They are irreversible from
  NFC but resettable over I²C (§8.3.6, §8.3.7). The MCU checks and clears them on
  each update, and verification confirms that I²C URL updates succeed while
  they are set. Session registers are read-only via NFC (§8.3.12).
- Configure the I²C address byte to `04h` (§8.3.8 remark): byte 0 of block 00h
  always reads back as 04h, so read-modify-write of that block (lock bytes, CC)
  cannot then change the address.
- Provision an unpredictable, non-default, per-device password through a
  controlled local process. The provisioning tool generates PWD/PACK with a
  cryptographic random-number generator and passes them to the MCU over the
  debug link for the I²C write. PWD and PACK always read back as zero
  (§8.3.11), and an RF transition can skip or corrupt an I²C write (errata
  §2.1.2), so a read-back cannot prove they were programmed. Before the
  credential is discarded and before any `REG_LOCK`, a fresh RF session must
  authenticate successfully with the generated PWD (checking the returned
  PACK) and a second fresh session must show that the factory-default PWD
  (`FFFFFFFFh`, Table 10) is rejected. The credential exists only transiently on
  the provisioning host; the MCU does not need it afterwards because it updates
  the tag over I²C. Never store, log, or commit credentials, credential-bearing
  register dumps, or provisioning logs. This password mechanism is not
  cryptographic anti-cloning protection; MCU/I²C access is trusted within the
  badge.

NXP §8.7.2 warns that failed authentication attempts can permanently lock the
protected access mode. Do not choose retry limits or blanket lock bits without
checking public-read availability, MCU updates and the recovery procedure.
RF password authentication is not part of ordinary contact reading or MCU URL
updates.

## Provisioning and recovery gate

Hardware (errata Rev.4.1 §2.1, §3, §4): the tag has its **own I²C bus**
(100 kHz) and its VCC, bus pull-ups and field-detect pull-up come from an MCU
GPIO. The tag is powered only while firmware updates or verifies it; NFC reads
are passive either way. This keeps the power-up SCL/SDA glitch (§3) off the IMU
bus, lets firmware perform the VCC reset in §4's recovery order, and limits RF
transitions during I²C (§2.1) to update windows. VOUT (energy harvesting) is
unused.

Every I²C transaction follows the errata's procedure (§2.1.1.3.1–2.1.1.3.4):

1. Configure field detect to signal RF field on and off, enable a GPIO
   interrupt on both FD edges, and clear its latched flag.
2. Wait 20 µs plus the measured FD latency.
3. Run one I²C frame with START and STOP (no repeated start); separate frames by
   at least 50 µs.
4. Wait 20 µs plus the FD latency again, then disable and read the latched
   flag.
5. If any RF transition was latched, discard the result and retry the frame
   (bounded count). A latched transition during a **read** may have caused an
   unintended EEPROM write (§2.1.1.2), so the MCU then re-verifies the I²C
   address, configuration and NDEF blocks before continuing.
6. After the retry limit: VCC reset through the tag's supply GPIO, then ask the
   user to remove the phone (RF reset), per §4.3's order.

FD latency depends on the FD pull-up and decoupling values (§2.1.1.3.4); measure
it on the Rev A board and use the measured worst case in steps 2 and 4.

Every I²C EEPROM write follows NXP's write procedure (errata §2.3.1): once the
write starts (the Stop condition, datasheet §9.2), start a 5 ms hardware timer
and poll EEPROM_WR_BUSY in NS_REG until it clears; a timeout is a write error.
Send no other EEPROM command, read-back, or tag power-off until the flag has
cleared, because a command sent during the programming cycle can corrupt the
memory (datasheet §9.2). Then verify the write by reading it back and comparing
(errata §2.3.6).

Before provisioning any badge, review the ordered configuration writes and
verify them on a development tag: set the I²C address byte, write the CC and
NDEF region, then the access policy and credentials, enable protection with
AUTH0, verify the generated password by RF authentication (and rejection of the
factory default) as described above, and set the CC static lock bits last among
the reversible steps. Do this in a controlled RF environment. Verify public
reads, rejected RF writes (including CC and lock-bit attempts) and subsequent
MCU updates; apply any reviewed irreversible configuration locks only after
that. Record the non-secret settings and result, not the credentials. A failed
or interrupted provisioning run stays out of service until the policy is
verified again.

Also apply **NXP ES_NT3H2111_2211 errata Rev.4.1 (2025-10-30)** §§2–4. RF
transitions during I²C can corrupt transactions, EEPROM/configuration and even
the I²C address. The host needs the specified transaction timing, field-detect
monitoring, write/read verification and bounded retries; an ACK alone is not
proof of a successful update. Errata §2.3.5 recommends `REG_LOCK = 03h` as the
last personalization step. It permanently freezes the configuration (including
the password/access bytes) from both interfaces but not I²C user-memory updates.
Apply it only on the event build after all §6 tests pass; development boards
stay unlocked.

On failure, preserve/report the last verified URL, reconcile EEPROM with the MCU
configuration on restart, and withhold update success until QR/NFC agree. Use
errata §4's retry → VCC reset → RF reset recovery order where applicable;
document the operator step when the reader field cannot be removed by firmware.
Address recovery under §2.2 needs a controlled fixture; the tag's dedicated bus
keeps that procedure away from the IMU. Replace/reprovision a tag whose required
policy cannot be restored. No unlocked-tag fallback satisfies the RF-write
requirement.

Required evidence: [validation §6](validation.md#6-nfc). Primary document links
and versions: [references.md](references.md#component-datasheets).
