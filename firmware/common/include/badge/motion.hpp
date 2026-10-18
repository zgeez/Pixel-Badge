/* SPDX-License-Identifier: MIT */
#pragma once
#include "badge/text.hpp"

namespace badge
{
constexpr size_t kMotionBytes = 8;
/* v1, auto rotation, +X/-X/+Y/-Y/+Z/-Z right/down axes, response, count,
 * manual quarter turns, reserved. Axis signs describe the physical mount. */
struct MotionConfig {
	uint8_t bytes[kMotionBytes] = {1, 1, 0, 2, 3, 96, 0, 0};
};
inline bool valid_motion(const uint8_t *bytes, size_t size)
{
	return bytes && size == kMotionBytes && bytes[0] == 1 && bytes[1] <= 1 && bytes[2] < 6 &&
	       bytes[3] < 6 && bytes[2] / 2 != bytes[3] / 2 && bytes[4] >= 1 && bytes[4] <= 5 &&
	       bytes[5] >= 16 && bytes[5] <= 192 && bytes[6] < 4 && bytes[7] == 0;
}
class MotionTracker
{
public:
	void configure(const MotionConfig &config);
	bool sample(const int32_t acceleration[3], uint32_t now);
	bool fresh(uint32_t now) const
	{
		return sampled_ && now - sampled_at_ <= 250;
	}
	Rotation rotation() const;
	int32_t x() const
	{
		return x_;
	}
	int32_t y() const
	{
		return y_;
	}
	uint32_t age(uint32_t now) const
	{
		return sampled_ ? now - sampled_at_ : UINT32_MAX;
	}

private:
	MotionConfig config_{};
	int32_t x_ = 0, y_ = 0;
	uint32_t sampled_at_ = 0, candidate_at_ = 0;
	Rotation rotation_ = Rotation::k0, candidate_ = Rotation::k0;
	bool sampled_ = false, oriented_ = false;
};
/* Fixed-point visual particle fluid, not a numerical FLIP/PIC solver. */
class Fluid
{
public:
	static constexpr unsigned kMaximum = 192;
	void reset(unsigned count);
	void tick(int32_t x, int32_t y, unsigned response, uint32_t now, bool fresh);
	void render(Frame &frame) const;
	unsigned count() const
	{
		return count_;
	}
	bool valid() const;

private:
	struct Particle {
		int16_t x, y, vx, vy;
	};
	Particle particles_[kMaximum]{};
	uint8_t occupied_[kMatrixSize * kMatrixSize]{};
	unsigned count_ = 0, step_ = 0;
	uint32_t at_ = 0;
	bool clocked_ = false;
	void step(int32_t x, int32_t y, unsigned response);
};
} // namespace badge
