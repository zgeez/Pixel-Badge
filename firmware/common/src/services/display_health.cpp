/* SPDX-License-Identifier: MIT */
#include "badge/display_health.hpp"

namespace badge
{
void DisplayHealth::report(int result, uint32_t now)
{
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
} // namespace badge
