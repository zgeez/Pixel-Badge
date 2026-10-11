/* SPDX-License-Identifier: MIT */
#include "badge/boot_guard.hpp"
#include <zephyr/ztest.h>

using namespace badge;

ZTEST(boot_guard, test_loop_threshold_and_healthy_latch)
{
	BootGuard guard;
	uint8_t retained = guard.start(0xff, false);
	zassert_false(guard.safe_mode());
	for (int i = 1; i <= 3; ++i) {
		retained = guard.start(retained, true);
		zassert_equal(guard.safe_mode(), i == 3);
	}
	zassert_false(guard.healthy(29999));
	zassert_true(guard.healthy(30000));
	zassert_false(guard.healthy(31000));
	guard.start(retained, false);
	zassert_false(guard.safe_mode());
}

ZTEST(boot_guard, test_malformed_retention_and_saturation)
{
	BootGuard guard;
	const uint8_t first = guard.start(0xff, true);
	zassert_equal(first, 0xa1);
	zassert_false(guard.safe_mode());
	zassert_equal(guard.start(0xa7, true), 0xa3);
	zassert_true(guard.safe_mode());
}

ZTEST_SUITE(boot_guard, nullptr, nullptr, nullptr, nullptr, nullptr);
