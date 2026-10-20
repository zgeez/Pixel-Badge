/* SPDX-License-Identifier: MIT */
#include "badge/power_policy.hpp"
#include <zephyr/ztest.h>

using namespace badge::power;

namespace
{
struct Feed {
	LoadShedPolicy policy;
	uint32_t now = 0;

	/* One sample per step_ms, as the power manager takes them. */
	void run(int vbat_mv, bool loaded, uint32_t for_ms, uint32_t step_ms = 100,
		 bool usb = false, int limit_ma = -1)
	{
		for (uint32_t t = 0; t < for_ms; t += step_ms) {
			Sample s;
			s.valid = true;
			s.vbat_mv = vbat_mv;
			s.loaded = loaded;
			s.usb_present = usb;
			s.input_limit_ma = limit_ma;
			s.now_ms = now;
			policy.update(s);
			now += step_ms;
		}
	}
	void invalid()
	{
		Sample s;
		s.now_ms = now;
		policy.update(s);
		now += 100;
	}
	/* Shed from normal: three loaded samples below 3.40 V. */
	void shed()
	{
		run(3390, true, 300);
	}
};
} // namespace

ZTEST(power_policy, test_duty_follows_driver_timing)
{
	zassert_equal(matrix_duty_permille(0), 0);
	/* 255: 1020 on-ticks in 6 x 256 + 320 + 576 slot ticks. */
	zassert_equal(matrix_duty_permille(255), 420);
	/* 127: 501 on-ticks in 7 x 256 + 318. */
	zassert_equal(matrix_duty_permille(kCapLevel), 238);
	zassert_true(matrix_duty_permille(1) > 0, "level 1 still lights");
}

ZTEST(power_policy, test_input_estimate_and_budget)
{
	zassert_equal(estimated_input_ma(0), 69, "logic only");
	zassert_equal(estimated_input_ma(255), 495);
	zassert_equal(input_budget_level(1500), 255, "1.35 A x 80 % covers full brightness");
	const uint8_t at_500 = input_budget_level(500);

	zassert_true(at_500 > kCapLevel && at_500 < 200, "500 mA level %u", at_500);
	zassert_true(estimated_input_ma(at_500) <= 360);
	zassert_true(estimated_input_ma(at_500 + 1) > 360);
	zassert_equal(input_budget_level(100), 0, "100 mA does not cover the logic load");
}

ZTEST(power_policy, test_no_reading_holds_rail_off)
{
	Feed f;

	zassert_equal(f.policy.ceiling(), 0);
	f.invalid();
	zassert_equal(f.policy.ceiling(), 0);
	f.run(4000, false, 100);
	zassert_equal(f.policy.state(), ShedState::kNormal);
	zassert_equal(f.policy.ceiling(), 255);
	f.invalid();
	zassert_equal(f.policy.ceiling(), 0, "an invalid reading turns the rail off");
	f.run(4000, true, 100);
	zassert_equal(f.policy.ceiling(), 255);
}

ZTEST(power_policy, test_cap_then_shed)
{
	Feed f;

	f.run(3900, false, 100);
	f.run(3560, true, 1000);
	zassert_equal(f.policy.ceiling(), 255);
	f.run(3540, true, 100);
	zassert_equal(f.policy.ceiling(), kCapLevel, "loaded below 3.55 V caps");
	f.run(3600, true, 1000);
	zassert_equal(f.policy.ceiling(), kCapLevel, "the cap holds without recovery");
	f.run(3390, true, 200);
	f.run(3410, true, 100);
	f.run(3390, true, 200);
	zassert_equal(f.policy.state(), ShedState::kNormal, "two low samples are not a shed");
	f.run(3390, true, 100);
	zassert_equal(f.policy.state(), ShedState::kShed);
	zassert_equal(f.policy.ceiling(), 0);
	zassert_equal(f.policy.failures(), 0, "the first shed is not a failed attempt");
}

ZTEST(power_policy, test_unloaded_low_boot_starts_shed)
{
	Feed f;

	f.run(3350, false, 100);
	zassert_equal(f.policy.state(), ShedState::kShed);
	zassert_equal(f.policy.ceiling(), 0);
}

ZTEST(power_policy, test_battery_recovery_ramps)
{
	Feed f;

	f.run(3900, false, 100);
	f.shed();
	f.run(3750, false, kBatteryOkMs - 100, 1000);
	zassert_equal(f.policy.state(), ShedState::kShed, "needs 10 s at 3.70 V");
	f.run(3750, false, 2000, 1000);
	zassert_equal(f.policy.state(), ShedState::kRamping);
	zassert_equal(f.policy.ceiling(), 13, "re-enable starts at the lowest level");
	f.run(3750, true, kRampStepMs);
	zassert_equal(f.policy.ceiling(), 64);
	f.run(3750, true, kRampStepMs);
	zassert_equal(f.policy.ceiling(), kCapLevel);
	f.run(3750, true, kRampStepMs);
	zassert_equal(f.policy.state(), ShedState::kNormal);
	zassert_equal(f.policy.ceiling(), 255, "battery-qualified recovery lifts the cap");
}

ZTEST(power_policy, test_input_recovery_needs_limit)
{
	Feed f;

	f.run(3900, false, 100);
	f.shed();
	f.run(3550, false, 1000, 100, true, 100);
	zassert_equal(f.policy.state(), ShedState::kShed, "USB at 100 mA does not qualify");
	f.run(3450, false, 1000, 100, true, 1500);
	zassert_equal(f.policy.state(), ShedState::kShed, "VBAT below 3.50 V does not qualify");
	f.run(3550, false, 100, 100, true, 1500);
	zassert_equal(f.policy.state(), ShedState::kRamping);
	f.run(3550, true, 3 * kRampStepMs, 100, true, 1500);
	zassert_equal(f.policy.ceiling(), kCapLevel, "input recovery keeps the low-battery cap");

	Feed g;

	g.run(3900, false, 100);
	g.shed();
	g.run(3550, false, 100, 100, true, 500);
	g.run(3550, true, 3 * kRampStepMs, 100, true, 500);
	zassert_true(g.policy.ceiling() <= input_budget_level(500));
}

ZTEST(power_policy, test_failed_attempts_back_off)
{
	Feed f;

	f.run(3900, false, 100);
	f.shed();
	/* Attempt 1 qualifies at once on input, then sheds within 60 s. */
	f.run(3550, false, 100, 100, true, 1500);
	zassert_equal(f.policy.state(), ShedState::kRamping);
	f.shed();
	zassert_equal(f.policy.failures(), 1);
	f.run(3550, false, kBackoffMs - 200, 100, true, 1500);
	zassert_equal(f.policy.state(), ShedState::kShed, "60 s backoff");
	f.run(3550, false, 300, 100, true, 1500);
	zassert_equal(f.policy.state(), ShedState::kRamping);
	f.shed();
	zassert_equal(f.policy.failures(), 2);
	f.run(3550, false, 2 * kBackoffMs - 200, 100, true, 1500);
	zassert_equal(f.policy.state(), ShedState::kShed, "backoff doubles");
	f.run(3550, false, 300, 100, true, 1500);
	zassert_equal(f.policy.state(), ShedState::kRamping);
	f.shed();
	zassert_equal(f.policy.failures(), 3);
	f.run(3550, false, 30 * 60000, 1000, true, 1500);
	zassert_equal(f.policy.state(), ShedState::kShed, "input recovery locked out");
	f.run(3750, false, kBatteryOkMs + 1000, 1000, true, 1500);
	zassert_equal(f.policy.state(), ShedState::kRamping, "battery-qualified still recovers");
	f.run(3750, true, kRetryWindowMs + 100);
	zassert_equal(f.policy.failures(), 0, "an attempt that lasts 60 s clears failures");
}

/* Waits on a qualifying battery until the policy re-enables; returns ms. */
static uint32_t wait_reenable(Feed &f)
{
	uint32_t waited = 0;

	while (f.policy.state() == ShedState::kShed && waited < 3600000) {
		f.run(3750, false, 1000, 1000);
		waited += 1000;
	}
	return waited;
}

ZTEST(power_policy, test_backoff_caps_at_15_min)
{
	Feed f;

	f.run(3900, false, 100);
	f.shed();
	for (int i = 0; i < 7; ++i) {
		(void)wait_reenable(f);
		zassert_equal(f.policy.state(), ShedState::kRamping, "attempt %d", i);
		f.shed(); /* fails inside its 60 s window */
	}
	zassert_equal(f.policy.failures(), 7);
	const uint32_t waited = wait_reenable(f);

	zassert_true(waited >= kBackoffMaxMs && waited <= kBackoffMaxMs + 2000,
		     "15 min ceiling, waited %u ms", waited);
}

ZTEST(power_policy, test_low_power_only_unloaded_on_battery)
{
	Feed f;

	f.run(3900, false, 100);
	f.run(3290, true, 100);
	zassert_false(f.policy.low_power(), "loaded readings shed first");
	f.run(3290, false, 100, 100, true, 100);
	zassert_false(f.policy.low_power(), "USB present");
	f.run(3290, false, 200);
	zassert_false(f.policy.low_power(), "needs three readings in a row");
	f.run(3290, false, 100);
	zassert_true(f.policy.low_power());
	f.invalid();
	zassert_false(f.policy.low_power(), "never on an invalid reading");
}

ZTEST(power_policy, test_invalid_readings_abort_reenable)
{
	Feed f;

	f.run(3900, false, 100);
	f.shed();
	f.run(3750, false, kBatteryOkMs + 1000, 1000);
	zassert_equal(f.policy.state(), ShedState::kRamping);
	for (int i = 0; i < 700; ++i) { /* 70 s outage */
		f.invalid();
	}
	zassert_equal(f.policy.state(), ShedState::kShed, "attempt aborted, not completed");
	f.run(3750, true, 100);
	zassert_equal(f.policy.ceiling(), 0, "a loaded reading after the outage does not ramp");
	f.run(3750, false, kBatteryOkMs + 1000, 1000);
	zassert_equal(f.policy.state(), ShedState::kRamping);
	zassert_equal(f.policy.ceiling(), 13, "ramp restarts from the lowest level");
}

ZTEST(power_policy, test_invalid_readings_keep_failures)
{
	Feed f;

	f.run(3900, false, 100);
	f.shed();
	f.run(3550, false, 100, 100, true, 1500);
	f.shed();
	zassert_equal(f.policy.failures(), 1);
	f.run(3550, false, kBackoffMs + 200, 100, true, 1500);
	zassert_equal(f.policy.state(), ShedState::kRamping);
	for (int i = 0; i < 700; ++i) {
		f.invalid();
	}
	zassert_equal(f.policy.failures(), 1, "an outage is not a successful attempt");
}

ZTEST(power_policy, test_invalid_normal_reading_requalifies)
{
	Feed f;

	f.run(3900, false, 100);
	zassert_equal(f.policy.ceiling(), 255);
	f.invalid();
	f.run(3390, false, 100);
	zassert_equal(f.policy.state(), ShedState::kShed, "low first reading after a gap sheds");
	zassert_equal(f.policy.ceiling(), 0);

	Feed g;

	g.run(3900, false, 100);
	g.run(3540, true, 100);
	zassert_equal(g.policy.ceiling(), kCapLevel);
	g.invalid();
	g.run(3600, false, 100);
	zassert_equal(g.policy.ceiling(), kCapLevel, "the cap survives the gap");
}

ZTEST_SUITE(power_policy, NULL, NULL, NULL, NULL, NULL);
