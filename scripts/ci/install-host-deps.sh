#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Install host tools needed to build C++ tests for native_sim.
#
# The nRF Connect SDK toolchain image provides host gcc and gcc-multilib but not
# g++ or make. native_sim is a 32-bit target, so the multilib g++ is needed, and
# the native simulator runner is built with make.
#
# The complete set of packages this installs into the digest-pinned image is
# locked to exact versions below, including the g++-13 compiler packages that
# g++-multilib pulls in. Packages come from a fixed Ubuntu archive snapshot. If
# the snapshot service cannot be reached, the regular Ubuntu archive is tried
# instead. Either way, apt must plan exactly the locked set at the locked
# versions, and the installed versions are checked afterwards. If the archive no
# longer carries a locked version, the job fails rather than install something
# else. The g++ version matches the gcc already in the image.
# Intended for the CI toolchain container (Ubuntu 24.04, running as root).

set -euo pipefail

UBUNTU_SNAPSHOT="20260901T000000Z"
SNAPSHOT_URI="https://snapshot.ubuntu.com/ubuntu/${UBUNTU_SNAPSHOT}/"
ARCHIVE_URI="http://archive.ubuntu.com/ubuntu/"
# Everything apt installs for g++-multilib and make in this image, as installed
# from the snapshot above.
LOCK=(
	"g++-multilib=4:13.2.0-7ubuntu1"
	"g++=4:13.2.0-7ubuntu1"
	"g++-x86-64-linux-gnu=4:13.2.0-7ubuntu1"
	"g++-13=13.3.0-6ubuntu2~24.04.1"
	"g++-13-multilib=13.3.0-6ubuntu2~24.04.1"
	"g++-13-x86-64-linux-gnu=13.3.0-6ubuntu2~24.04.1"
	"libstdc++-13-dev=13.3.0-6ubuntu2~24.04.1"
	"lib32stdc++-13-dev=13.3.0-6ubuntu2~24.04.1"
	"libx32stdc++-13-dev=13.3.0-6ubuntu2~24.04.1"
	"make=4.3-4.1build2"
)

sources=/etc/apt/sources.list.d/ubuntu.sources
[ -f "${sources}" ] || {
	echo "error: ${sources} not found" >&2
	exit 1
}

use_mirror() {
	sed -i -E "s#^URIs: .*#URIs: $1#" "${sources}"
}

# Run apt outside the SDK toolchain environment, whose tools (for example perl)
# would otherwise shadow the system ones.
apt() {
	env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin DEBIAN_FRONTEND=noninteractive apt-get "$@"
}

# --error-on=any makes a failed index download an error instead of a warning,
# so an unreachable mirror is detected here rather than as a missing package.
update_index() {
	apt -o Acquire::Retries=3 update -qq --error-on=any
}

use_mirror "${SNAPSHOT_URI}"
if ! update_index; then
	echo "warning: Ubuntu snapshot ${UBUNTU_SNAPSHOT} unavailable;" \
		"trying ${ARCHIVE_URI} with the same locked package set" >&2
	use_mirror "${ARCHIVE_URI}"
	update_index
fi

# The install plan must be exactly the locked set: nothing added, upgraded or at
# another version.
expected="$(printf '%s\n' "${LOCK[@]}" | sort)"
plan="$(apt -s install --no-install-recommends "${LOCK[@]}" |
	awk '/^Inst /{ name = $2; sub(/:amd64$/, "", name); print name "=" substr($3, 2) }' | sort)"
if [ "${plan}" != "${expected}" ]; then
	echo "error: apt's install plan differs from the locked package set" >&2
	diff <(echo "${expected}") <(echo "${plan}") >&2 || true
	exit 1
fi

apt install -y -qq --no-install-recommends "${LOCK[@]}"

for entry in "${LOCK[@]}"; do
	name="${entry%%=*}"
	want="${entry#*=}"
	have="$(dpkg-query -W -f='${Version}' "${name}")"
	[ "${have}" = "${want}" ] || {
		echo "error: ${name} ${have} installed, ${want} locked" >&2
		exit 1
	}
done

# Print the first line of each version. Capturing the full output avoids
# SIGPIPE: `head` exits early and, under pipefail, would fail the script.
for tool in g++ make; do
	version="$("${tool}" --version)"
	echo "${version%%$'\n'*}"
done
