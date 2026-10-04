#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Check (default) or fix formatting of project C/C++ sources with clang-format.
# Usage: check-format.sh [--fix]

set -euo pipefail

cd "$(dirname "$0")/../.."

mapfile -d '' files < <(git ls-files -z --cached --others --exclude-standard -- \
	'*.c' '*.h' '*.cpp' '*.hpp')

if [ "${#files[@]}" -eq 0 ]; then
	echo "No C/C++ sources found."
	exit 0
fi

clang-format --version

if [ "${1:-}" = "--fix" ]; then
	clang-format -i --style=file "${files[@]}"
else
	clang-format --dry-run --Werror --style=file "${files[@]}"
	echo "Formatting check passed (${#files[@]} files)."
fi
