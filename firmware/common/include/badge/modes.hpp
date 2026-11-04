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
	bool configure(const Settings &settings);
	void next(uint32_t now);
	bool select(Mode mode, uint32_t now);
	void orientation(Rotation rotation);
	void render(Frame &output, uint32_t now) const;
#if defined(CONFIG_APP_MOTION)
	void motion_sample(const int32_t acceleration[3], uint32_t now);
	void tick(uint32_t now);
	const MotionTracker &motion() const
	{
		return motion_;
	}
#endif
	Rotation display_rotation() const
	{
		return current_rotation_;
	}
	Mode active() const
	{
		return mode_;
	}
	/* Bench screening: light the dark modules instead of the light ones. */
	void toggle_qr_polarity()
	{
		qr_inverted_ = !qr_inverted_;
	}
	bool qr_inverted() const
	{
		return qr_inverted_;
	}

private:
	bool available(Mode mode) const;
	Settings settings_ = kDefaultSettings;
	Frame qr_{};
	mutable ArtworkPlayer artwork_player_;
	Mode mode_ = Mode::kQr;
	bool safe_ = false;
	bool qr_inverted_ = false;
	Rotation current_rotation_ = Rotation::k0;
	Rotation qr_rotation_ = Rotation::k0;
	uint32_t entered_at_ = 0;
#if defined(CONFIG_APP_MOTION)
	MotionTracker motion_;
	Fluid fluid_;
#endif
};
} // namespace badge
