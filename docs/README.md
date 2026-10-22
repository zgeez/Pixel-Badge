# Documentation

| Read about | Guide |
|---|---|
| How the system fits together | [System overview](architecture.md) |
| Firmware setup, controls, and tests | [Firmware](firmware.md) |
| nRF52840 resources, timing, and power policy | [Rev A firmware](rev-a-firmware.md) |
| PCB and component choices | [Hardware](hardware.md) |
| Phone app and development setup | [Pixel Badge app](../app/README.md) |
| Measurements and remaining work | [Validation](validation.md) |
| First power-on and staged testing | [Board bring-up](bringup.md) |

## Technical references

| Interface | Reference |
|---|---|
| Configuration and artwork | [BLE protocol](ble-protocol.md) |
| Recording transport | [Audio protocol](audio-protocol.md) |
| Power path, USB, and battery sensing | [Power design](power.md) |
| Tag protection and provisioning | [NFC design](nfc.md), [setup tools](../scripts/nfc/README.md) |
| DK development tests | [BLE](ble-bench.md), [NFC](nfc-prototype.md), [audio](audio-prototype.md), [motion](motion-prototype.md) |
| Custom footprints and clearance models | [Library provenance](../hardware/libraries/README.md) |
| Design rationale and source documents | [Decision records](decisions/README.md), [references](references.md) |

Dated records in `validation/results/` describe the firmware and setup tested
at that time. Use the current guides for today's builds and behavior.
