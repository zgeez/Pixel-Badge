/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdint.h>

namespace badge
{
enum class Press {
	kNone,
	kShort,
	kLong
};

class Button
{
public:
	static constexpr uint32_t kDebounceMs = 30;
	static constexpr uint32_t kLongMs = 800;
	/* A button held at boot is suppressed until its first stable release. */
	void reset(bool pressed, uint32_t now);
	Press sample(bool pressed, uint32_t now);

private:
	bool raw_ = false;
	bool stable_ = false;
	bool suppress_ = false;
	bool long_sent_ = false;
	uint32_t changed_at_ = 0;
	uint32_t pressed_at_ = 0;
};
} // namespace badge
