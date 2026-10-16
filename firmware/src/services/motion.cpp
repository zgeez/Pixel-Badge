/* SPDX-License-Identifier: MIT */
#include "badge/motion.hpp"
#include <string.h>

namespace badge
{
namespace
{
int32_t clamp(int32_t value, int32_t low, int32_t high)
{
	return value < low ? low : value > high ? high : value;
}
int32_t magnitude(int32_t value)
{
	return value < 0 ? -value : value;
}
int32_t project(const int32_t a[3], unsigned axis)
{
	// An accelerometer measures support force; gravity is the opposite vector.
	return axis & 1 ? a[axis / 2] : -a[axis / 2];
}
unsigned cell(int32_t x, int32_t y)
{
	return (y / 256) * 32 + x / 256;
}
} // namespace
void MotionTracker::configure(const MotionConfig &config)
{
	if (!valid_motion(config.bytes, kMotionBytes)) {
		return;
	}
	config_ = config;
	sampled_ = oriented_ = false;
	x_ = y_ = 0;
	rotation_ = candidate_ = Rotation::k0;
}
Rotation MotionTracker::rotation() const
{
	return config_.bytes[1] ? rotation_ : static_cast<Rotation>(config_.bytes[6]);
}
bool MotionTracker::sample(const int32_t a[3], uint32_t now)
{
	if (!a || a[0] < -40000 || a[0] > 40000 || a[1] < -40000 || a[1] > 40000 || a[2] < -40000 ||
	    a[2] > 40000) {
		return false;
	}
	const int32_t raw_x = project(a, config_.bytes[2]), raw_y = project(a, config_.bytes[3]);
	const bool continuous = fresh(now);
	x_ = continuous ? x_ + (raw_x - x_) / 4 : raw_x;
	y_ = continuous ? y_ + (raw_y - y_) / 4 : raw_y;
	sampled_ = true;
	sampled_at_ = now;
	const int64_t norm = int64_t(a[0]) * a[0] + int64_t(a[1]) * a[1] + int64_t(a[2]) * a[2];
	const int32_t ax = magnitude(x_), ay = magnitude(y_);
	// Flat poses, diagonal boundaries and large dynamic acceleration hold the last
	// orientation. A candidate must remain dominant for 250 ms after initial lock.
	if (norm < 6000LL * 6000 || norm > 14000LL * 14000 || (ax < 4000 && ay < 4000) ||
	    (ax * 10 < ay * 14 && ay * 10 < ax * 14)) {
		candidate_ = rotation_;
		candidate_at_ = now;
		return true;
	}
	const Rotation next = ax > ay ? (x_ > 0 ? Rotation::k270 : Rotation::k90)
				      : (y_ > 0 ? Rotation::k0 : Rotation::k180);
	if (!oriented_) {
		rotation_ = candidate_ = next;
		candidate_at_ = now;
		oriented_ = true;
	} else if (!continuous || next != candidate_) {
		candidate_ = next;
		candidate_at_ = now;
	} else if (now - candidate_at_ >= 250) {
		rotation_ = next;
	}
	return true;
}
void Fluid::reset(unsigned count)
{
	count_ = count > kMaximum ? kMaximum : count;
	memset(occupied_, 0, sizeof(occupied_));
	memset(particles_, 0, sizeof(particles_));
	for (unsigned i = 0; i < count_; ++i) {
		auto &p = particles_[i];
		p.x = (8 + i % 16) * 256 + 128;
		p.y = (4 + i / 16) * 256 + 128;
		occupied_[cell(p.x, p.y)] = 1;
	}
	clocked_ = false;
	step_ = 0;
}
void Fluid::step(int32_t x, int32_t y, unsigned response)
{
	x = clamp(x, -20000, 20000) * static_cast<int32_t>(response) / 3000;
	y = clamp(y, -20000, 20000) * static_cast<int32_t>(response) / 3000;
	for (unsigned n = 0; n < count_; ++n) {
		auto &p = particles_[step_ & 1 ? n : count_ - 1 - n];
		occupied_[cell(p.x, p.y)] = 0;
		p.vx = clamp((p.vx * 255) / 256 + x, -192, 192);
		p.vy = clamp((p.vy * 255) / 256 + y, -192, 192);
		// Subpixel steps cannot jump over a full occupied cell. Alternate axis
		// priority each step to reduce grid-direction bias without unbounded work.
		for (unsigned axis = 0; axis < 2; ++axis) {
			const bool horizontal = ((axis + step_) & 1) == 0;
			const int32_t nx = p.x + (horizontal ? p.vx : 0);
			const int32_t ny = p.y + (horizontal ? 0 : p.vy);
			if (nx >= 0 && nx < 8192 && ny >= 0 && ny < 8192 &&
			    !occupied_[cell(nx, ny)]) {
				p.x = nx;
				p.y = ny;
			} else if (horizontal) {
				p.vx = -p.vx / 4;
				if (magnitude(x) > magnitude(y)) {
					p.vy += (n + step_) & 1 ? 16 : -16;
				}
			} else {
				p.vy = -p.vy / 4;
				if (magnitude(y) > magnitude(x)) {
					p.vx += (n + step_) & 1 ? 16 : -16;
				}
			}
		}
		occupied_[cell(p.x, p.y)] = 1;
	}
	++step_;
}
void Fluid::tick(int32_t x, int32_t y, unsigned response, uint32_t now, bool fresh)
{
	if (!clocked_ || !fresh || now - at_ > 100) {
		at_ = now;
		clocked_ = true;
		return;
	}
	unsigned budget = 4;
	while (now - at_ >= 20 && budget--) {
		step(x, y, response > 5 ? 5 : response);
		at_ += 20;
	}
}
void Fluid::render(Frame &frame) const
{
	frame.clear();
	for (unsigned i = 0; i < count_; ++i) {
		const auto &p = particles_[i];
		frame.set(p.x / 256, p.y / 256);
	}
}
bool Fluid::valid() const
{
	uint8_t seen[1024]{};
	for (unsigned i = 0; i < count_; ++i) {
		const auto &p = particles_[i];
		if (p.x < 0 || p.x >= 8192 || p.y < 0 || p.y >= 8192 || seen[cell(p.x, p.y)]++) {
			return false;
		}
	}
	return memcmp(seen, occupied_, sizeof(seen)) == 0;
}
} // namespace badge
