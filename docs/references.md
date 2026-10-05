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
| BQ25620 (selected charger) | Switching charger, NVDC power path, BC1.2 detection (Tables 8-1/8-2), POR defaults (§8.6), TS/JEITA network (§8.3.5.4.2), inductor/capacitor selection (§9.2.2), RYK0018A land pattern (4226526/A), I²C address 0x6B | [TI BQ25620/BQ25622, SLUSEG2D](https://www.ti.com/lit/ds/symlink/bq25620.pdf) | Rev.D, February 2025 |
| BQ24074 (previously selected; replaced) | Linear-charger analysis kept for reference: K_ISET, thermal Eq. 11 | [TI BQ2407x, SLUS810N](https://www.ti.com/lit/gpn/bq24074) | Rev.N, October 2021 |
| NT3H2111 (selected: NT3H2111W0FHKH, XQFN-8) | Passive NFC, I²C NDEF persistence and RF-write protection | [NXP NT3H2111_2211](https://www.nxp.com/docs/en/data-sheet/NT3H2111_2211.pdf), §8.3.11/Tables 9–10 (pp.26–29), §8.7 (pp.38–39), §10.8; memory maps §§8.3.2–8.3.5 | Rev.3.6, 2023-07-21 |
| NT3H2111 / NT3H2211 errata | RF/I²C corruption, field-detect/timing, verification, configuration locks and recovery | [NXP ES_NT3H2111_2211](https://www.nxp.com/docs/en/errata/ES_NT3H2111_2211.pdf), §§2–4 | Rev.4.1, 2025-10-30 |
| NTAG antenna design | Coil target frequency, capacitance budget, rectangular-coil inductance formula, turn exponent | NXP AN11276 NTAG Antenna Design Guide, §§4.2–4.3, Table 5 (NXP-authored; retrieved from the [NXP Community attachment](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/nfc/10167/1/AN11276.pdf), SHA-256 `ce97eea73d4517fc5d9caed533aba2e35a2d25ca6f07b315bd40309bf5d77d2d`) | Rev.1.8, 2018-10-23 |
| TPS63802 (selected logic regulator) | 3.3 V buck-boost; pin functions Table 7-1; FB setting §10.2.2.5; inductor selection §10.2.2.2; DLA0010A package and land pattern (drawing 4223750/D) | [TI TPS63802, SLVSEU9D](https://www.ti.com/lit/ds/symlink/tps63802.pdf) | Rev.D, January 2021 |
| TPS61023 (selected LED boost) | VLED boost with load disconnect; VREF/OVP/EN in §6.5; FB §8.2.2.1; inductor §8.2.2.2; feed-forward §8.2.2.4; DRL0006A land pattern (4223266/F) | [TI TPS61023, SLVSF14B](https://www.ti.com/lit/ds/symlink/tps61023.pdf) | Rev.B, August 2020 |
| XEL4030-102ME (TPS61023 inductor) | 1.0 µH, DCR 9.78 mΩ max, Isat 9.0 A, Irms 8.4 A (20 °C rise); recommended land pattern | Coilcraft XEL4030, Documents 1321-1 to 1321-3 | Revised 02/19/26 |
| DFE201612E-R47M=P2 (TPS63802 inductor) | 0.47 µH ±20%, DCR 26 mΩ max, 5.5 A (−30% L), 4.5 A (40 °C rise) | Murata reference specification J(E)TE243A-0006B-01 | — |
| ICM-42688-P (conditional IMU) | Pinout (Table 9), I²C address and 1 MHz limit, I²C driver settings §12.3, INT_ASYNC_RESET §12.6, registers §§13–15 | TDK InvenSense DS-000347 | Rev.1.2 |
| AO3401A / AO3400A | Row and VBAT high-side switch; VBAT and CC-sense N-MOSFETs; IDSS, VGS(th), RDS(on), SOT-23 pinout | Alpha & Omega AO3401A and AO3400A datasheets | Rev.3.1 (December 2023 / July 2023) |
| CM7V-T1A 32.768 kHz, 9 pF (selected crystal) | CL, RS, C0, drive level against the Raytac crystal specification | Micro Crystal CM7V-T1A datasheet | Version 8.0, 10.2016 |
| Adafruit 328 / PKCELL LIPO785060 (selected pack) | Capacity, charge and discharge limits, temperature ranges, dimensions, plug | [Adafruit product 328](https://www.adafruit.com/product/328); [PKCELL LIPO785060 2500 mAh specification](https://cdn-shop.adafruit.com/product-files/328/LP785060+2500mAh+3.7V+20190510.pdf) | Valid 2019-05-10 (accessed 2026-10-05) |
| Ricoh R5478N101CD (pack protection IC) | Over-charge/over-discharge/excess-current thresholds and delays | Ricoh R5475N/R5478N datasheet EA-291-121009 (selection guide) | EA-291-121009 |
| NCP15XH103F03RC (board NTC) | 10 kΩ, B25/50 3380 K (B25/85 3434 K), 103AT-class | Murata NCP series | — |
| BQ27427 (comparison only; not selected) | Representative capacity/state-of-charge gauge with chemistry selection | [TI BQ27427, SLUSEB5B](https://www.ti.com/lit/ds/symlink/bq27427.pdf), features, §6.4 device modes and §7 application | Rev.B, September 2025 |

The pack's protection-board trip current, delays and recovery are not in the
documents above; record them if the vendor provides them. Do not assign
electrical values from targets.
