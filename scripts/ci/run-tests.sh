#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Run the Ztest suites under firmware/tests with Twister on a host platform.
# native_sim requires a Linux host. Twister builds tests with warnings treated
# as errors by default.

set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"

TEST_PLATFORM="${TEST_PLATFORM:-native_sim}"
TWISTER_OUT="${TWISTER_OUT:-${repo_root}/build/twister}"

west twister \
	--testsuite-root "${repo_root}/firmware/tests" \
	--platform "${TEST_PLATFORM}" \
	--outdir "${TWISTER_OUT}" \
	--inline-logs \
	--verbose
