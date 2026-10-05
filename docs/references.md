# References

Authoritative sources for hardware and API claims. Prefer original
manufacturer documentation over secondary sources. When a design decision relies
on a specific document, cite the document revision and section.

## Nordic Semiconductor

- [nRF Connect SDK v3.4.1 documentation](https://nrfconnectdocs.nordicsemi.com/ncs/3.4.1/nrf/index.html)
- [Zephyr documentation as shipped with nRF Connect SDK v3.4.1](https://nrfconnectdocs.nordicsemi.com/ncs/3.4.1/zephyr/index.html)
- [nRF Connect SDK toolchain Docker image](https://nrfconnectdocs.nordicsemi.com/ncs/3.4.1/nrf/scripts/docker/README.html)
- [nRF5340 Product Specification](https://docs.nordicsemi.com/r/bundle/ps_nrf5340/page/keyfeatures_html5.html)
  (HTML tracks the latest revision; accessed 2026-10-04):
  [USBREG](https://docs.nordicsemi.com/r/bundle/ps_nrf5340/page/chapters/usbregulator/doc/usbregulator.html)
  for dedicated VBUS/main-supply requirements, decoupling, USB operating limits,
  and source resistance;
  [Reference circuitry](https://docs.nordicsemi.com/r/bundle/ps_nrf5340/page/chapters/ref_circuitry.html?contentId=VghuZK7iL3DXXXwg0apSEA)
  for supply configurations and recommended VBUS series conditioning.
- [nRF5340 SAADC](https://docs.nordicsemi.com/r/bundle/ps_nrf5340/page/saadc.html)
  (latest Product Specification HTML; accessed 2026-10-04), **Digital output**,
  **Reference**, **Acquisition time/Table 2** and **Electrical specification**:
  ADC errors/calibration, VSS–VDD analog-input limits, gain/reference and
  source-resistance/acquisition constraints for battery sensing.
- [nRF5340 DK Hardware user guide](https://docs.nordicsemi.com/bundle/ug_nrf5340_dk/page/ug/dk/intro.html)

## Raytac MDBT53-P1M

- [MDBT53-1M & MDBT53-P1M Specification, Ver.E](https://www.raytac.com/document/act.php?act=1&index_id=78),
  issued **2024-10-25**. §2.1 (pp.7–8): dimensions/pin orientation;
  §2.2 (pp.9–10): solder-pad layout; §2.3 (pp.11–14): antenna keep-out;
  §2.5 (pp.15–18): pinout, including VDD/VDDH and USB VBUS/D+/D−;
  §5: ratings; §8 (p.42): clock/reference-circuit notes;
  §8.1.2 (p.44): normal-voltage DC/DC reference circuit, including external
  VBUS conditioning and decoupling. Page numbers are the document's printed
  numbers. Release notes on p.71 identify the Ver.E changes.
- [MDBT53 / MDBT53-P / MDBT53-U Footprint & Design Guide, 241021](https://www.raytac.com/document/act.php?act=1&index_id=74),
  package dated **2024-10-21** (RAR archive). Use the **MDBT53-P** variant:
  `2D & 3D Drawing/MDBT53-P/`, the supplied footprint libraries,
  `53 Series Reference solder pad Layout-F_v6-230825v2.pdf`,
  `Recommended RF Layout/53 recommendended RF layout-F-241021.pdf`,
  and `Recommended 32.768KHZ Crystal Spec/` (crystal specification and Nordic
  guidance). The package also includes the reflow profile. Check crystal load
  configuration against the chosen crystal and firmware; an example part in
  the package is not a frozen Rev A BOM selection.

The manufacturer download endpoints above deliver the documents directly.
If Raytac replaces a file, locate the named revision via its
[module product downloads](https://www.raytac.com/tw/product/ins.php?index_id=103)
and [support index](https://www.raytac.com/document/). Downloads verified on
2026-10-04 had these SHA-256 hashes (retain the exact reviewed files in the
local design evidence; do not substitute a new revision silently):

| Document | SHA-256 |
|---|---|
| Ver.E PDF | `b10ea0cfdc962bf20122c0f91fb75d807f8feee2774d09086bf526208f93a217` |
| 241021 RAR | `aa0ffdbf622c99a0c9f68498f8f4eb870cc05d39fb83beda038e45ce71184220` |

## Zephyr RTOS

The nRF Connect SDK uses a Nordic fork of Zephyr; when upstream and SDK
documentation differ, the SDK-versioned documentation above applies.

- [C++ support](https://docs.zephyrproject.org/latest/develop/languages/cpp/index.html)
- [Logging](https://docs.zephyrproject.org/latest/services/logging/index.html)
- [Ztest](https://docs.zephyrproject.org/latest/develop/test/ztest.html)
- [Twister](https://docs.zephyrproject.org/latest/develop/test/twister.html)
- [native_sim board](https://docs.zephyrproject.org/latest/boards/native/native_sim/doc/index.html)

## Debug and logging

- [SEGGER Real-Time Transfer (RTT)](https://www.segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/)

## PCB design and manufacturing

- [KiCad 10.0 documentation](https://docs.kicad.org/10.0/)
- [JLCPCB PCB capabilities](https://jlcpcb.com/capabilities/pcb-capabilities)

## Standards

- USB-IF [USB Type-C Cable and Connector Specification Release 2.5](https://www.usb.org/document-library/usb-type-cr-cable-and-connector-specification-release-25),
  **March 2026** (published 2026-04-08),
  [official release ZIP](https://www.usb.org/sites/default/files/USB%20Type-C%202.5%20Release%20202603.zip).
  Use `USB Type-C Spec R2.5 - March 2026.pdf`:
  §4.5.1.3.2/Figure 4-9 (p.165) requires CC1 and CC2 sink terminations;
  §4.11.1/Table 4-28 (p.245) specifies 5.1 kΩ Rd and total-resistance tolerance;
  §4.6.2.1 defines current behavior and refers Default USB Power sinks to the
  applicable USB specification; Table 4-36 (p.255) gives sink CC voltage ranges
  and thresholds for current-advertisement detection; tSinkAdj (60 ms) bounds
  the sink's reaction to an advertisement change. Page numbers are printed numbers;
  accessed 2026-10-04. No USB PD functionality is selected.
- QR Code: ISO/IEC 18004 (QR Code bar code symbology specification)
- DENSO WAVE:
  [QR versions and dimensions](https://www.qrcode.com/en/about/version.html)
  (Version 1: 21×21; each version adds four modules per side) and
  [required quiet zone](https://www.qrcode.com/en/howto/code.html/index.html)
  (four modules on every side). Accessed 2026-10-04; supports the 29×29
  Version 1 versus 33×33 Version 2 display-fit calculation.

## Component datasheets

Record part number, manufacturer, document revision, and link. Sources for
provisional parts do not imply final part selection or hardware validation.

| Part | Function | Manufacturer document | Revision |
|---|---|---|---|
| BQ24074 (provisional charger) | Charge/power path; distinguish charger functions from independent cell protection | [TI BQ2407x, SLUS810N](https://www.ti.com/lit/gpn/bq24074), §§9.3.4–9.3.5; EN1/EN2 Table 7-2, EN logic levels and K_ILIM in §8 electrical characteristics | Rev.N, October 2021 |
| NT3H2111 (selected) / NT3H2211 (alternate) | Passive NFC, I²C NDEF persistence and RF-write protection | [NXP NT3H2111_2211](https://www.nxp.com/docs/en/data-sheet/NT3H2111_2211.pdf), §8.3.11/Tables 9–10 (pp.26–29), §8.7 (pp.38–39), §10.8; memory maps §§8.3.2–8.3.5 | Rev.3.6, 2023-07-21 |
| NT3H2111 / NT3H2211 errata | RF/I²C corruption, field-detect/timing, verification, configuration locks and recovery | [NXP ES_NT3H2111_2211](https://www.nxp.com/docs/en/errata/ES_NT3H2111_2211.pdf), §§2–4 | Rev.4.1, 2025-10-30 |
| NTAG antenna design | Coil target frequency, capacitance budget, rectangular-coil inductance formula, turn exponent | NXP AN11276 NTAG Antenna Design Guide, §§4.2–4.3, Table 5 (NXP-authored; retrieved from the [NXP Community attachment](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/nfc/10167/1/AN11276.pdf), SHA-256 `ce97eea73d4517fc5d9caed533aba2e35a2d25ca6f07b315bd40309bf5d77d2d`) | Rev.1.8, 2018-10-23 |
| BQ27427 (comparison only; not selected) | Representative capacity/state-of-charge gauge with chemistry selection | [TI BQ27427, SLUSEB5B](https://www.ti.com/lit/ds/symlink/bq27427.pdf), features, §6.4 device modes and §7 application | Rev.B, September 2025 |

Selected battery pack/cell and any dedicated protector documents: **TBD**.
Record their actual protection/current/temperature limits before schematic
freeze and wearable-use approval; do not assign electrical values from targets.
