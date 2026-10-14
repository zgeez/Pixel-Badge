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
	mode_ = available(settings_.default_mode) ? settings_.default_mode : Mode::kQr;
	entered_at_ = 0;
	current_rotation_ = qr_rotation_ = Rotation::k0;
	encode_url(settings_.url, strlen(settings_.url), qr_);
}

void Modes::next(uint32_t now)
{
	if (safe_) {
		return;
	}
	Mode next = mode_;
	do {
		next = static_cast<Mode>((static_cast<unsigned>(next) + 1) % 4);
	} while (!available(next));
	(void)select(next, now);
}

bool Modes::available(Mode mode) const
{
	return mode <= Mode::kAnimation &&
	       (mode != Mode::kCanvas || artwork_present(settings_.artwork)) &&
	       (mode != Mode::kAnimation ||
		(artwork_present(settings_.artwork) && settings_.artwork.bytes[1] >= 2));
}

bool Modes::select(Mode mode, uint32_t now)
{
	if (safe_ || !available(mode)) {
		return false;
	}
	mode_ = mode;
	entered_at_ = now;
	if (mode_ == Mode::kQr) {
		qr_rotation_ = current_rotation_;
	}
	return true;
}

bool Modes::configure(const Settings &settings)
{
	Frame qr;
	if (safe_ || !encode_url(settings.url, strlen(settings.url), qr)) {
		return false;
	}
	settings_ = settings;
	qr_ = qr;
	return true;
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
	} else if (mode_ == Mode::kName) {
		render_text(content, settings_.text, settings_.name, now - entered_at_);
	} else {
		render_artwork(content, settings_.artwork, now - entered_at_,
			       mode_ == Mode::kAnimation);
	}
	rotate(content, output, mode_ == Mode::kQr ? qr_rotation_ : current_rotation_);
}
} // namespace badge
