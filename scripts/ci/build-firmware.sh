#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Build the firmware application, or the application in APP_DIR (for example a
# bench test under firmware/nrf5340_prototype/bench). Requires the nRF Connect SDK toolchain on
# PATH and ZEPHYR_BASE pointing at the pinned SDK's zephyr directory.

set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"

APP_DIR="${APP_DIR:-${repo_root}/firmware/nrf5340_prototype}"
BOARD="${BOARD:-nrf5340dk/nrf5340/cpuapp}"
BUILD_DIR="${BUILD_DIR:-${repo_root}/build/nrf5340dk}"
# Optional Kconfig fragment(s) added to the application image.
EXTRA_CONF_FILE="${EXTRA_CONF_FILE:-}"

extra_args=()
if [ -n "${SB_EXTRA_CONF_FILE:-}" ]; then
    extra_args+=("-DSB_EXTRA_CONF_FILE=${SB_EXTRA_CONF_FILE}")
fi
if [ -n "${EXTRA_CONF_FILE}" ]; then
    # Sysbuild otherwise forwards a global fragment to MCUboot as well.
    if [ "$(cd "${APP_DIR}" && pwd)" = "${repo_root}/firmware/nrf5340_prototype" ] || [ "$(cd "${APP_DIR}" && pwd)" = "${repo_root}/firmware/rev_a_nrf52840" ]; then
        extra_args+=("-D$(basename "${APP_DIR}")_EXTRA_CONF_FILE=${EXTRA_CONF_FILE}")
    else
        extra_args+=("-DEXTRA_CONF_FILE=${EXTRA_CONF_FILE}")
    fi
fi

# BOARD_ROOT makes the selected application's board definitions visible to sysbuild.
# Zephyr also reads EXTRA_CONF_FILE from the environment in every image.
# Pass it only through the image-scoped command argument prepared above.
env -u EXTRA_CONF_FILE -u SB_EXTRA_CONF_FILE west build --sysbuild --pristine=auto -b "${BOARD}" -d "${BUILD_DIR}" "${APP_DIR}" \
    -- -DBOARD_ROOT="${APP_DIR}" ${extra_args[@]+"${extra_args[@]}"}
