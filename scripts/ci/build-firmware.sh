#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Build the firmware application. Requires the nRF Connect SDK toolchain on
# PATH and ZEPHYR_BASE pointing at the pinned SDK's zephyr directory.

set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"

BOARD="${BOARD:-nrf5340dk/nrf5340/cpuapp}"
BUILD_DIR="${BUILD_DIR:-${repo_root}/build/nrf5340dk}"

west build --pristine=auto -b "${BOARD}" -d "${BUILD_DIR}" "${repo_root}/firmware"
