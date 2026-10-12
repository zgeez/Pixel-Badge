# ADR 0012: Prototype settings, QR encoder and boot layout

- Status: Proposed
- Date: 2026-10-11

## Context

The DK badge needs persistent, validated configuration and runtime QR/name
modes before NFC and BLE integration. Memory must remain bounded, and MCUboot
needs a stable layout that leaves space for future radio-core updates.

## Decision

Vendor Project Nayuki's MIT-licensed C QR Code generator at tag v1.8.0,
including its license, original source hashes and upstream link. The C API
uses caller-owned buffers and no heap. A separate library target preserves
upstream source and isolates it from application warning/formatting rules.
The application wrapper allows only Version 1-M, without ECC boosting,
with a full quiet zone on the 32 × 32 frame. Its automatic encoder uses
alphanumeric encoding when possible and byte encoding otherwise. Reject
non-fitting input before replacing the previous setting or frame.

Use Zephyr settings with NVS and data CRC. The nRF5340's erase-based internal
flash fits NVS, whose data-before-metadata ordering supports recovery from
an interrupted field write. Each setting is one record; no multi-field atomic
commit is claimed. ZMS is an alternative for other memory types, but offers
no required capability for this bounded, small configuration on this target.
The application stores neither Bluetooth keys nor app keys in P0; flash space
is reserved for them.

Enable MCUboot with sysbuild on both application-core board targets and
explicitly enable the still-supported Partition Manager in pinned NCS v3.4.1.
Use the committed `firmware/pm_static.yml` layout. PM is deprecated in this
SDK; its deprecation diagnostics remain enabled. Migration to the SDK's
default devicetree partitioning requires a later layout-preserving change.
The bootloader uses the SDK's publicly distributed development signing key;
P0 supplies no production key or secure deployment claim. Network-core DFU,
transport and key provisioning remain future work.

## Consequences

Settings fields and URL bytes are portable between boards. Unsupported schema
versions load immutable defaults. A failed write leaves the current RAM value
unchanged. Recovery writes all application defaults before publishing version
1, then applies the requested edit, preventing old records from reappearing.
The shared flash layout does not introduce a Rev A display driver.
Owners must flash the merged bootloader/application image. Host tests check
independent Segno fixtures, validation, UI logic and interrupted NVS writes;
the [initial bench result](../validation.md#result-prototype-foundation-on-the-dk)
records an owner-reported functional pass and narrower direct RTT evidence.
Display/schema recovery corrections still require a new bench run.

## Sources

- [Project Nayuki library](https://www.nayuki.io/page/qr-code-generator-library)
- [Pinned C implementation and license](https://github.com/nayuki/QR-Code-generator/tree/v1.8.0/c)
- [Bootloader partitioning at the pinned NCS v3.4.1 revision](https://github.com/nrfconnect/sdk-nrf/blob/b20f8619ba9a5530f8c34b0a130d829947cfe55d/doc/nrf/app_dev/bootloaders_dfu/mcuboot_nsib/bootloader_partitioning.rst)
- [Partition Manager source at the pinned NCS v3.4.1 revision](https://github.com/nrfconnect/sdk-nrf/blob/b20f8619ba9a5530f8c34b0a130d829947cfe55d/cmake/sysbuild/partition_manager.cmake)
- [NVS documentation at NCS v3.4.1's Zephyr revision](https://github.com/nrfconnect/sdk-zephyr/blob/33fa6a7aac6a4401d16a67cb9f27a3483fa02dd6/doc/services/storage/nvs/nvs.rst)
