/* SPDX-License-Identifier: MIT */
#include "badge/text.hpp"
#include <zephyr/ztest.h>
#include <string.h>

using namespace badge;

ZTEST(text, test_rotation_coordinates_and_aliasing)
{
	Frame original{};
	original.set(1, 2);
	Frame frame;
	rotate(original, frame, Rotation::k90);
	zassert_true(frame.get(29, 1));
	zassert_false(frame.get(1, 2));
	rotate(frame, frame, Rotation::k90);
	zassert_true(frame.get(30, 29));
	rotate(original, frame, Rotation::k270);
	zassert_true(frame.get(2, 30));
}

ZTEST(text, test_centered_name_and_scroll_wrap)
{
	Frame frame;
	draw_name(frame, "BADGE", 0);
	zassert_true(frame.get(6, 13)); /* B top-left, width 19, height 5 */
	zassert_false(frame.get(5, 13));
	Frame lowercase;
	draw_name(lowercase, "badge", 0);
	zassert_mem_equal(frame.bytes, lowercase.bytes, sizeof(frame.bytes));
	draw_name(frame, "ABCDEFGHI", 0); /* width 35; cycle 43 pixels */
	Frame shifted;
	draw_name(shifted, "ABCDEFGHI", 150);
	zassert_not_equal(memcmp(frame.bytes, shifted.bytes, sizeof(frame.bytes)), 0);
	draw_name(shifted, "ABCDEFGHI", 43 * 150);
	zassert_mem_equal(frame.bytes, shifted.bytes, sizeof(frame.bytes));
}

ZTEST_SUITE(text, nullptr, nullptr, nullptr, nullptr, nullptr);
