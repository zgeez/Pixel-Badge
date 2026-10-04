#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Install host tools needed to build C++ tests for native_sim.
#
# The nRF Connect SDK toolchain image provides host gcc and gcc-multilib but not
# g++ or make. native_sim is a 32-bit target, so the multilib g++ is needed, and
# the native simulator runner is built with make.
#
# Packages come from a fixed Ubuntu archive snapshot with exact versions, so
# together with the digest-pinned image the test environment is reproducible.
# The g++ version matches the gcc already in the image.
# Intended for the CI toolchain container (Ubuntu 24.04, running as root).

set -euo pipefail

UBUNTU_SNAPSHOT="20260901T000000Z"
PACKAGES=(
	"g++-multilib=4:13.2.0-7ubuntu1"
	"make=4.3-4.1build2"
)

sources=/etc/apt/sources.list.d/ubuntu.sources
[ -f "${sources}" ] || {
	echo "error: ${sources} not found" >&2
	exit 1
}
sed -i -E "s#^URIs: .*#URIs: https://snapshot.ubuntu.com/ubuntu/${UBUNTU_SNAPSHOT}/#" "${sources}"

# Run apt outside the SDK toolchain environment, whose tools (for example perl)
# would otherwise shadow the system ones.
apt() {
	env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin DEBIAN_FRONTEND=noninteractive apt-get "$@"
}

apt update -qq
apt install -y -qq --no-install-recommends "${PACKAGES[@]}"
g++ --version | head -n 1
make --version | head -n 1
