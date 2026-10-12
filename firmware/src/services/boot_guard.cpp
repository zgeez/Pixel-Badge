/* SPDX-License-Identifier: MIT */
#include "badge/boot_guard.hpp"

namespace badge
{
uint8_t BootGuard::start(uint8_t retained, bool watchdog_reset)
{
	const uint8_t previous = (retained & 0xf8) == 0xa0 ? retained & 7 : 0;
	count_ = watchdog_reset ? (previous < kLimit ? previous + 1 : kLimit) : 0;
	acknowledged_ = false;
	return 0xa0 | count_;
}

bool BootGuard::healthy(uint32_t elapsed_ms)
{
	if (!acknowledged_ && elapsed_ms >= kHealthyMs) {
		acknowledged_ = true;
		return true;
	}
	return false;
}
} // namespace badge
