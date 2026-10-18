/* SPDX-License-Identifier: MIT */
#include "badge/display_health.hpp"
#include "badge/boot_guard.hpp"
#include <errno.h>

namespace badge
{
void DisplayHealth::report(int result, uint32_t now, bool intentionally_disarmed)
{
	if (intentionally_disarmed && result == -EPERM) {
		result = 0;
	}
	if (restart_required(now)) {
		return;
	}
	if (result == 0) {
		if (failed_) {
			healthy_since_ = now;
		}
		failed_ = false;
	} else if (!failed_) {
		failed_ = true;
		failed_since_ = now;
	}
}

bool DisplayHealth::restart_required(uint32_t now)
{
	if (failed_ && now - failed_since_ >= kFailureMs) {
		restart_ = true;
	}
	return restart_;
}

uint32_t DisplayHealth::healthy_ms(uint32_t now) const
{
	return failed_ || restart_ ? 0 : now - healthy_since_;
}

bool DisplayHealth::acknowledge_boot(BootGuard &guard, bool display_ready,
				     bool intentionally_disarmed, uint32_t now) const
{
	return (display_ready || intentionally_disarmed) && guard.healthy(healthy_ms(now));
}
} // namespace badge
