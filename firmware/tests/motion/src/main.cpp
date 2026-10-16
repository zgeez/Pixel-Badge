/* SPDX-License-Identifier: MIT */
#include "badge/modes.hpp"
#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <string.h>
using namespace badge;
namespace
{
void hold(MotionTracker &tracker, int32_t x, int32_t y, int32_t z, uint32_t start)
{
	const int32_t a[] = {x, y, z};
	for (uint32_t dt = 0; dt <= 800; dt += 20) {
		zassert_true(tracker.sample(a, start + dt));
	}
}
unsigned pixels(const Frame &frame)
{
	unsigned count = 0;
	for (int y = 0; y < 32; ++y) {
		for (int x = 0; x < 32; ++x) {
			count += frame.get(x, y);
		}
	}
	return count;
}
unsigned y_sum(const Frame &frame)
{
	unsigned sum = 0;
	for (int y = 0; y < 32; ++y) {
		for (int x = 0; x < 32; ++x) {
			if (frame.get(x, y)) {
				sum += y;
			}
		}
	}
	return sum;
}
} // namespace
ZTEST(motion, test_all_mounts_and_payload_validation)
{
	unsigned mounts = 0;
	const int32_t a[] = {1000, -2000, 9500};
	for (unsigned right = 0; right < 6; ++right) {
		for (unsigned down = 0; down < 6; ++down) {
			MotionConfig config;
			config.bytes[2] = right;
			config.bytes[3] = down;
			zassert_equal(valid_motion(config.bytes, kMotionBytes),
				      right / 2 != down / 2);
			if (right / 2 == down / 2) {
				continue;
			}
			++mounts;
			MotionTracker tracker;
			tracker.configure(config);
			zassert_true(tracker.sample(a, 0));
			zassert_equal(tracker.x(), right & 1 ? a[right / 2] : -a[right / 2]);
			zassert_equal(tracker.y(), down & 1 ? a[down / 2] : -a[down / 2]);
		}
	}
	zassert_equal(mounts, 24);
	MotionConfig config;
	zassert_false(valid_motion(config.bytes, 7));
	config.bytes[7] = 1;
	zassert_false(valid_motion(config.bytes, 8));
	config.bytes[7] = 0;
	config.bytes[5] = 193;
	zassert_false(valid_motion(config.bytes, 8));
}
ZTEST(motion, test_four_orientations_dwell_flat_and_jerk)
{
	MotionTracker tracker;
	hold(tracker, 0, -9807, 0, 0);
	zassert_equal(tracker.rotation(), Rotation::k0);
	const int32_t flipped[] = {0, 9807, 0};
	for (uint32_t t = 820; t <= 920; t += 20) {
		tracker.sample(flipped, t);
	}
	zassert_equal(tracker.rotation(), Rotation::k0); // Filter plus dwell, never immediate.
	hold(tracker, 0, 9807, 0, 940);
	zassert_equal(tracker.rotation(), Rotation::k180);
	hold(tracker, 9807, 0, 0, 1760);
	zassert_equal(tracker.rotation(), Rotation::k90);
	hold(tracker, 0, 0, 9807, 2580);
	zassert_equal(tracker.rotation(), Rotation::k90);
	hold(tracker, 20000, 20000, 0, 3400);
	zassert_equal(tracker.rotation(), Rotation::k90);
	hold(tracker, -9807, 0, 0, 4220);
	zassert_equal(tracker.rotation(), Rotation::k270);
	hold(tracker, -6900, -6900, 0, 5040);
	zassert_equal(tracker.rotation(), Rotation::k270);
}
ZTEST(motion, test_manual_freshness_wrap_and_invalid_samples)
{
	MotionConfig config;
	config.bytes[1] = 0;
	config.bytes[6] = 2;
	MotionTracker tracker;
	tracker.configure(config);
	zassert_equal(tracker.rotation(), Rotation::k180);
	zassert_false(tracker.fresh(0));
	const int32_t a[] = {0, -9807, 0};
	tracker.sample(a, UINT32_MAX - 10);
	zassert_true(tracker.fresh(20));
	zassert_false(tracker.fresh(300));
	const int32_t bad[] = {INT32_MIN, 0, 0};
	zassert_false(tracker.sample(bad, 301));
	zassert_false(tracker.sample(nullptr, 302));
	zassert_false(tracker.fresh(302));
	zassert_equal(tracker.rotation(), Rotation::k180);
}
ZTEST(motion, test_fluid_conserves_pixels_and_follows_gravity)
{
	Fluid fluid;
	fluid.reset(96);
	fluid.tick(0, 9807, 3, 0, true);
	for (uint32_t t = 20; t <= 8000; t += 20) {
		fluid.tick(0, 9807, 3, t, true);
		zassert_true(fluid.valid());
	}
	Frame below, above;
	fluid.render(below);
	zassert_equal(pixels(below), 96);
	for (uint32_t t = 8020; t <= 16000; t += 20) {
		fluid.tick(0, -9807, 3, t, true);
		zassert_true(fluid.valid());
	}
	fluid.render(above);
	zassert_equal(pixels(above), 96);
	zassert_true(y_sum(below) > y_sum(above) + 96 * 10);
}
ZTEST(motion, test_maximum_stress_and_bounded_stall_recovery)
{
	Fluid fluid;
	fluid.reset(1000);
	zassert_equal(fluid.count(), Fluid::kMaximum);
	for (uint32_t t = 0; t < 200000; t += 20) {
		fluid.tick(t & 1024 ? 16000 : -16000, t & 2048 ? 16000 : -16000, 5, t, true);
		if (t % 400 == 0) {
			zassert_true(fluid.valid());
		}
	}
	Frame before, after;
	fluid.render(before);
	fluid.tick(9807, 0, 5, 300000, false);
	fluid.render(after);
	zassert_mem_equal(before.bytes, after.bytes, sizeof(before.bytes));
	fluid.tick(9807, 0, 5, 400000, true);
	fluid.render(after);
	zassert_mem_equal(before.bytes, after.bytes, sizeof(before.bytes));
	zassert_true(fluid.valid());
	zassert_equal(pixels(after), 192);
}
ZTEST(motion, test_modes_lock_qr_and_freeze_unavailable_fluid)
{
	Modes modes;
	modes.start(kDefaultSettings, false);
	Frame qr, held, rotated;
	modes.render(qr, 0);
	const int32_t sideways[] = {9807, 0, 0};
	for (uint32_t t = 0; t < 800; t += 20) {
		modes.motion_sample(sideways, t);
		modes.tick(t);
	}
	zassert_equal(modes.display_rotation(), Rotation::k90);
	modes.render(held, 800);
	zassert_mem_equal(qr.bytes, held.bytes, sizeof(qr.bytes));
	zassert_true(modes.select(Mode::kName, 800));
	zassert_true(modes.select(Mode::kQr, 820));
	modes.render(held, 820);
	rotate(qr, rotated, Rotation::k90);
	zassert_mem_equal(held.bytes, rotated.bytes, sizeof(held.bytes));
	zassert_true(modes.select(Mode::kFluid, 840));
	modes.tick(840);
	modes.render(held, 840);
	zassert_equal(pixels(held), 96);
	modes.tick(2000);
	modes.render(held, 2000);
	Frame unavailable;
	draw_name(unavailable, "IMU", 0);
	zassert_mem_equal(held.bytes, unavailable.bytes, sizeof(held.bytes));
	modes.start(kDefaultSettings, true);
	zassert_false(modes.select(Mode::kFluid, 0));
	modes.motion_sample(sideways, 0);
	modes.render(held, 0);
	zassert_mem_equal(held.bytes, qr.bytes, sizeof(held.bytes));
}
ZTEST(motion, test_manual_setting_preserves_qr_until_reentry)
{
	Modes modes;
	modes.start(kDefaultSettings, false);
	Frame before, after, expected;
	modes.render(before, 0);
	Settings settings = kDefaultSettings;
	settings.motion.bytes[1] = 0;
	settings.motion.bytes[6] = 3;
	zassert_true(modes.configure(settings));
	modes.render(after, 20);
	zassert_mem_equal(before.bytes, after.bytes, sizeof(before.bytes));
	modes.select(Mode::kName, 40);
	modes.select(Mode::kQr, 60);
	modes.render(after, 60);
	rotate(before, expected, Rotation::k270);
	zassert_mem_equal(expected.bytes, after.bytes, sizeof(after.bytes));
}
ZTEST_SUITE(motion, nullptr, nullptr, nullptr, nullptr, nullptr);
