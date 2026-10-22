# ADR 0012: Prototype settings, QR encoder and boot layout

Status: Accepted 2026-10-12; artwork allocation amended by [0019](0019-large-artwork-fixed-workspaces.md)

Date: 2026-10-11

## Decision

Vendor Nayuki's MIT C QR generator v1.8.0 with its license and source hashes.
Caller-owned buffers avoid heap allocation. Initially restrict output to
Version 1-M with a full quiet zone; later firmware selects the strongest
fitting correction level.

Use Zephyr settings/NVS with data CRC. Commit each validated field separately;
there is no general multi-field atomic transaction. Unsupported schemas load
defaults; recovery writes all defaults before publishing the schema version.

Use MCUboot/sysbuild with the pinned SDK's Partition Manager and static layout.
Flash the merged bootloader/application output. The SDK development signing
key is not a production trust root; update transport/key provisioning remain
separate work.

[QR library and notices](../../firmware/common/lib/qrcodegen/README.md) ·
[Storage layout](../rev-a-firmware.md#flash-and-ram) ·
[Initial bench evidence](../validation.md#result-prototype-foundation-on-the-dk)
