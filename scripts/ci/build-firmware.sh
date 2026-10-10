#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Build the firmware application, or the application in APP_DIR (for example a
# bench test under firmware/bench). Requires the nRF Connect SDK toolchain on
# PATH and ZEPHYR_BASE pointing at the pinned SDK's zephyr directory.

set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"

APP_DIR="${APP_DIR:-${repo_root}/firmware}"
BOARD="${BOARD:-nrf5340dk/nrf5340/cpuapp}"
BUILD_DIR="${BUILD_DIR:-${repo_root}/build/nrf5340dk}"
# Optional Kconfig fragment(s) added to the application image.
EXTRA_CONF_FILE="${EXTRA_CONF_FILE:-}"

extra_args=()
if [ -n "${EXTRA_CONF_FILE}" ]; then
    extra_args+=("-DEXTRA_CONF_FILE=${EXTRA_CONF_FILE}")
fi

# BOARD_ROOT makes the Rev A board (firmware/boards) visible to sysbuild too.
west build --pristine=auto -b "${BOARD}" -d "${BUILD_DIR}" "${APP_DIR}" \
    -- -DBOARD_ROOT="${repo_root}/firmware" ${extra_args[@]+"${extra_args[@]}"}
