# Firmware

Status: **bootstrap**. The application currently initializes logging and
prints a startup message. No product features are implemented.

## Platform

- nRF Connect SDK v3.4.1 (Zephyr RTOS)
- Initial target: `nrf5340dk/nrf5340/cpuapp`
- Language: C++17 for application and domain logic; C or C-compatible
  interfaces where Zephyr APIs require them

## Language and runtime policy

- No C++ exceptions.
- No RTTI unless explicitly justified later.
- No runtime dynamic allocation unless explicitly justified.
- Zephyr synchronization and threading primitives (`k_mutex`, `k_sem`,
  `k_msgq`, work queues), not `std::thread`/`std::mutex`.
- The build currently uses Zephyr's minimal C++ library. Whether to enable a
  full C++ standard library is TBD.
- Application-owned sources compile with `-Wextra -Werror`. These flags apply
  only to the application target, not to Zephyr or third-party modules.

## Proposed layering

Dependencies point downward only.

| Layer | Directory | Responsibility |
|---|---|---|
| Application | `firmware/src/app/` | Display modes, mode switching, top-level behavior |
| Services | `firmware/src/services/` | Hardware-independent logic and protocol handling (for example display content generation, BLE/NFC service logic, sensor data processing) |
| Drivers | `firmware/src/drivers/` | Project-specific device drivers (for example the LED matrix driver), using the Zephyr device model where it fits |
| Platform | `firmware/src/platform/` | Board and Zephyr integration glue |

Pure logic in the services layer should not depend on hardware so it can be
unit-tested on a host target. Public headers shared across layers go in
`firmware/include/`. Board overlays and custom board definitions go in
`firmware/boards/`.

The directories exist but are empty; interfaces will be defined as features are
implemented.

## Logging

- Primary: Zephyr logging with the SEGGER RTT backend (enabled in `prj.conf`).
- Fallback on the custom board: UART test pads.

No project-specific logging abstraction is used.

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

Output: `build/nrf5340dk/firmware/zephyr/zephyr.hex`.

## Tests

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

The only current test (`firmware/tests/sanity`) checks that C++17 tests build
and run; it does not test product behavior.
