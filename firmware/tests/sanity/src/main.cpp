/*
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/ztest.h>

static_assert(__cplusplus >= 201703L, "tests must build as C++17 or later");

ZTEST_SUITE(sanity, nullptr, nullptr, nullptr, nullptr, nullptr);

ZTEST(sanity, test_constexpr_lambda)
{
	/* constexpr lambdas are a C++17 feature. */
	constexpr auto square = [](int x) constexpr { return x * x; };

	static_assert(square(4) == 16);
	zassert_equal(square(3), 9);
}
