/* SPDX-License-Identifier: MIT */
#pragma once

#include "badge/settings.hpp"
#include "badge/text.hpp"

namespace badge
{
uint8_t next_brightness(uint8_t current);

class Modes
{
public:
	void start(const Settings &settings, bool safe_mode);
	void next(uint32_t now);
	void orientation(Rotation rotation);
	void render(Frame &output, uint32_t now) const;
	Mode active() const
	{
		return mode_;
	}

private:
	Settings settings_ = kDefaultSettings;
	Frame qr_{};
	Mode mode_ = Mode::kQr;
	bool safe_ = false;
	Rotation current_rotation_ = Rotation::k0;
	Rotation qr_rotation_ = Rotation::k0;
	uint32_t entered_at_ = 0;
};
} // namespace badge
