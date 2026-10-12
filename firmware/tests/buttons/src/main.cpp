/* SPDX-License-Identifier: MIT */
#include "badge/buttons.hpp"
#include <zephyr/ztest.h>

using namespace badge;

ZTEST(buttons, test_bounce_and_short_release)
{
	Button button;
	button.reset(false, 0);
	zassert_equal(button.sample(true, 10), Press::kNone);
	zassert_equal(button.sample(false, 20), Press::kNone);
	zassert_equal(button.sample(true, 25), Press::kNone);
	zassert_equal(button.sample(true, 55), Press::kNone);
	zassert_equal(button.sample(false, 200), Press::kNone);
	zassert_equal(button.sample(true, 210), Press::kNone);
	zassert_equal(button.sample(false, 215), Press::kNone);
	zassert_equal(button.sample(false, 245), Press::kShort);
	zassert_equal(button.sample(false, 300), Press::kNone);
}

ZTEST(buttons, test_long_once_no_short_afterward)
{
	Button button;
	button.reset(false, 0);
	button.sample(true, 100);
	button.sample(true, 130);
	zassert_equal(button.sample(true, 899), Press::kNone);
	zassert_equal(button.sample(true, 900), Press::kLong);
	zassert_equal(button.sample(true, 1500), Press::kNone);
	button.sample(false, 1600);
	zassert_equal(button.sample(false, 1630), Press::kNone);
}

ZTEST(buttons, test_boot_hold_wrap_and_late_release)
{
	Button button;
	button.reset(true, 0);
	zassert_equal(button.sample(true, 2000), Press::kNone);
	button.sample(false, 2010);
	zassert_equal(button.sample(false, 2040), Press::kNone);
	button.reset(false, UINT32_MAX - 200);
	button.sample(true, UINT32_MAX - 100);
	button.sample(true, UINT32_MAX - 70);
	button.sample(false, 100);
	zassert_equal(button.sample(false, 130), Press::kShort);
	button.reset(false, 0);
	button.sample(true, 100);
	button.sample(true, 130);
	button.sample(false, 950);
	zassert_equal(button.sample(false, 980), Press::kLong);
}

ZTEST_SUITE(buttons, nullptr, nullptr, nullptr, nullptr, nullptr);
