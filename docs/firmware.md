# Firmware

The nRF5340 DK prototype implements QR/text, persistent drawings/animations,
BLE configuration through Pixel Badge, synchronized NFC links, microphone
level/spectrum visualization, encrypted phone recording and IMU-driven
Fluid/orientation. Recording stays disabled in default builds pending physical
recording-view visibility/security/endurance acceptance; the explicit bench fragment
allows development tests. P0-P4 software is merged through PR26.

The current cleanup adds automatic V1 QR correction selection, longer fitting
links and smaller BLE staging buffers. Pixel Badge adds a named local library,
image/GIF conversion and simpler connection/editor flows. Firmware now has separate
`nrf5340_prototype` and `rev_a_nrf52840` application roots with shared code in
`common`. The first [Rev A port](rev-a-firmware.md) builds application/controller
and a disarmed matrix backend; power policy and board validation remain.
The legacy nRF5340 custom-board definition is retained for regression only.

Current behavior and evidence:
- [BLE/Studio protocol](ble-protocol.md), [app](../app/README.md)
- [Local NFC synchronization](decisions/0017-local-nfc-synchronization.md)
- [Audio and recording](audio-prototype.md), [motion](motion-prototype.md)
- [Complete DK deployment and retention limits](../validation/results/2026-10-15-dk-complete-prototype/README.md)
- [Prototype polish and firmware review](../validation/results/2026-10-16-prototype-polish/README.md)

Historical foundation, pairing and NFC bench reports remain in validation/results.
Owner functional reports and software tests do not establish complete RF,
power, optical, timing or Rev A hardware acceptance.

## Platform

- nRF Connect SDK v3.4.1 (Zephyr RTOS)
- Initial target: `nrf5340dk/nrf5340/cpuapp`
- Language: C++17 for application and domain logic; C or C-compatible
  interfaces where Zephyr APIs require them

## Language and runtime policy

- No C++ exceptions.
- No RTTI unless explicitly justified later.
- No runtime dynamic allocation unless explicitly justified.
- P2 exception: the pinned SDK uses its bounded crypto/controller heaps for
  authenticated BLE (CPUAPP Mbed TLS heap 512 bytes, IPC radio system heap
  8192 bytes in the verified development configuration). Application-owned
  commands, staging, queues and frames remain static; CPUAPP system heap is
  disabled. This SDK allocation supports LESC rather than general application
  allocation. Runtime exhaustion still needs phone bench testing.
- Zephyr synchronization and threading primitives (`k_mutex`, `k_sem`,
  `k_msgq`, work queues), not `std::thread`/`std::mutex`.
- The build currently uses Zephyr's minimal C++ library. Whether to enable a
  full C++ standard library is TBD.
- Application-owned sources compile with `-Wextra -Werror`. These flags apply
  only to the application target, not to Zephyr or third-party modules.

## Layering

Dependencies point downward only.

| Layer | Directory | Responsibility |
|---|---|---|
| Application | `firmware/common/src/app/` | Display modes, mode switching, top-level behavior |
| Services | `firmware/common/src/services/` | Hardware-independent logic and protocol handling (for example display content generation, BLE/NFC service logic, sensor data processing) |
| Drivers | `firmware/common/src/drivers/` | Project-specific device drivers (for example the LED matrix driver), using the Zephyr device model where it fits |
| Platform | `firmware/common/src/platform/` | Board and Zephyr integration glue |

Pure logic in the services layer should not depend on hardware so it can be
unit-tested on a host target. Public headers shared across layers go in
`firmware/common/include/`. Board overlays and custom board definitions go in
each application root's `boards/` directory.

`firmware/common/src/drivers/` is a Zephyr module (`zephyr/module.yml`) so the
application and the tests share its Kconfig, devicetree bindings and sources.
It holds the ICM-42688-P I²C driver (compatible `invensense,icm42688p-i2c`):
NCS v3.4.1's `icm4268x` driver supports SPI only. The project driver reuses
that driver's register map and devicetree property encoding, applies the
datasheet's I²C settings (DS-000347 §12.3), and supports data-ready interrupts.
It rejects accelerometer ODRs the selected power mode cannot run, sets ODR and
full scale before turning the sensors on, and reports a sensor that has no
sample since reset (data registers at -32768: sensor off or still starting) as
`-EAGAIN` from fetch and `-ENODATA` from `channel_get` instead of as data. It
does not detect repeated samples: with unequal ODRs the slower sensor's
registers hold its last sample, so consumers that need each sample once use
matching ODRs or, later, the FIFO. On the bench module, the first I²C
transaction after the driver turns I3C off stalled until the TWIM timeout and
its bus recovery, so the driver runs the bus recovery right after that write
(a bench observation, not a datasheet requirement). Moving the part to SPI only
changes the bus node and uses `compatible = "invensense,icm42688",
"invensense,icm4268x"`. Wake-on-motion
is not implemented yet.

The application reaches the display only through Zephyr's display API
(`zephyr,display` chosen node), as a 32 × 32 monochrome display
(`PIXEL_FORMAT_MONO01`, rows packed most significant bit first). On the DK the
backend is a HUB75 panel (below); the badge gets its own matrix driver behind
the same API. Frames and test patterns live in `firmware/common/src/services/`
(`badge/test_pattern.hpp`). `src/app/prototype.cpp` owns the main loop;
`src/app/modes.cpp` selects content. Services `qr`, `settings`, `buttons`,
`boot_guard`, `display_health` and `text` are independent of Zephyr and have
native_sim Ztests.
`src/platform/settings_store.cpp` connects validated fields to Zephyr NVS;
`boot_retention.cpp` isolates the Nordic retention register. No application
code includes HUB75 or IMU driver headers.

## Prototype settings and controls

Public defaults: URL `HTTPS://EXAMPLE.COM`, name `BADGE`, brightness 128,
boot mode QR; schema version 1. Records live under `badge/` in Zephyr settings.
Local builds can replace the compiled URL from `.env` as described below;
that URL also becomes the factory-reset and safe-mode default.
Strings are stored without their terminating NUL; scalar fields are one byte.
Every field is validated on load. Missing/invalid fields use their defaults
and log the field name, never the stored payload. An absent/unknown schema
defaults all fields. Load errors select safe mode and disable writes.
On the first save after schema recovery, every application field is replaced
with its default before version 1 is written. The requested field is saved
afterward. An interruption before schema activation still loads defaults;
after activation, all remaining records are defaults, so stale values cannot
reappear. Future bonds/keys outside `badge/` are not erased by this recovery.

- URL: bounded ASCII HTTP(S) URL with a nonempty authority, no whitespace,
  controls or user-info; percent escapes must have two hex digits. Maximum
  25 QR alphanumeric characters or 17 ASCII bytes, required to fit V1. Encoding keeps all URL
  bytes and path case; it does not uppercase a link to make it fit.
- Name: 1–32 printable ASCII bytes. The original 3 × 5 font renders lowercase
  as uppercase; common punctuation is supported and other glyphs show `?`.
  Names of up to four characters use 2× glyphs, five to eight use 1×; longer
  names scroll one pixel per 150 ms with an eight-pixel gap.
- Brightness: 1–255. Button steps are 16, 64, 128, 255, then 16.
- Default mode: QR or name. QR is the compiled boot default; the BLE
  configuration client can save a different boot mode. Button mode changes
  affect the active mode only.

The settings service validates before writing and updates RAM only after a
successful single-field save. NVS writes data and its CRC before commit
metadata, ignores incomplete writes on mount, and retains the prior committed
value. Schema recovery publishes the schema last after writing all defaults;
it is not a general multi-field transaction. The host
flash-simulator test interrupts a name replacement at each byte boundary and
remounts the actual backend. Arbitrary bit corruption is detected with the
data CRC. A per-setting read failure (including a CRC error or short read)
discards partially loaded settings, selects defaults and safe mode, and disables
writes for that boot. Semantically invalid records still default without an I/O
failure. This is distinct from power-cut recovery. No keys or audio are stored
in P0. NVS itself is not encrypted.
See [ADR 0012](decisions/0012-prototype-foundation.md) for the storage choice.

| DK control | Action |
|---|---|
| Button 1 short | Toggle QR/name |
| Button 2 short | Cycle brightness and persist the new value |
| Either button long | Reserved; log the event with no action in P0 |
| Button 2 held at boot | Factory reset after a continuous one-second hold |

Buttons are polled at 20 ms, debounced for 30 ms and classified long at 800 ms.
A long press never also produces a short press. A button held at startup is
suppressed until release. Factory reset erases the entire settings partition
before mounting, including future bonds/app-key records. Keep holding Button 2
through the reset confirmation; after a power cut during erasure, repeat this
procedure. Factory reset is not an atomic transaction across the partition.

QR is fixed at Version 1 (21 × 21), EC M without boosting. Modules start at
(5,5), giving five quiet pixels above/left and six below/right: at least the
full four-module quiet zone. Light modules/background are lit, dark modules
unlit. Invalid/non-fitting input keeps the previous URL and frame. No V2/V3
experimental option is introduced. Rotation happens in rendering, with a
hook for later orientation; QR captures rotation on entry and retains it
until re-entry. The scan driver does not rotate content.

The hardware watchdog has an eight-second timeout and is fed by the badge
loop. Reset cause is logged through Zephyr hwinfo. POWER GPREGRET[1] retains
a tagged watchdog-reset count; three consecutive watchdog resets before a
30-second interval of successful display operation select safe mode with the
default QR and brightness.
Safe mode ignores mode/brightness presses and continues feeding the watchdog.
A healthy loop clears the count while staying in safe mode for that boot;
a later reset can return to normal. A non-watchdog reset starts a fresh count.
Nordic's [nRF5340 reset behavior](https://docs.nordicsemi.com/r/bundle/ps_nrf5340/page/chapters/reset/doc/reset.html)
specifies GPREGRET retention on watchdog reset. The owner reports the forced
reset-loop bench test passed; that sequence is absent from the direct capture.

Display writes, including `-EBUSY`/`-EAGAIN`, are retried at 100 ms intervals
without permanently disabling rendering or brightness controls. Failed
initialization is also retried. Two seconds of uninterrupted errors latches
a restart: the application attempts blanking and stops feeding the watchdog,
which resets the SoC within its configured timeout. If the watchdog is
unavailable, it requests a software reset instead. Any recovered error starts
a fresh healthy interval, so a recent display fault cannot clear the retained
reset-loop count. These are application policy intervals, not measured
hardware recovery times. A display that remains broken can still prevent
safe-mode QR output; the restart path does not repair hardware.

`CONFIG_APP_BRINGUP_CHECKS` is off for the default prototype. The
`prototype-debug.conf` fragment enables checks in a separate lower-priority
thread after startup; normal display/buttons/watchdog do not wait for them.
`watchdog-loop.conf` deliberately stops feeding before healthy confirmation
until safe mode is reached. Use it only for the bench procedure below.

## Logging

- Primary: Zephyr logging with the SEGGER RTT backend (enabled in `prj.conf`).
- Rev A has no UART pads (all module GPIO are used); RTT is the only log path.

No project-specific logging abstraction is used.

## Audio privacy requirement

The badge visualizes sound and can record voice for playback in the phone app
([ADR 0011](decisions/0011-visible-voice-recording.md)). Recording is visible
and never stored on the badge:

- It starts only from the paired app or a button long-press, and only when
  the phone is connected with an authenticated bond (LE Secure Connections
  with MITM protection; Just Works bonds are refused for audio), the
  companion app is subscribed to the audio stream and has proved the automatic session key
  on this connection (a button start also waits for a fresh authenticated
  accept from the app), and the LED rail is on (not shed for low battery)
  with the matrix driver running. Otherwise nothing is captured for sending, and
  after a long-press the matrix, if on, briefly shows that recording is
  unavailable.
- It runs as its own display mode: the matrix shows REC and a live level bar
  for the whole recording, at a fixed recording brightness (set by the
  visibility test) whatever the brightness setting. The mode cannot run
  behind another mode. Release firmware enables recording only if REC passes
  that test in every condition, direct sun included.
- Capture for a recording starts only after the matrix driver confirms the
  REC frame is shown; earlier samples, including any from a visualization
  mode, are discarded and never encoded or sent.
- Audio is streamed live, compressed (IMA ADPCM at 16 kHz), over the encrypted
  Bluetooth link to the paired app, which keeps recordings only in its
  private, encrypted storage, excluded from backups; a recording leaves the
  app only by an explicit export or share.
- The normal authenticated Bluetooth bond authorizes audio. Session keys are
  exchanged automatically on connection, held only in RAM and cleared on
  disconnect; no separate enrollment or code confirmation. Commands carry
  replay counters/MACs and each recording has a fresh encryption key. This
  trusts the phone rather than authenticating one distinct app on it, per
  [ADR0016](decisions/0016-simple-audio-controls.md).
- The user stops it with a button press or from the app. It also stops when
  the link drops or the app unsubscribes, when the display is turned off or
  blanked, when the LED rail is shed for low battery or the matrix driver
  faults. The phone also stops at low storage (16MiB reserve) or its classic WAV
  format boundary (~37.28hours); there is no selectable duration or five-minute
  timeout. Before REC leaves the
  matrix or the LED rail turns off, capture stops and any audio not yet sent
  is discarded; on a matrix driver fault, capture stops at once and queued
  audio is discarded, not sent.

Audio data, both raw microphone samples and the encoded ADPCM stream, exists
on the badge only in volatile/transient buffers (capture, DSP, encoding and
Bluetooth transmit queues) and is discarded when visualization or the recording
ends. No audio data may reach flash, NVS/settings, external storage, logs,
telemetry, crash dumps, or other persistent storage, and no key (app key,
per-recording key or key-exchange secret) may reach logs, telemetry or dumps.
Review capture, DSP, transmission, logging, persistence, and fault/dump data
flows before release, including debug configurations. Persistent crash dumps
must exclude audio buffers or be disabled. Verification requirements are in
[validation.md](validation.md#8-microphone-and-audio-processing); no audio
feature or completed privacy audit exists yet (the bring-up check only logs
levels).

## Building

Requires nRF Connect SDK v3.4.1 and its toolchain.

Windows (PowerShell), with the SDK installed under `C:\ncs` (override with
`-NcsRoot` or the `NCS_ROOT` environment variable). The script loads that SDK
and the toolchain it requires, and fails if the SDK is not v3.4.1 or the
toolchain is missing:

```powershell
.\scripts\dev.ps1 build
```

Linux/WSL, with the toolchain on `PATH` and `ZEPHYR_BASE` set:

```sh
make build
```

### Local QR links

Prototype builds read the repository-root `.env` when it exists. The four
`BADGE_URL_PORTFOLIO`, `BADGE_URL_LINKEDIN`, `BADGE_URL_GITHUB` and
`BADGE_URL_RESUME` entries must all be valid V1 URLs. Only the HTTP(S) scheme
is uppercased to allow alphanumeric encoding; every other byte, including path
case, is preserved. Alphanumeric URLs fit up to 25 characters; URLs needing
byte encoding fit up to 17. Unrelated `.env` entries are ignored and values
are never evaluated as commands or environment-variable substitutions.

Portfolio is the local build default. To select another link before the phone
app exists:

```powershell
.\scripts\dev.ps1 build -QrLink LinkedIn
# Other choices: Portfolio, GitHub, Resume, Representative
```

For Linux/WSL, pass `-DBADGE_QR_LINK=LINKEDIN` to `west build` after `--`.
Use `REPRESENTATIVE` to explicitly ignore `.env`, or omit `.env` for the public
portfolio placeholder. Changing the link or `.env` regenerates a header in
the build directory; personal URLs remain outside committed sources.

An existing valid NVS URL takes precedence over the compiled default. To use
the new default on an already configured badge, hold Button 2 while resetting
through the one-second factory-reset confirmation. This also resets the name,
brightness and other settings. Button 1 continues switching QR/name; the
badge never automatically cycles destinations. App/BLE link selection is
planned and is not implemented in P0.

Flash output: **`build/nrf5340dk/merged.hex`**, containing MCUboot and the signed
application. The unsigned `nrf5340_prototype/zephyr/zephyr.hex` cannot boot this layout.
CI archives merged HEX, the signed application, ELF/map/config files and the
resolved partition layout. Bench configurations also archive their merged HEX.

Use `-Pristine` once for each existing CPUAPP build directory when moving
from the bring-up firmware to this layout; an old cached sysbuild selection
can otherwise retain its no-bootloader setting. CI builds fresh directories.

### MCUboot and static flash layout

Sysbuild enables MCUboot and Partition Manager for both CPUAPP board targets.
The 1 MiB application-core flash uses `firmware/nrf5340_prototype/pm_static.yml`:

| Region | Start | Size | Reason |
|---|---|---|---|
| MCUboot | `0x00000` | 64 KiB | Space for the current bootloader with growth margin |
| Primary slot | `0x10000` | 320 KiB | Application plus 512-byte header pad and upgrade/trailer allowance |
| Secondary slot | `0x60000` | 320 KiB | Later application update staging; no DFU transport in P0 |
| Artwork banks | `0xb0000` | 264 KiB | Two 132 KiB banks for verified flash-backed animation |
| Reserve | `0xf2000` | 24 KiB | Unallocated |
| Settings | `0xf8000` | 32 KiB | Eight 4 KiB NVS sectors for app settings, future bonds and a small key store |

The app starts at `0x10200`. Slot size is not the maximum payload size: header,
trailer and the selected swap-using-move algorithm also consume space. Future
images must pass the build/signing size checks. Settings are deliberately
larger than the few P0 records, providing garbage-collection and wear margin;
bonding/key endurance still needs physical acceptance. ADR0019 reused the old
network-update reserve for artwork; no network-core update payload is reserved.
Adding radio-core updates would require a new storage plan.

The superseded nRF5340 custom-board CPUAPP uses this boot layout for compile
compatibility. The current E73 target has its own [port](rev-a-firmware.md),
with matching artwork/settings offsets and a combined app/controller image. The standalone CPUNET image and matrix timing
bench application retain their original build/flash layout. Partition Manager
is deprecated but supported on nRF53 in pinned NCS v3.4.1; see ADR 0012 for the
explicit choice. P0 uses the SDK's publicly distributed
development signing key and supplies no production signing credentials.

### Owner bench acceptance

The owner reports the six steps below passed on 2026-10-11 for the initial P0
image. The [record](validation.md#result-prototype-foundation-on-the-dk)
separates this report from the narrower direct RTT evidence. Repeat after
display, schema and settings-read recovery changes; the revised image has not
been bench-tested.

Use the existing HUB75 wiring and power procedure below. Close all DK COM
terminals, use RTT, and keep Buttons 3/4 untouched. In an NCS v3.4.1 terminal:

```powershell
.\scripts\dev.ps1 build -Pristine -QrLink Representative
west flash -d build/nrf5340dk --runner jlink
```

Check `build/nrf5340dk/nrf5340_prototype/zephyr/runners.yaml` first: its HEX path must
resolve to the top-level `merged.hex`. The sysbuild flash command programs the
merged image; if using J-Link Commander directly, select the nRF5340 application
core and load that merged HEX. Do not erase the chip when checking persistence.
Open the J-Link RTT viewer and select the application-core device.

1. Fresh/reset settings: RTT logs reset cause, defaulted fields and `badge ready:
   QR mode`; the panel shows the representative URL and a phone decodes
   `HTTPS://EXAMPLE.COM`. Confirm the full quiet border.
2. Button 1 short switches to `BADGE`, then QR. Holding either button for at
   least 800 ms logs a reserved long press with no short action on release.
3. Button 2 short changes brightness and logs the committed level. Reset the DK
   without erasing/reflashing; the level must persist. Repeat after a power cycle.
4. Hold Button 2 while resetting, through early boot and its one-second
   confirmation. RTT must report settings erasure, default QR and brightness
   128. Release: no accidental brightness change should occur.
5. Build/flash the forced-loop image:

   ```powershell
   .\scripts\dev.ps1 build -BuildDir build/watchdog-loop -ExtraConfFile firmware/nrf5340_prototype/configs/watchdog-loop.conf -QrLink Representative
   west flash -d build/watchdog-loop --runner jlink
   ```

   Leave it running without debugger halts. Three watchdog resets, each after
   the eight-second application timeout, must reach `watchdog reset loop: safe
   mode, default QR`. It must then remain alive beyond 30 seconds, ignore short
   UI presses, and log healthy-counter clearing. Bootloader time adds to reset
   intervals. Restore the default merged image after this test.
6. Optional checks: build with `prototype-debug.conf`, flash that directory,
   confirm module checks/readings coexist with immediate QR/button operation.
   Preserve the existing QR screening build separately; it retains its original
   Button 1 next / Button 2 previous controls.

Record the build revision, panel/phone, RTT logs, persistence/factory-reset and
watchdog outcomes. Measured GPIO timing, interrupted physical flash writes,
concurrent optional checks and broad optical acceptance remain unverified.
The owner-reported functional pass is not a measurement of those properties.

On the DK, `firmware/nrf5340_prototype/boards/nrf5340dk_nrf5340_cpuapp.overlay` puts the Rev A
pin roles on the DK's pins where it can: SPIM4 on P0.08–P0.10, matrix LAT and
BLANK on P0.11/P0.12, and I²C1 with SCL on P1.02 and SDA on P1.03. It also adds
a frame marker on P0.26 for the logic analyzer and removes Buttons 3 and 4
(their pins carry the matrix SPI). UART1 stays off: its pins are the interface
MCU's serial port 0 (VCOM0 on the board; RXD P1.00, TXD P1.01, CTS P0.10,
RTS P0.11), and P0.10/P0.11 are no longer forwarded to the network core, so
starting it (for Bluetooth, for example) cannot take matrix MISO and LAT away.
Adding or changing an overlay needs a pristine build (`-Pristine`, or
`--pristine` with west).

The overlay also makes a HUB75 RGB panel the DK's display, for bench work
only: R1, G1, B1, CLK and LAT on P1.11–P1.15, OE on P1.08 and row address A–E
on P0.04–P0.07 and P0.25, through 5 V buffers (SN74AHCT245). The driver
(`firmware/common/src/drivers/hub75`, compatible `smartledbadge,hub75`) shows the
32 × 32 window in the panel's top half in one colour. TIMER0 times each 160 µs
row slot (195 Hz refresh): its interrupt latches the row shifted during the
previous slot and shifts the next row while this one is lit, and a second
compare switches OE off through DPPI and GPIOTE, so brightness is set by
hardware. TIMER1 and TIMER2 stay free for the matrix timing test. If the image
is mirrored left to right, set `mirror-x` on the node.

The DK overlay also puts the IMU (INT1 on P1.04), the PDM microphone (CLK P1.07,
DATA P1.09) and the NFC tag's field-detect line (P1.06) on their Rev A pins. The
tag shares the IMU's I²C1 bus on the DK, because its Rev A bus pins are the
debugger's serial port. At start-up the application runs module checks
(`firmware/common/src/app/bringup.cpp`) while the display test patterns run in their
own thread, then logs an IMU sample, the NFC field-detect level with its edge
count, and the microphone level every 5 s:

- IMU: identity (checked by the driver), one sample with the gravity magnitude,
  and the data-ready interrupt rate (100 Hz expected).
- NFC tag: the P1 worker owns runtime verification and the FD latch; the
  bring-up check logs delegation to that service. In builds without the worker,
  the shared transport initializes volatile NC_REG for field detection, reads
  block 00h and checks UID byte 0 (`04h`). It writes no EEPROM. Frames use the
  guarded procedure in [nfc.md](nfc.md), STOP-separated reads and the same bus
  mutex as IMU fetch/trigger setup. The DK FD-latency margin is unmeasured.
- Microphone: captures both PDM channels at 32 kHz with the clock near 2 MHz
  (the MP34DT01-M's range is 1–3.25 MHz, 2.4 MHz typical; at 1.25 MHz the
  quiet level was over three times higher) and logs the mean, AC RMS and
  range of each. A
  mic's PDM stream averages near zero; a DATA line nothing drives (pulled down
  on the DK) decimates to a large constant offset, so the check fails unless a
  channel varies with a mean within ±4096. Both channels may carry nearly the
  same data, because the mic's idle half-cycle can hold the previous bit.
  Samples stay in the capture buffers and are discarded; only the levels are
  logged.

Before matrix work on the DK: the interface MCU leaves its UART pins tri-stated
until a terminal opens the port and asserts DTR, and then drives them. Close
every terminal on the DK's J-Link COM ports and log over RTT instead. If one was
opened, power-cycle the DK by unplugging its USB cable. For captures that do not
need the debugger, the nRF ONLY switch (SW6) disconnects the interface MCU
completely, including SWD, so flashing and RTT stop working until it is set back.

### P1 NFC service

The default prototype starts a dedicated NFC worker with the persisted URL.
It synchronizes only a tag with a matching verified UID/profile receipt and
valid access policy. Safe mode or settings I/O failure disables writes.
The fixed-buffer service uses EEPROM, public RF reads, protected RF writes,
reversible CC locks and no SRAM mirroring/pass-through. The `nfc/verified`
NVS key stores no password. Factory reset removes it, so the external tag
must be explicitly provisioned again; its old EEPROM URL is not erased.

`firmware/nrf5340_prototype/configs/nfc-provisioning.conf` enables the temporary SWD mailbox and
host/Android provisioning tools. It also permits a bench URL change that is
persisted, rendered as QR and written/read back on the tag before success.
URL changes preview the candidate QR without modifying settings. They persist
the URL only after successful display submission and guarded tag verification.
A failure or timeout restores the previous QR/mode and leaves the durable URL
unchanged; tag work that started is followed by a guarded rollback attempt.
Late display/tag completion cannot commit a cancelled request. A failed rollback
can leave tag data different or incomplete: the command reports failure, exposes
the last verified tag URL and keeps the tag out of service. Normal
builds omit this SWD interface. P2 BLE uses the same guarded URL path. Build, flash,
RF proof, passive-read and recovery instructions are in
[nfc-prototype.md](nfc-prototype.md).

### P2 BLE configuration and Flutter app

The optional BLE sysbuild configuration adds the application-core GATT host
and network-core IPC radio, with network UART disabled. Hold Action/Button 2
to open a 60-second pairing window and enter the six-digit passkey shown in
two rows on the matrix. Authenticated LE Secure Connections is required for
settings, Apply and status; one bond is retained in NVS. Factory reset also
removes this bond and the NFC receipt.

The app chooses among the four local `.env` contact links and edits name,
brightness and mode. Writes stage one field, then Apply queues its operation.
Bluetooth callbacks perform no display, settings or I2C work. Completion is
distinct from ATT acceptance: URL success requires display/tag synchronization
and persistence; the app then checks exact readback. Disconnection or uncertain
completion requires a fresh read, with no automatic replay.

The normal build keeps P1 behavior. Use the dedicated development target and
flash both sysbuild domains with `west flash`; app build and phone bench steps
are in [ble-protocol.md](ble-protocol.md). Source, pinned dependency and Android
setup are in [app/README.md](../app/README.md). Native protocol and Flutter
tests do not establish physical pairing or RF/QR acceptance. Audio streaming,
firmware updates and iOS packaging are later work.

### Current Rev A board

Use `./scripts/dev.ps1 build -Target RevA -Pristine` for the current E73 board.
See [Rev A firmware](rev-a-firmware.md) for pins, variants, budgets and limits.

### Superseded nRF5340 custom board

The following definition describes the earlier MDBT53-P1M schematic. It is
retained for regression builds and does not match the current E73 hardware.

`firmware/nrf5340_prototype/boards/smart_led_badge/` defines the earlier board (Raytac
MDBT53-P1M, nRF5340), based on Zephyr's `raytac_mdbt53_db_40` board. The
application-core devicetree follows the earlier Rev A schematic:
- SPIM4 for the matrix chain, with GPIOs for LAT, BLANK, ARM and SAFE.
- TWIM1 for the IMU and charger at 400 kHz, and TWIM2 for the NTAG at 100 kHz.
- PDM microphone; SAADC channels for VBAT, CC1, CC2 and VLED with the gains
  in [power.md](power.md).
- Buttons, status LED, UART on the test pads, and USB.
- 32.768 kHz crystal with the internal 9 pF load; DC/DC on VREGMAIN and
  VREGRADIO.

Board signals without a Zephyr driver (charger enable CHG_EN, INT, STAT, power switch,
NFC power and field detect, LED-rail enable, VBAT measurement switch) are
`zephyr,user` GPIOs. The network core owns no pins. The build scripts pass
`BOARD_ROOT` so sysbuild finds the board, and CI builds both cores:

```powershell
.\scripts\dev.ps1 build -Board smart_led_badge/nrf5340/cpuapp -BuildDir build/badge
```

```sh
BOARD=smart_led_badge/nrf5340/cpuapp BUILD_DIR=build/badge make build
```

The application does not use most of these peripherals yet, so its own
configuration leaves their drivers out. `firmware/nrf5340_prototype/configs/rev-a-peripherals.conf`
enables the I²C, SPI, ADC, PDM, sensor (IMU), input, LED, USB and watchdog
drivers. CI builds the application core with it too, so driver and devicetree
integration breaks the build rather than bring-up. It is a compile check, not
a product configuration. Pass it with `-ExtraConfFile` (path relative to the
repository root) or `EXTRA_CONF_FILE`:

```powershell
.\scripts\dev.ps1 build -Board smart_led_badge/nrf5340/cpuapp -BuildDir build/badge-periph -ExtraConfFile firmware/nrf5340_prototype/configs/rev-a-peripherals.conf
```

```sh
BOARD=smart_led_badge/nrf5340/cpuapp BUILD_DIR=build/badge-periph \
  EXTRA_CONF_FILE="$PWD/firmware/nrf5340_prototype/configs/rev-a-peripherals.conf" make build
```

The board files are modified from Zephyr's `raytac_mdbt53_db_40` board and
keep its Apache-2.0 license and copyright notices
([LICENSES/Apache-2.0.txt](../LICENSES/Apache-2.0.txt)).

## Bench tests

Bench tests are separate applications under `firmware/nrf5340_prototype/bench/`, built for the
DK with the same overlay. They are not part of the badge image. CI builds them
so they keep compiling.

### Matrix scan timing

`firmware/nrf5340_prototype/bench/matrix_timing` drives the matrix scan signals with the planned
hardware timing, without a matrix attached, so they can be checked on a logic
analyzer. TIMER1 times each 160 µs row slot and drives SPIM4 and the LAT and
BLANK pins through DPPI and GPIOTE. TIMER2 counts 32 rows and then stops TIMER1.
The CPU only restarts each frame from the TIMER2 interrupt, so a late interrupt
leaves the matrix blanked instead of a row on.

| Time in slot | Event |
|---|---|
| 1 µs | LAT high: latches the row shifted during the previous slot |
| 2 µs | LAT low; SPIM4 starts shifting the next row (8 bytes at 8 MHz) |
| 4 µs | BLANK low: row on |
| 159 µs | BLANK high: row off |
| 160 µs | Next slot |

Each row is 32 row-select bits (the row's bit is 0, which turns its PFET on)
followed by 32 column bits (`A5`, row number, `5A`, inverted row number).
RTT logs the frame rate once a second (about 195 frames/s).

Measured results, with the setup and raw data, are in
[validation.md](validation.md#result-scan-timing-on-the-nrf5340-dk-bench-no-matrix).

```powershell
.\scripts\dev.ps1 build -App firmware/nrf5340_prototype/bench/matrix_timing -BuildDir build/matrix_timing
```

```sh
APP_DIR="$PWD/firmware/nrf5340_prototype/bench/matrix_timing" BUILD_DIR=build/matrix_timing make build
```

Analyzer channels: SCK P0.08, MOSI P0.09, LAT P0.11, BLANK P0.12, frame
marker P0.26 (high while the frame interrupt runs), and GND. A 24 MS/s
analyzer decodes the 8 MHz shift (three samples per clock) and times edges to
about ±42 ns, but it distorts the SCK duty cycle; use a faster one to check
SCK itself. Leave Buttons 3 and 4 unpressed and keep terminals off
the DK's COM ports (see above).

### QR screening

Building the application with `firmware/nrf5340_prototype/configs/qr-screening.conf` replaces the
display test patterns with QR test codes for the scan-reliability screening
(validation plan, QR section). Each code is shown in normal and inverted
polarity at brightness 255, 64 and 16; DK Button 1 steps to the next setting,
Button 2 back, and RTT logs the setting number.

```powershell
.\scripts\dev.ps1 build -BuildDir build/qr -ExtraConfFile firmware/nrf5340_prototype/configs/qr-screening.conf
```

The codes come from `scripts/qr/gen_qr_test_codes.py` (segno to encode,
ZXing-C++ to decode each frame back in both polarities). The committed set uses
representative Version 1 payloads with the full quiet zone. Codes for other
payloads, such as personal links, are written to a file outside the repository
with `--out-cpp` and selected with `CONFIG_APP_QR_CODES_FILE`; Version 2 and 3
codes fit only with a reduced quiet zone and are experiments.

## Tests

P4 adds bounded IMU-driven Fluid mode and four-way orientation to the full BLE
DK configuration, with persistent sensor mapping and matching app controls.
Native/Flutter tests and both DK builds pass; new physical behavior, frame time,
stack peaks and combined-load I²C behavior remain unmeasured. This is still the
nRF5340 prototype, not the nRF52840 port. Follow the
[P4 guide](motion-prototype.md) and [software evidence](../validation/results/2026-10-15-dk-p4-motion/README.md).

Tests use Ztest and run with Twister on the `native_sim` host target. They live
under `firmware/tests/`. `native_sim` runs only on Linux, so on Windows
`dev.ps1 test` runs the tests in the same toolchain container image as CI
(requires Docker). The SDK sources are kept in the Docker volume
`smart-led-badge-ncs-v3.4.1`; if it ever fails the workspace version check,
remove it with `docker volume rm smart-led-badge-ncs-v3.4.1`.

The toolchain image has no host C++ compiler or `make`, so CI and `dev.ps1 test`
install them from a fixed Ubuntu archive snapshot with exact package versions
(`scripts/ci/install-host-deps.sh`).

```powershell
.\scripts\dev.ps1 test
```

```sh
make test
```

`firmware/tests/sanity` checks that C++17 tests build and run.
`firmware/tests/icm42688_i2c` runs the IMU driver against a register-level
emulator on the native_sim I²C bus: reset and identity checks, I²C-mode
configuration and write order, power-mode/ODR validation (including a
low-power start-up), invalid-sample handling, unit conversion, and the
data-ready interrupt path. It proves
the driver logic, not behavior on the real part.
`firmware/tests/test_pattern` checks the frame bit layout and the test
patterns.


### Grayscale Studio extension (2026-10-16)

[ADR0018](decisions/0018-grayscale-studio-recording-view.md) adds 0..255
single-color intensity, 32-frame animations within an 8192-byte encoded limit,
individual frame timing and playback direction. The DK HUB75 backend now uses
eight weighted planes for all pixels, including binary QR/text, with timer/DPPI
OE-on and OE-off edges and one-shot row slots. There is no content-dependent
switch of white-pixel duty. This lowers binary-only nominal duty to the existing
gray schedule; QR brightness and combined-load optics need fresh bench checks.
Late interrupts extend blanking. The 250 ns base pulse is an implementation
parameter; actual refresh and optical steps require
scope/panel measurements, including audio/BLE/motion load. Rev A scanning is a
separate backend and has not been ported by this change.

Artwork uses two verified chunk banks and an atomic manifest in the unchanged
32 KiB NVS partition; legacy binary records load on upgrade. There is no required
factory reset. Large main-owned workspaces live in static RAM, and BLE queues
carry a wake token rather than repeated 8 KiB artwork copies. Native interrupted
write tests cover maximum artwork, both banks and repeated garbage collection.
Target build memory is evidence of DK allocation only, not nRF52840 fit.


### Dedicated artwork store (2026-10-16)

[ADR0019](decisions/0019-large-artwork-fixed-workspaces.md) supersedes the prior
8KiB/32-frame bound. Artwork is an immutable flash view. Two132KiB raw banks use
264KiB of the DK's inert network-update reservation;24KiB remains reserved. Code,
MCUboot/update slots and the32KiB settings partition do not move. This is not the
nRF52840 layout and does not retain a full network-update reserve. The NVS
format3 manifest selects one CRC-checked bank; old inline/chunk artwork loads on
upgrade without erase. An interrupted inactive upload leaves the old bank active.

The worker writes aligned bounded fragments and erases pages lazily. Main-owned
playback uses a frame cache and indexed offsets rather than animation-size RAM
copies; settings and BLE commands hold views. Upload/Apply excludes recording
starts, rejects an already active recording/NFC transaction, and pauses local PDM
before flash operations. Wide BLE transfer/readback and metadata caching are in
[ble-protocol.md](ble-protocol.md). Existing display blanking/fault behavior and
release recording gate remain. Measure upload/erase effects under combined load;
software bounds alone do not establish timing, optical or Rev A acceptance.


### Grayscale scheduling recovery (2026-10-17)

HUB75 shifts the next row with its one-shot timer stopped and OE off, then starts
both timed OE edges. Short grayscale compares cannot be retriggered while that
GPIO shift is still executing. This prevents that path from starving the watchdog
thread; scan period now explicitly includes shifting. A35-second RAM scan check
and a93-frame imported animation stayed writable with advancing scan counters.
See [recovery validation](../validation/results/2026-10-17-upload-recovery/README.md).
Optical refresh/gray-step and combined-load acceptance remain open.

### Editor and transfer update (2026-10-17)

Matching Pixel Badge0.10.0/build28 adds per-line bouncing text, a5% global
brightness preset and sequential artwork reads. Legacy stored16 brightness
normalizes to13; accepted25/50/100% presets are unchanged. New capability bit7
gates the extended client paths. Sequential short ATT responses remove the
cursor-write round trip per page while retaining bounds, CRC, metadata and exact
Apply readback checks. Uploads remain acknowledged and use the existing inactive
bank/manifest transaction. Storage partitions and the schema are unchanged.

Normal, recording and NFC-setup DK builds pass, as do51 targeted native cases.
The recording image and app were installed without erase; persistent flash was
byte-identical immediately after deployment, with bond/NFC/healthy boot retained.
A fresh bonded artwork read transferred3551 bytes in786ms at MTU247. This does
not validate maximum-size upload speed, optics, combined load or the nRF52840 port.
See [evidence](../validation/results/2026-10-17-editor-transfer/README.md).
