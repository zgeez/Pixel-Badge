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
#if defined(CONFIG_APP_MOTION)
	motion_.configure(settings_.motion);
	fluid_.reset(settings_.motion.bytes[5]);
	current_rotation_ = motion_.rotation();
	qr_rotation_ = current_rotation_;
#endif
}

void Modes::next(uint32_t now)
{
	if (safe_) {
		return;
	}
	Mode next = mode_;
	do {
		next = static_cast<Mode>((static_cast<unsigned>(next) + 1) %
#if defined(CONFIG_APP_MOTION)
					 7
#elif defined(CONFIG_APP_AUDIO)
					 6
#else
					 4
#endif
		);
	} while (!available(next));
	(void)select(next, now);
}

bool Modes::available(Mode mode) const
{
	return mode <=
#if defined(CONFIG_APP_MOTION)
		       Mode::kFluid &&
#elif defined(CONFIG_APP_AUDIO)
		       Mode::kAudioSpectrum &&
#else
		       Mode::kAnimation &&
#endif
	       (mode != Mode::kCanvas || artwork_present(settings_.artwork, true)) &&
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
#if defined(CONFIG_APP_MOTION)
	if (mode_ == Mode::kFluid) {
		fluid_.reset(settings_.motion.bytes[5]);
	}
#endif
	return true;
}

bool Modes::configure(const Settings &settings)
{
	Frame qr;
	if (safe_ || !encode_url(settings.url, strlen(settings.url), qr)) {
		return false;
	}
#if defined(CONFIG_APP_MOTION)
	if (memcmp(settings_.motion.bytes, settings.motion.bytes, kMotionBytes) != 0) {
		motion_.configure(settings.motion);
		fluid_.reset(settings.motion.bytes[5]);
		current_rotation_ = motion_.rotation();
	}
#endif
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
#if defined(CONFIG_APP_MOTION)
	if (mode_ == Mode::kFluid) {
		if (motion_.fresh(now)) {
			fluid_.render(output);
		} else {
			draw_name(output, "IMU", 0);
		}
		return; // Gravity/particles are already in physical panel coordinates.
	}
#endif
	Frame content;
	if (mode_ == Mode::kQr) {
		content = qr_;
	} else if (mode_ == Mode::kName) {
		render_text(content, settings_.text, settings_.name, now - entered_at_);
	} else if (mode_ <= Mode::kAnimation) {
		render_artwork(content, settings_.artwork, now - entered_at_,
			       mode_ == Mode::kAnimation);
	} else {
		content.clear();
	}
	rotate(content, output, mode_ == Mode::kQr ? qr_rotation_ : current_rotation_);
}
#if defined(CONFIG_APP_MOTION)
void Modes::motion_sample(const int32_t acceleration[3], uint32_t now)
{
	if (!safe_ && motion_.sample(acceleration, now)) {
		current_rotation_ = motion_.rotation();
	}
}
void Modes::tick(uint32_t now)
{
	if (mode_ == Mode::kFluid) {
		fluid_.tick(motion_.x(), motion_.y(), settings_.motion.bytes[4], now,
			    motion_.fresh(now));
	}
}
#endif
} // namespace badge
