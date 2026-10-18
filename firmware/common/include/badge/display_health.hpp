/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdint.h>

namespace badge
{
/* Main-loop policy: a delayed frame is retried; an uninterrupted failure
 * interval eventually stops watchdog feeding. Success restarts health timing. */
class DisplayHealth
{
public:
	static constexpr uint32_t kRetryMs = 100;
	static constexpr uint32_t kFailureMs = 2000;
	/* Recovery must clear a driver's latched fault, then prove scanning resumed.
	 * An accepted initialization alone must not reset the failure deadline. */
	template <typename Submit, typename ConfirmVisible>
	int present(bool ready, Submit submit, ConfirmVisible confirm_visible) const
	{
		const int result = submit(!ready || recovering());
		return result == 0 && recovering() ? confirm_visible() : result;
	}
	void report(int result, uint32_t now);
	bool restart_required(uint32_t now);
	bool recovering() const
	{
		return failed_;
	}
	uint32_t healthy_ms(uint32_t now) const;

private:
	bool failed_ = false;
	bool restart_ = false;
	uint32_t failed_since_ = 0;
	uint32_t healthy_since_ = 0;
};
} // namespace badge
