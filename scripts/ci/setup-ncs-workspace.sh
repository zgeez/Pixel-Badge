#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Create or update an nRF Connect SDK west workspace at the pinned version.
# Usage: setup-ncs-workspace.sh <workspace-dir>
#
# An existing workspace (CI cache, Docker volume) is reused only if its
# manifest repository is a clean checkout of the pinned version: west does not
# move the manifest repository after `west init`. Delete the workspace to
# recreate it if this check fails.

set -euo pipefail

NCS_VERSION="v3.4.1"
NCS_MANIFEST_URL="https://github.com/nrfconnect/sdk-nrf"

workspace="${1:?usage: $0 <workspace-dir>}"

die() {
	echo "error: $*" >&2
	exit 1
}

# Mounted or cached repositories may be owned by a different user.
git config --global --add safe.directory '*'

if [ ! -d "${workspace}/.west" ]; then
	west init -m "${NCS_MANIFEST_URL}" --mr "${NCS_VERSION}" "${workspace}"
fi

cd "${workspace}"

manifest_dir="$(west config manifest.path)"
[ -n "${manifest_dir}" ] || die "workspace has no manifest.path"

origin="$(git -C "${manifest_dir}" remote get-url origin)"
[ "${origin%.git}" = "${NCS_MANIFEST_URL}" ] ||
	die "manifest repository origin is ${origin}, expected ${NCS_MANIFEST_URL}"

expected="$(git -C "${manifest_dir}" rev-parse -q --verify "refs/tags/${NCS_VERSION}^{commit}")" ||
	die "manifest repository has no tag ${NCS_VERSION}"
actual="$(git -C "${manifest_dir}" rev-parse HEAD)"
[ "${actual}" = "${expected}" ] ||
	die "manifest repository is at ${actual}, expected ${NCS_VERSION} (${expected})"

[ -z "$(git -C "${manifest_dir}" status --porcelain)" ] ||
	die "manifest repository ${manifest_dir} has local changes"

west update --narrow -o=--depth=1

# Every project must now be a clean checkout of its manifest revision.
dirty=()
while IFS= read -r project_dir; do
	[ -d "${project_dir}" ] || continue
	if [ -n "$(git -C "${project_dir}" status --porcelain --untracked-files=no)" ]; then
		dirty+=("${project_dir}")
	fi
done < <(west list -f '{abspath}')

if [ "${#dirty[@]}" -ne 0 ]; then
	die "projects with local changes: ${dirty[*]}"
fi

echo "nRF Connect SDK ${NCS_VERSION} workspace ready at $(pwd)"
