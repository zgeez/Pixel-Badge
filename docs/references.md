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
| BQ24074 (provisional charger) | Charge/power path; distinguish charger functions from independent cell protection | [TI BQ2407x, SLUS810N](https://www.ti.com/lit/gpn/bq24074), §§9.3.4–9.3.5 | Rev.N, October 2021 |
| NT3H2111 / NT3H2211 (NTAG I²C plus family; exact variant provisional) | Passive NFC, I²C NDEF persistence | [NXP NT3H2111_2211](https://www.nxp.com/docs/en/data-sheet/NT3H2111_2211.pdf), §§2.3, 8.3–8.4, 9–11 | Rev.3.6, 2023-07-21 |

Selected battery pack/cell and any dedicated protector documents: **TBD**.
Record their actual protection/current/temperature limits before schematic
freeze and wearable-use approval; do not assign electrical values from targets.
