/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdint.h>

namespace badge
{
class BootGuard
{
public:
	static constexpr uint32_t kHealthyMs = 30000;
	static constexpr uint8_t kLimit = 3;
	uint8_t start(uint8_t retained, bool watchdog_reset);
	bool safe_mode() const
	{
		return count_ >= kLimit;
	}
	bool healthy(uint32_t elapsed_ms); /* true once: clear retained counter */

private:
	uint8_t count_ = 0;
	bool acknowledged_ = false;
};
} // namespace badge
