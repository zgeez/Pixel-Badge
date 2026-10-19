/* SPDX-License-Identifier: MIT */
#include "badge/buttons.hpp"

namespace badge
{
void Button::reset(bool pressed, uint32_t now)
{
	raw_ = stable_ = suppress_ = pressed;
	long_sent_ = false;
	changed_at_ = pressed_at_ = now;
}

Press Button::sample(bool pressed, uint32_t now)
{
	if (pressed != raw_) {
		raw_ = pressed;
		changed_at_ = now;
	}
	if (raw_ != stable_ && now - changed_at_ >= kDebounceMs) {
		stable_ = raw_;
		if (stable_) {
			pressed_at_ = changed_at_;
			long_sent_ = false;
		} else {
			const bool short_press = !suppress_ && !long_sent_;
			suppress_ = false;
			/* A release observed at/after the threshold is long even when
			 * a delayed polling iteration missed the held-button event. */
			if (short_press) {
				return changed_at_ - pressed_at_ >= kLongMs ? Press::kLong
									    : Press::kShort;
			}
		}
	}
	if (stable_ && raw_ && !suppress_ && !long_sent_ && now - pressed_at_ >= kLongMs) {
		long_sent_ = true;
		return Press::kLong;
	}
	return Press::kNone;
}
} // namespace badge
