/* SPDX-License-Identifier: MIT */
#include "badge/modes.hpp"

#include <string.h>

namespace badge
{
uint8_t next_brightness(uint8_t current)
{
	constexpr uint8_t levels[] = {16, 64, 128, 255};
	for (uint8_t level : levels) {
		if (current < level) {
			return level;
		}
	}
	return levels[0];
}

void Modes::start(const Settings &settings, bool safe_mode)
{
	safe_ = safe_mode;
	settings_ = safe_ ? kDefaultSettings : settings;
	mode_ = settings_.default_mode;
	entered_at_ = 0;
	current_rotation_ = qr_rotation_ = Rotation::k0;
	encode_url(settings_.url, strlen(settings_.url), qr_);
}

void Modes::next(uint32_t now)
{
	if (safe_) {
		return;
	}
	mode_ = mode_ == Mode::kQr ? Mode::kName : Mode::kQr;
	entered_at_ = now;
	if (mode_ == Mode::kQr) {
		qr_rotation_ = current_rotation_;
	}
}

void Modes::orientation(Rotation rotation)
{
	current_rotation_ = rotation;
}

void Modes::render(Frame &output, uint32_t now) const
{
	Frame content;
	if (mode_ == Mode::kQr) {
		content = qr_;
	} else {
		draw_name(content, settings_.name, now - entered_at_);
	}
	rotate(content, output, mode_ == Mode::kQr ? qr_rotation_ : current_rotation_);
}
} // namespace badge
