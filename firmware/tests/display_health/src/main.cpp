/* SPDX-License-Identifier: MIT */
#include "badge/display_health.hpp"
#include "badge/boot_guard.hpp"

#include <zephyr/ztest.h>
#include <errno.h>

using namespace badge;

ZTEST(display_health, test_transient_busy_recovers_and_restarts_health_interval)
{
	DisplayHealth health;
	BootGuard guard;
	guard.start(0xa1, true);
	health.report(0, 0);
	health.report(-EBUSY, 29900);
	zassert_true(health.recovering());
	zassert_false(health.restart_required(30000));
	zassert_false(guard.healthy(health.healthy_ms(30000)));
	health.report(0, 30100);
	zassert_false(health.recovering());
	zassert_false(health.restart_required(30100));
	zassert_false(guard.healthy(health.healthy_ms(60099)));
	zassert_true(guard.healthy(health.healthy_ms(60100)));
	/* Subsequent frames remain usable, not permanently disabled. */
	health.report(-EAGAIN, 60200);
	health.report(0, 60300);
	zassert_false(health.recovering());
}

ZTEST(display_health, test_persistent_errors_have_a_latched_deadline)
{
	const int errors[] = {-EBUSY, -EIO, -ENODEV};
	for (int error : errors) {
		DisplayHealth health;
		health.report(error, 100);
		for (uint32_t now = 200; now < 2100; now += DisplayHealth::kRetryMs) {
			health.report(error, now);
			zassert_false(health.restart_required(now));
		}
		zassert_true(health.restart_required(2100));
		health.report(0, 2200);
		zassert_true(health.restart_required(2200));
		zassert_equal(health.healthy_ms(40000), 0);
	}
}

ZTEST(display_health, test_initialization_retry_and_time_wrap)
{
	DisplayHealth health;
	health.report(-ENODEV, 0);
	health.report(0, 100);
	zassert_false(health.restart_required(2000));
	zassert_equal(health.healthy_ms(2000), 1900);
	health.report(-EBUSY, UINT32_MAX - 999);
	zassert_false(health.restart_required(999));
	zassert_true(health.restart_required(1000));

	DisplayHealth recovered;
	recovered.report(-EIO, UINT32_MAX - 999);
	recovered.report(0, 500);
	zassert_false(recovered.restart_required(1000));
	zassert_equal(recovered.healthy_ms(1000), 500);
}

ZTEST_SUITE(display_health, nullptr, nullptr, nullptr, nullptr, nullptr);
