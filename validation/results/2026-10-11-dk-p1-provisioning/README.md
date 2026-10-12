# DK P1 provisioning confirmation — 2026-10-11

Result: **provisioning workflow completed; powered/off/restore reads owner-reported
passed on 2026-10-12; full NFC acceptance pending**.

**2026-10-12 review correction:** helper v0.2 accepted an ambiguous PWD_AUTH
I/O error as factory-password rejection when a later public read succeeded.
The captures below establish workflow completion, not unambiguous rejection.
They are retained unchanged as historical evidence. Corrected firmware/host
proof version 2 and helper v0.3 require an explicit wrong-password NAK; repeat
provisioning remains pending. The reported power-state URL reads remain valid
as read observations. No corrected-image hardware test was performed.

Equipment: nRF5340 DK + NFC Tag 2 Click + HUB75 prototype. Owner's Android
phone: Nothing 4a Pro (ADB model A069P). Helper versionCode2/versionName0.2,
connected through wireless ADB. iPhone 13 is a later passive-read target;
it was not required for provisioning. Phone OS version and probe serial were
not recorded. Firmware base: `99f85d273c27a00e3c74b95c57e5d91a96257816`, with
uncommitted P1 changes on `fw/prototype-nfc`.

The owner completed the two-check helper workflow and computer confirmation
after the helper lifecycle fix. Independent read-only SWD inspection found
request8=response8, command4, result0, readytrue, address02h. Desired and verified
URLs are both `HTTPS://ZIADG.COM/L`. RTT confirms command4 result0, verifiedyes,
at device uptime 00:50:30.730895. Earlier command3 entries show incomplete
attempts. Non-secret text capture: [provisioning-rtt.log](provisioning-rtt.log),
with ANSI colors and line endings normalized; original local capture is
`build/p1-provisioned-rtt.log`.
Mailbox snapshot: [status.json](status.json).

This establishes completion of the local helper/firmware-confirmation workflow
under the former verifier. It cannot establish factory-password rejection
because the old helper allowed ambiguous errors. Credential-bearing RF traces
and dumps were not retained.

Local bench artifact SHA-256 (no byte-for-byte target flash readback performed):

| Artifact | SHA-256 |
|---|---|
| Provisioning merged image | `21950C3E19D2EBF65F67CE4FA1D2591EB47F97594CBFAF8008D8F97325388D0C` |
| Provisioning firmware ELF | `DC82E5AE5453DD99087F2E4A38004CB17DABE0B1CD1204C1C3FB8E69B7E0B222` |
| Helper v0.2 APK | `3720C116B67C8B8A2C55DF166E70A0A8B353635B6209B8798D33B86A5F4106A7` |

The owner initially reported that both configured links open on tap. Exact
decoded NDEF/QR comparisons and phone/power conditions were not recorded then;
the 2026-10-12 power-state read report is recorded below. Still pending: detailed
per-phone/supply-removal records, a second URL after verified provisioning, normal-image reboot,
unauthenticated URL/CC/configuration/lock attacks, interrupted writes/recovery,
FD/SCL timing and Rev A coil/mechanical-stack testing. R005/R023 remain open.
See [the bench procedure](../../../docs/nfc-prototype.md).

Subsequent iPhone observation: tapping opens Chrome and then the LinkedIn app,
but the app shows its home screen instead of the profile. This is a navigation
failure, not full functional acceptance. Public `/L` requests (including an
iPhone browser user agent) return HTTP302 to
`https://www.linkedin.com/in/ziad-ghanem/`; DK status still verifies `/L`.
The direct profile link from Notes opens the profile (owner report), while
the NFC retest after selecting that link preference remains deferred. No
firmware/payload/website change was made for this observation.

## Owner follow-up — 2026-10-12

The owner reports the configured LinkedIn URL is readable before power-off,
while powered off and after power restoration:

| Condition | Outcome | Evidence |
|---|---|---|
| Powered, before power-off | Pass | Owner report |
| Powered off | Pass | Owner report |
| Power restored | Pass | Owner report |

These are owner-reported functional results, separate from the earlier direct
SWD/RTT provisioning capture. The report does not identify each phone, supply
disconnection details, reader/OS version, fresh image hash or raw URI/QR capture.
It does not establish a second-URL comparison, normal-release-image reboot,
unauthenticated-write/metadata protection, interruption recovery or measured
FD/SCL timing. Rev A coil/mechanical-stack testing remains pending. The deferred
iPhone Chrome-to-LinkedIn profile handoff (OD-60) is not resolved by URL readability.
R005/R023 remain open. ADRs 0012 and 0013 were accepted by the owner on this date;
that design approval does not replace remaining bench evidence.
