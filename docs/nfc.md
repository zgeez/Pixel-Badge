# Passive NFC content and provisioning

Status: **design requirements; exact NTAG variant and provisioning map remain
provisional; no hardware/security test results**. The configured contact URL is
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
- Resolve the 1k/2k memory map and, for a 2k variant, Sector 1 access/protection
  separately. The complete address map, AUTH0 boundary, sector settings and
  combined register bytes remain TBD until the exact part/layout is selected.
- Provision an unpredictable, non-default, per-device password through a
  controlled local process. Never commit credentials, credential-bearing register dumps, or
  provisioning logs. This password mechanism is not cryptographic anti-cloning
  protection; MCU/I²C access is trusted within the badge.

Review `AUTHLIM` and irreversible register/lock settings with the final map.
NXP §8.7.2 warns that failed authentication attempts can permanently lock the
protected access mode. Do not choose retry limits or blanket lock bits without
checking public-read availability, MCU updates and the recovery procedure.
RF password authentication is not part of ordinary contact reading or MCU URL
updates.

## Provisioning and recovery gate

Before schematic freeze, select/review the exact part's memory/access map and
the host's field-detect connection. Before provisioning any badge, review the
ordered configuration writes and verify them on a development tag: configure
the NDEF region, access policy and credentials in a controlled RF environment;
enable protection; verify public reads, rejected RF writes and subsequent MCU
updates; apply any reviewed irreversible configuration locks last. Record the
non-secret settings and result, not the credentials. A failed/interrupted
provisioning run stays out of service until the policy is verified again.

Also apply **NXP ES_NT3H2111_2211 errata Rev.4.1 (2025-10-30)** §§2–4. RF
transitions during I²C can corrupt transactions, EEPROM/configuration and even
the I²C address. The host needs the specified transaction timing, field-detect
monitoring, write/read verification and bounded retries; an ACK alone is not
proof of a successful update. Review §2.3.5's final configuration-lock
recommendation separately from user-memory locks, with its irreversible effect
and continued I²C URL updates checked before application.

On failure, preserve/report the last verified URL, reconcile EEPROM with the
MCU configuration on restart, and withhold update success until QR/NFC agree.
Use errata §4's retry → VCC reset → RF reset recovery order where applicable;
document the operator step when the reader field cannot be removed by firmware.
Address recovery under §2.2 needs a controlled fixture: do not scan/write
arbitrary targets on the shared IMU bus. Replace/reprovision a tag whose required
policy cannot be restored. No unlocked-tag fallback satisfies the RF-write
requirement.

Required evidence: [validation §6](validation.md#6-nfc). Primary document links
and versions: [references.md](references.md#component-datasheets).
