# ADR 0014: positive NFC authentication for provisioning

Status: **Accepted** by explicit owner instruction on 2026-10-12.
Local implementation/software checks and positive phone provisioning pass;
broader protection, timing and end-to-end acceptance remain pending.
Amends the provisioning proof in ADR 0013; its guarded updates remain unchanged.

## Context

The Nothing phone successfully authenticated the generated password, but the
factory-password attempt produced an inconclusive I/O error. An I/O error
cannot establish rejection and must never be converted into proof. Requiring
a raw wrong-password response blocks provisioning through this phone path.

[NXP NT3H2111/2211 Rev.3.6](https://www.nxp.com/docs/en/data-sheet/NT3H2111_2211.pdf),
Table 10 and sections 8.7/10.7, specifies one stored PWD and a PACK response on
successful authentication. Under that component model, fresh authentication
using a generated non-factory password establishes that it was programmed.
This is a design inference; it does not independently demonstrate every
unauthenticated-write protection path or resistance to a counterfeit tag.

## Decision

Use proof version 3 and helper v0.5:

1. Generate a non-factory PWD and PACK transiently on the trusted host. Write
   credentials, NDEF and protection policy through guarded I2C as before.
2. In a fresh RF session, check UID/type, read the public CC, authenticate the
   generated PWD with an exact PACK match, then read the CC again.
3. Remove the phone fully. After removal detection, explicitly arm check 2 and
   repeat those positive checks in a new session with the same UID/PWD/PACK.
4. Accept only a one-use, matching-UID, version-3 proof with both positive
   results and the fresh-session assertion. No error or malformed reply
   supplies either result. Do not claim a directly observed factory rejection.
5. Remove the RF field, then finalize through guarded I2C. Firmware verifies
   the protection profile and NDEF before saving the version-3 receipt.

Reject older mailbox/proof/receipt versions; do not migrate their verification
claims. Wipe transient credentials after check 2, failure or pause. Keep the
helper in reader mode until pause so normal URI dispatch cannot hide errors.
Physical RF-field removal is still an operator requirement, not a measured
guarantee from Android's removal callback.

## Consequences and acceptance

This removes the separate raw-NAK requirement from the provisioning receipt.
It does not relax REQ-N8: fresh unauthenticated RF URL/metadata/bypass write
rejection, public powered/passive reads, MCU updates and RF-transition timing
remain separate bench/release gates. R023 remains open until evidence exists.
The retained negative-response test utility still treats I/O errors as
inconclusive; it is no longer the proposed provisioning criterion.

After owner acceptance, build/flash matching firmware and install v0.5, perform
both checks, verify ready status, restore normal BLE firmware, and test two
contact links through Apply, QR scan, NFC tap and power restoration. Software
checks alone cannot establish physical acceptance.
