# Separate firmware roots and first nRF52840 port

Date: 2026-10-17 deployment; 2026-10-18 port/build checks.
Source: merged main `58a50197c0b278a0fcc95d849aad2d1132e657eb` for deployment;
uncommitted `fw/rev-a-nrf52840` worktree based on that commit for the port.
Hardware: nRF5340 DK probe1050067477 and Nothing A069P for deployment only.
No nRF52840/Rev A board has been programmed or physically tested.
SDK/toolchain: pinned nRF Connect SDK v3.4.1, application `-Wextra -Werror`.

## Deployment result

Installed Pixel Badge0.10.3/build31 in place and launched it. APK SHA-256:
`9054348f3476d06f005baf384fdb10632b23b9ef33acc9b17897ab34fa5868aa`.
App draft/library hash remained unchanged. Flashed the rebuilt merged-main
recording-enabled nRF5340 image without erase after a fresh locally encrypted
320KiB persistent backup. Post-flash persistent bytes matched, with zero changed
and zero newly programmed bytes; settings, loaded bond, NFCready/result0 and
healthy-boot RTT were observed. No recording or owner functional tests run.
Later port source changes have been built, not deployed to the DK/phone.

Local ignored evidence: `build/reva-start-app-install.json`,
`build/reva-start-deployment/flash.log`, encrypted backup/check JSON and RTT.
Secret-bearing backups are not published.

## Software checks

| Check | Result |
|---|---|
| Shared Twister suite after relocation |149/149 cases,22/22 configurations passed |
| Updated audio suite after16kHz capture change |6/6 cases passed, including native 320-sample capture/FFT profile |
| QR configuration / app define / NFC host tests |8 /3 /17 passed; NFC ResourceWarning treated as an error |
| Compiled Rev A contract |Passed; GPIO/PSEL/bus/ADC/UICR/partitions matched all 43 U401 schematic pads |
| Contract negative checks |Incorrect latch pin and overlapping flash rejected |
| C/C++ format, YAML, hygiene, whitespace |Passed |
| Bash wrapper |Syntax and both targets' image-scoped fragments/environment cleanup passed with mock west |
| Vendored QR/font relocation |Byte-for-byte identical to main |

The 149-case suite predates the one additional audio case; the final six-case
audio report covers that change. Both saved Twister JSON reports contain only
passed cases/configurations. The host capture test does not measure PDM hardware.
CI configuration is updated; no hosted run for this uncommitted branch exists.

## Build allocation

Linker allocations, in bytes; RAM includes statically reserved stacks/buffers.
They are capacity evidence, not measured execution/stack high-water margins.

| Configuration | Flash | RAM |
|---|---:|---:|
| Rev A default, matrix disarmed |253968 |100869 |
| Rev A matrix + recording bench |254448 |100869 |
| Rev A peripheral diagnostics |62044 |65056 |
| Preserved nRF5340 recording app |206584 |104497 |
| nRF5340 network radio |150384 |38216 |
| Rev A MCUboot |30660 |16552 |

Rev A application/controller fits 327168 bytes of linked app space in a 320 KiB
primary slot and uses 256 KiB total chip RAM. The 64 KiB boot region, two 320 KiB
firmware slots,264KiB artwork,24KiB reserve and32KiB settings fit exactly 1 MiB.
128-frame capacity is retained. OTA transport/release signing is not supplied.
SDK warnings include deprecated Partition Manager/CC310 boot options, development
signing keys, unused image-manager footer assignment and a vendor-prefix warning
in MCUboot. The diagnostic override also emits dependency warnings for disabled
BLE options inherited from the normal configuration. No application compile
errors occurred; these builds do not validate release configuration/security.

Local logs: `build/reva-start-*-final.log`,
`build/reva-start-native/twister.json`, `build/reva-start-audio/twister.json`.
The source pin contract and checker are versioned; schematic XML export is local.

## Deferred and unfinished

Owner explicitly deferred remaining final physical tests to Rev A bring-up.
They are pending, not passed: optics/refresh/grayscale, current/scan-stall safety,
maximum artwork transfers, BLE/audio/motion combined load, recording-view
visibility, security/endurance, passive NFC and RF/antenna behavior.

Remaining implementation: charger/USB/battery sensing and shedding, off/charging/
low-power policy, complete board diagnostics/chain readback, nRF52840 wired tag
setup packaging and NFC dynamic power/recovery. Default matrix output is disarmed,
charging disabled; no claim of ready-to-wear firmware. REG1 DC/DC support, clock/
chain/pin mapping and microphone response are verification assumptions.
See [Rev A implementation](../../../docs/rev-a-firmware.md) for commands and limits.

## Recovery and command-path corrections - 2026-10-18

A failed runtime frame now makes the next bounded retry reinitialize the
display, clearing the matrix driver's latched fault. Recovery requires scan
progress after initialization; successful configuration with no scan progress
does not clear the existing two-second failure deadline. Persistent failures
still lead to watchdog reboot. Healthy frames retain the normal submission
path. Recovery confirmation also covers builds without recording enabled.

NFC configure/provision examples and runner/unsigned-image paths now name the
`nrf5340_prototype` image directory rather than the removed `firmware` image.

Verification: seven display-health/boot-guard cases in two native configurations
passed, including latched-fault recovery and initialization-without-scan-progress
failure escalation. Seventeen NFC host cases passed with ResourceWarning as an
error. C/C++ format, hygiene, whitespace and rebuilt Rev A pin/flash contract
plus negative checks passed. Both recording-enabled target builds passed.
The full shared native suite was not repeated for these corrections.

Updated linker allocations: Rev A matrix/recording254512 bytes flash /
100869 RAM; nRF5340 recording app206648 /104497. Both add64 flash bytes relative
to the earlier builds above; RAM and partition layout are unchanged. MCUboot
and radio allocations remain unchanged. Sources remain uncommitted.

Local evidence: `build/port-fix-native/twister.json`,
`build/port-fix-native.log`, `build/port-fix-reva.log` and
`build/port-fix-prototype.log`. No hardware was reflashed or physically tested;
these results do not establish actual scan-fault recovery or optical timing.

## Intentional disarm and service-loop health - PR28 follow-up

The default Rev A configuration previously treated the intentional unblank
`-EPERM` result as a persistent display failure and stopped watchdog feeding.
The application now recognizes this state only when the Rev A matrix is built
with output disabled and initialization reaches that expected result. It
excludes this deliberate disarm from health failure tracking, skips repeated
display initialization/brightness writes and logs the state once.

Physical display readiness remains false and visibility consumers still receive
`-EPERM`. Existing display-dependent write, new-pairing and capture gates stay
closed; no blank frame is acknowledged as visibly presented. The service loop,
BLE startup and local NFC startup can continue. Actual initialization errors,
runtime faults in enabled builds and unexpected `-EPERM` retain bounded recovery
and watchdog escalation.

Nine native display-health/boot-guard cases in two configurations passed. New
cases cover 31 seconds of simulated disarmed health/boot-guard time, other
driver failures during disarm, and unexpected permission errors in enabled
operation. Default disarmed Rev A, enabled matrix/recording Rev A and nRF5340
recording builds passed. Both Rev A configurations passed compiled contract and
wrong-GPIO/overlap negative checks; format, hygiene and whitespace passed.

Updated linker allocations: safe Rev A254120 flash/100869 RAM; enabled Rev A
254480/100869; prototype206632/104497. Partition, bootloader and radio sizes
remain unchanged. Local logs: `build/pr28-disarmed-{safe,matrix,prototype}.log`,
`build/pr28-disarmed-native.log` and `build/pr28-disarmed-native/twister.json`.
No flash or physical reset-loop/visibility test was performed. Sources are a
local correction on top of27277c4 until owner signing and push.
