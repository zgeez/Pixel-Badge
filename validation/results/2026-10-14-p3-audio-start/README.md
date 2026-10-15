# P3 audio - software evidence

Date:2026-10-14. Local uncommitted fw/prototype-audio, based on merged
PR24/main602bd8779e08e5a259da2d2947d63fa403491992. NCS3.4.1, nRF5340 DK
application/HUB75/PDM plus IPC radio. Flutter3.38.5, Android app0.3.0+4.
No hardware flash/install, microphone/phone recording or physical measurement
was performed during this run. No source commit/push was made.

## Implemented

Normal Level/Spectrum modes; bounded volatile capture/FFT and filtered16kHz
IMA ADPCM; P-256 matching-code app-key enrollment, fresh connection challenges,
HMAC/counter controls and fresh per-recording AES-GCM; REC full-scan capture
gate; manual/automatic stops and encrypted transport timeout cleanup; Android
Keystore/private encrypted recording, playback, deletion and explicit WAV export.
Recording remains disabled by default; a separate bench fragment enables it.
Owner bench: [P3 audio](../../../docs/audio-prototype.md).
Wire contract: [audio protocol](../../../docs/audio-protocol.md).

## Verification

- Native_sim: seven configurations /38cases pass. Additional final crypto
  suite5/5passes adds successful bidirectional P-256/HKDF enrollment:39unique
  cases across the same seven configurations. Covers RMS/FFT numerical bounds,
  IMA silence/nibble order and malformed blocks, HMAC/AES reference vectors,
  invalid EC points/output erasure, fresh challenge/counter/replay rejection,
  settings, modes and configuration BLE regressions.
- Flutter:49tests pass, including audio capability/frame parsing, fragment
  bounds, control serialization, release gate, local stop and foreground checks.
  The mocked platform boundary does not prove Keystore or RF behavior.
- Android JVM:3audio tests pass using the actual MAC, packet decryption and
  playback decoder, including tag/header/recording-context tamper rejection.
  Gradle uses the installed Android Studio JDK; the shell's Java25 fails this
  pinned Gradle/Kotlin configuration. No tool upgrade was performed.
- Normal and recording bench sysbuilds pass both cores; default .config proves
  APP_AUDIO=y, APP_AUDIO_RECORDING_BENCH unset, DEBUG_COREDUMP unset. The bench
  enables recording explicitly. Application about221kB flash/106.5kB linker RAM;
  network150384B flash/38216B RAM; MCUboot40888B/16904B runs separately.
  Slot size327168B. These are prototype linker allocations, not stack peaks or
  nRF52840 forecasts. Exact final values/artifact hashes are in the local manifest.
- Debug APK builds with only four validated public contact defines; no other
  dotenv values were copied. Source/format, YAML and repository hygiene pass.
  CI now builds default P3 plus the recording and mic-screening bench images.

Ignored local evidence: build/p3-twister/, p3-crypto-final/, p3-native-tests.log,
p3-app-tests.log, p3-recording-build.log, p3-normal-build.log, p3-apk-build.log,
p3-check.log and p3-artifacts.json. APK: app/build/app/outputs/flutter-apk/.
No captured microphone audio or credentials were saved in those logs/artifacts.

## Acceptance still open

Actual mic lane/sample/PDM clock, audio pitch and tone response, DMA STOP,
CPU/frame/stack timing, sustained RF throughput, phone Keystore lifecycle,
physical code enrollment and stop/security/failure-injection/privacy inspection,
and indoor/outdoor/direct-sun REC visibility. The external HUB75 supply has no
power-good feedback; Rev A must connect rail shedding/health to stop permission.
Release recording stays disabled until these gates pass. The nRF52840 port and
combined fluid/audio/refresh validation follow prototype firmware. Existing NFC
reset-recovery failure is separate; this run does not restore or validate it.

## Audio-tab correction follow-up

Owner reported RangeError0..3:4 opening Audio. The phone/tablet navigation
regression reproduced the four-entry title/subtitle lists; the fix supplies
both fifth entries. Pixel Badge0.3.1+5: Flutter49 and analysis pass, APK rebuilt
and installed in place on Nothing A069P. Package version confirmed; actual
Badge -> Audio navigation shows Sound and voice/Level meter without RangeError.
This is phone UI evidence only: no firmware change/flash or capture/recording
acceptance. Local evidence: p3-mic-repro.log, p3-mic-tests.log,
p3-mic-analyze.log, p3-mic-apk.log and p3-mic-phone-after.xml under build/.

## Audio control/start correction follow-up

Owner reports Level/Spectrum rejected and recording stopping immediately. The
BLE staging validator still capped modes at3; corrected to Mode::kAudioSpectrum
and regression-tested successful Apply for4/5 and invalid6 in both errno builds
(2configs/20cases pass). Runtime DMIC reads used20ms for20ms blocks despite the
SDK's asynchronous clock start; now100ms, matching screening. Non-secret capture
stage/result logging added. A successful late heartbeat preserves a latched
capture error. This timing correction still requires actual capture verification.

The app also had a reproduced stale idle-poll race that could stop a newly
started recording. Operation epochs/busy checks discard stale replies. Cleanup
preserves failure reasons and only claims Saved after a real filename is returned.
Pixel Badge0.3.2+6 adds a short explanation of enrollment and numeric non-secret
device stop status. Flutter51 and analysis pass; APK builds. Firmware bench build
flashed with --no-erase, preserving stored settings and bonds. The RTT capture
contains only partial startup, including NFC-13; it does not prove healthy
runtime or microphone activity. Wireless ADB was restored; package0.3.2/build6
was installed in place and verified, and the app reconnected to the badge.
Live meter/recording confirmation is pending the owner's foreground retest.
No microphone samples or keys logged; hardware/security/visibility gates remain.

The final bench/normal sysbuilds also queue each capture block's bounded pair
of encrypted10ms notifications before awaiting both callbacks under one40ms
completion deadline. This removes the mandatory serial completion waits; it
is not a measured throughput result. The final bench image was flashed without
erasing NVS. Default firmware still disables recording and coredumps. Phone
interaction was stopped when the owner switched away from Pixel Badge; no
recording was initiated by automation. No new hardware acceptance is claimed.


## Simple recording controls (owner amendment, ADR0016)

Owner removed separate audio enrollment and selectable duration. Audio Statev2
prepares an ephemeral session automatically on the authenticated bonded phone
connection. No persistent app key or matching-code confirmation; trust is in
the phone rather than a separately authenticated app. Wire/private storage
remain encrypted; REC and existing foreground/link/display stops remain.
Record/Stop replaces the enrollment/duration controls. Legacy settings ignored;
firmware elapsed timer and phone30,000-block/3.36MB limits removed.16MiB storage
reserve and classic WAV max13,421,772blocks (~37.28h) bound a recording. Sizes
40.32MB/hour private and115.2MB/hour WAV are calculations, not endurance evidence.
Native finish trims a partial final write to complete encrypted chunks.

Both normal and explicit recording-bench sysbuilds and APK0.3.3+7 build. Android
JVM4cases pass, including >five-minute size/WAV overflow boundary. Flutter52cases
pass; an added UI test timed out in its async harness, then was corrected to
isolate rendering. Full rerun of that last test remains pending. Earlier analysis
passes; final source hygiene/diff checks recorded separately under build/.
Automatic approval review could not approve install/flash because its usage
limit was hit. Neither deployment ran; phone/DK still have the previous candidate.
No new recording, endurance, low-storage hardware or security acceptance.


### Audio UI test teardown correction

Owner local script reproduced the10minute UI-test timeout. Explicitly pump
Flutter's fake-clock microtasks before awaiting detach and stream-close futures;
remove runAsync teardown and add a15second test timeout. The targeted UI test
passes; full53Flutter cases pass in about4seconds and analysis reports no issues.
Only test/docs changed, so existing0.3.3+7APK and matching built firmware remain
unchanged. Deployment awaits phone ADB reconnection. No hardware acceptance.
Local logs: p3-widget-timeout-fix.log, p3-widget-fix-full-tests.log and
p3-widget-fix-analyze.log in ignored build/.


Deployment completed after owner restored wireless ADB: installed0.3.3/build7,
verified package version, flashed matching recording bench with --no-erase.
Phone UI reconnects and shows Ready to record with Record, no enrollment button
or duration selector. Record was not initiated by automation; owner10s record/
stop/playback retest requested. No capture or sustained-duration acceptance.


## Phone audio queue correction

Owner reports immediate stop and then specifically Phone could not keep up.
Phone UI has four~0.1s files and confirms automatic Ready state. This identifies
the eight-packet phone overflow path; the earlier observed firmware-11 is not
proof of the recording stop cause. RTT Logger/PyLink captures obtained only
partial boot/no fresh failure records and are not live timing evidence.

Moved file/Keystore preparation before requesting capture. V2 encrypted files
wrap a fresh32-byte key once using Keystore and use that RAM key for local
packet encryption, removing per-packet containsAlias/getKey/Keystore AES calls.
64-byte authenticated header,112-byte chunks; legacy headerless files readable.
Playback/export unwrap once. File/key/plaintext cleanup remains bounded and
partial final writes are trimmed. Phone packet queue remains bounded at32
(320ms), with tests for16-packet bursts and33-packet overflow. Startup test
proves no capture command is sent while file preparation is pending.

Flutter55 pass; analysis clean; Android JVM7 pass, including wrapped-key/file/
sequence tampering, legacy-file decoding and WAV limits. APK0.3.4/build8 built,
installed in place and package verified. Phone reconnects/Ready; owner10s record/
stop/playback requested. No Record action initiated by automation. Candidate
firmware display-wait change was discarded after owner identified phone overflow;
no firmware flash in this correction. Default recording gate remains unchanged.
Source/recording endurance/security/low-storage hardware acceptance not claimed.
Local logs p3-phone-queue-tests/analyze/jvm/apk.log; ignored UI/evidence in build/.


### Owner short recording pass, build8

2026-10-14, owner report, Nothing A069P/Pixel Badge0.3.4/build8 and existing
nRF5340 DK recording-bench image: after the requested foreground Record,
speak10seconds, Stop and Play sequence, owner reports Recording and playback
work. Short functional check passed. No raw audio was collected or inspected by
automation. Sustained recording/endurance, stop/fault/privacy/security/REC
visibility and Rev A acceptance remain pending; default recording gate unchanged.


## Recording level correction

Owner supplied three quiet16kHz/mono/PCM16 WAV exports and authorized a fix.
Originals inspected read-only. Their whole-clip AC RMS levels were-47.43,
-51.41,-37.23dBFS; peaks-29.26,-37.11,-18.60dBFS, no clipping. Bench records name
MP34DT01-M, distinct from the Rev A microphone. Source had no recording gain.

Added a hardware-independent fixed-point pre-ADPCM processor: gentle30Hz DC/
rumble filter, initial8x gain, adaptive0.25x-16x range, RMS target2600counts,
fast reduction/slow increase, gain freeze in near-silence without a noise gate,
interpolated gain and block-peak limiter+/-30000. State clears on capture start/
stop. Extra scratch is320int32 values; no audio persistence or new transport.
Five native voice/DSP cases pass, covering existing codec plus quiet boost,
reset determinism, DC/silence, ungated ambience, extreme peaks and bad input.

The actual C++ processor was also run against the three exported WAVs in RAM
(no copied/derived audio files). Numerical whole-clip RMS increases11.04-17.97dB;
zero clipped processor-output samples. Output peaks were-11.99,-16.01,-8.03dBFS.
This reprocesses previously decoded audio and is not a fresh microphone capture,
listening acceptance, calibrated SPL result or proof of hardware noise/clarity.
Logs/statistics under ignored build/p3-recording-gain-*. Final flash/listening
follow-up recorded below when complete. No hardware risk closed by these tests.


Gain-correction deployment: normal and bench sysbuilds passed, as did source/
hygiene checks. Default recording/coredumps remain disabled. Bench app-core
221932Bflash/107676BRAM linker allocations; not stack peaks or a port forecast.
J-Link flash both cores succeeded with --no-erase. Owner fresh same-position/
distance10s recording requested. Existing app0.3.4+8/wire format works unchanged;
no new capture/listening acceptance is claimed yet.


### Owner recording level/clarity pass

2026-10-14, owner report, existing nRF5340 DK gain-correction bench and Nothing
A069P/Pixel Badge0.3.4+8: requested a new10s clip from the same position/distance,
then playback check for louder audio without crackling/distortion/obvious pumping.
Owner reports Louder and clear. Short functional loudness/clarity check passed.
No raw audio collected by automation. Sustained recording, failure/visibility/
security/privacy and Rev A acceptance remain pending; release gate unchanged.


## App playback and connection refinement

Pixel Badge0.4.0+9 adds a seekable native player with play/pause, played-sample
position, elapsed/total time and ten-second skips. Random block reads authenticate
both legacy and wrapped-key recordings without loading whole files. Playback
uses its own executor and pauses on background/audio-focus loss. Android keeps
the secure BLE link using a connected-device foreground service and an app-owned
Flutter engine. Recording still stops when the app leaves the foreground.
Main pages now use concise controls and contextual help. No firmware change.

Software verification: Flutter58 pass, analysis clean, Android JVM10 pass,
APK and repository checks pass. Reader tests cover legacy/new forward/back/last
block, bounds, tamper and truncation; Flutter covers slider/skip/pause, stale
poll protection and lifecycle link retention. Installed/version verified on
Nothing A069P. Initial build9 phone connection and isForeground=true service
observed. Final layout refinement installed in place. Actual phone seeking and
Home/lock/Back retention remain unverified; screen changed during automation and
further taps were stopped. No new recording/firmware flash or acceptance claim.


### Playback and background lease corrections

Owner reports app0.4.0 all good for now; this is general functional feedback,
not a new endurance/visibility/security acceptance. Follow-up audit found two
edge cases, corrected in Pixel Badge0.4.1+10. Connection service start/stop now
carry a per-link owner token, enforced by a native lease. Stale teardown cannot
stop the replacement service. The app closes the old GATT client before opening
a new one, including reconnecting the same device ID.

Playback appends bounded silence when the remaining queued tail is shorter than
the platform startup buffer, then ends at the actual audio endpoint. Nonblocking
writes and final drain fail after two seconds without progress; pause/seek/stop
still cancel the old playback worker. No plaintext export or file format change.

Flutter59, Android JVM17, analysis, APK build and repository checks pass. New
regressions exercise real link/controller replacement with a fake BLE backend,
native current/stale/failed lease ownership, a10ms tail below the startup threshold,
full-stream drain, zero-length end seek/cancellation and write/drain stalls.
Installed/version verified0.4.1/build10 on Nothing A069P, preserving app data.
No phone recording or live short-tail/drop-reconnect test during this fix;
firmware unchanged and no flash/commit/push. Existing P3 acceptance gates remain.


### PR25 recording visibility, library and session cleanup corrections

Follow-up to8b2e7a5: pending/active/draining REC always outranks an unexpired
NO REC message. The display gate checks the kind of overlay actually drawn,
so state alone cannot acknowledge a non-REC frame. A native regression models
a failed long-press immediately followed by requested, active and draining
recording while the transient deadline remains live.

Read-only Android library listing isolates entries with invalid layouts/IDs,
empty files, directories and the open capture file. Damaged entries are skipped
without modifying/deleting their bytes. Listing is outside capture/session
failure cleanup, so a malformed entry cannot finish capture or clear the key.
Valid legacy/current and later recordings remain listed.

Stop callers now await one shared cleanup future. Detach and replacement
session setup wait for it; new capture is refused while stop/detach is pending.
Device-ended recording uses the same serialized cleanup path. Regressions hold
control and native file finalization open across background/detach/reconnect,
then verify the replacement subscription and recording survive.

Verification: Flutter62/analysis clean; Android JVM19 (two new library cases);
native_sim audio4/4; normal and recording DK sysbuilds both pass; debugAPK
0.4.2+11 built. Source formatting/YAML/hygiene/whitespace pass. Default recording
and coredumps remain disabled; only the explicit bench enables recording.
No firmware flash, APK installation or phone/microphone capture in this follow-up.
The installed app/board still run the previous candidate until explicitly updated.
Visibility/security/endurance/Rev A acceptance remains open.


### PR25 foreground subscription and pending-start corrections (2026-10-15)

Base e1595ac: resume waits for shared stop cleanup, then restores the current
authenticated idle subscription without starting capture. Detach drains any
pending idle resubscribe before removing the listener. Stop changes the
operation epoch; each awaited setup step and queued/signed Start checks that
epoch, generation and foreground/stop/detach state. Capture Stop is sent
promptly; file finalization waits for pending native preparation to settle.
Returning quickly cannot revive a cancelled Start or close a newly prepared
file after cleanup. Fresh manual and button starts remain available.

Ten new Flutter regressions cover button start after completed background
cleanup, resume/detach overlap, pending idle resubscribe, delayed playback,
subscription, file setup, control queue/signing, a submitted Start response,
and replacement-session ordering. Full Flutter suite72 passes; analysis clean.
Debug APK0.4.3+12 builds. No firmware source/configuration changed this run.

CI now installs Java17 and the pinned Android SDK/Build Tools/NDK, prepares
the ignored Gradle wrapper/local.properties with Flutter config-only, runs
testDebugUnitTest and uploads XML/HTML results. A fresh local snapshot with
only tracked app files successfully restored the lockfile, generated the
wrapper/configuration and ran all19 app JVM tests. This was Windows with the
installed Android Studio JBR, not a GitHub Ubuntu/Temurin17 execution; that CI
run remains pending publication. Plugin JVM tests also passed. Repository
formatting/YAML/hygiene and git diff --check pass.

Logs: ignored app/build/pr25-lifecycle-tests.log, pr25-lifecycle-analyze.log,
pr25-lifecycle-apk.log; build/pr25-clean-check/clean-check.log and its app/build/
app/test-results/testDebugUnitTest/ XML; build/pr25-lifecycle-repo-check.log.
APK: app/build/app/outputs/flutter-apk/app-debug.apk. No APK install, firmware
flash or new phone/REC/microphone bench test. Existing P3 hardware, visibility,
security, endurance and Rev A gates remain open; recording stays default-disabled.


### PR25 button gate, capture I/O and Android CI correction (2026-10-15)

Follow-up to d3f65e9: button challenges require the recording build gate as
well as proof/security/subscription/display permission. Normal firmware falls
through to the existing one-second NO REC feedback; Stop still has precedence.
No firmware security or visibility guard was removed.

Pixel Badge0.4.4+13 disables library refresh during capture and stop cleanup.
The controller also refuses native list requests while receiving, so repeated
programmatic calls cannot queue directory scans ahead of incoming audio on the
native I/O executor. Stop finalization still refreshes the saved library.
The regression issues eight refreshes while recording, checks no native scan
and verifies refresh works afterward; the widget checks the enabled/disabled
refresh action. Flutter73 and analysis pass; debug APK builds.

Hosted CI on d3f65e9 failed before Gradle tests because sdkmanager was absent
from PATH. Add android-actions/setup-android@v4 before installing pinned SDK
packages, pinning command-line tools build15859902. The action installs tools,
accepts licenses and exports their PATH; see its
[action definition](https://github.com/android-actions/setup-android/blob/v4/action.yml).
The hosted rerun is pending owner signing/push; local YAML validation alone
does not establish hosted success. Android source/tests are unchanged, so the
previous19-case clean-snapshot JVM result is retained, not rerun here.

Normal and explicit recording DK sysbuilds both pass. Generated normal config
keeps CONFIG_APP_AUDIO_RECORDING_BENCH unset; only the explicit bench enables
it. C/C++ formatting, YAML, repository hygiene and git diff --check pass.
Logs: ignored build/pr25-gates-normal.log, pr25-gates-recording.log,
pr25-gates-repo-check.log and app/build/pr25-gates-tests.log,
pr25-gates-analyze.log, pr25-gates-apk.log. No APK install, firmware flash or
new physical NO REC, throughput or microphone acceptance in this follow-up.
Existing P3 hardware/security/visibility/endurance/Rev A gates remain open.
