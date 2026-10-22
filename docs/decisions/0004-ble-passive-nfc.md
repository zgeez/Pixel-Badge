# 0004: BLE configuration and external passive NFC

Status: Accepted

Date: 2026-10-04

## Decision

Configure over BLE and keep the contact URL in an external NTAG I²C plus.
The MCU updates EEPROM while powered; the tag supplies the same URI passively
when the badge is off. Native MCU NFC alone does not provide this independent
stored-content path.

Keep RF reads public, protect RF writes, and preserve MCU/I²C updates. Include
metadata/configuration in protection tests. QR, NFC, and app readback share one
URL contract, including failure and interrupted-update recovery.

The extra tag and coil add tuning and synchronization work. Test the complete
assembly with its battery, magnets, and carrier.
[NFC design](../nfc.md) · [Wireless tests](../testing/wireless.md)
