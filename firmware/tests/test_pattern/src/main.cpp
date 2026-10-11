/*
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/ztest.h>

#include "badge/test_pattern.hpp"

using badge::Frame;
using badge::kMatrixSize;
using badge::Pattern;

namespace
{

int lit_count(const Frame &frame)
{
	int n = 0;

	for (int y = 0; y < kMatrixSize; y++) {
		for (int x = 0; x < kMatrixSize; x++) {
			n += frame.get(x, y) ? 1 : 0;
		}
	}
	return n;
}

} // namespace

ZTEST_SUITE(test_pattern, nullptr, nullptr, nullptr, nullptr, nullptr);

ZTEST(test_pattern, test_bit_layout_is_mono01_msb_first)
{
	Frame frame;

	frame.clear();
	frame.set(0, 0);
	frame.set(9, 0);
	frame.set(31, 1);
	zassert_equal(frame.bytes[0], 0x80);
	zassert_equal(frame.bytes[1], 0x40);
	zassert_equal(frame.bytes[badge::kFrameStride + 3], 0x01);
	zassert_equal(lit_count(frame), 3);

	frame.set(9, 0, false);
	zassert_equal(frame.bytes[1], 0x00);
}

ZTEST(test_pattern, test_out_of_range_is_ignored)
{
	Frame frame;

	frame.clear();
	frame.set(-1, 0);
	frame.set(0, kMatrixSize);
	frame.set(kMatrixSize, 0);
	zassert_equal(lit_count(frame), 0);
	zassert_false(frame.get(-1, 0));
	zassert_false(frame.get(kMatrixSize, kMatrixSize));
}

ZTEST(test_pattern, test_pattern_pixel_counts)
{
	Frame frame;

	badge::draw(frame, Pattern::kAllOn);
	zassert_equal(lit_count(frame), kMatrixSize * kMatrixSize);

	badge::draw(frame, Pattern::kCheckerboard);
	zassert_equal(lit_count(frame), kMatrixSize * kMatrixSize / 2);
	zassert_true(frame.get(0, 0));
	zassert_false(frame.get(1, 0));

	badge::draw(frame, Pattern::kBorder);
	zassert_equal(lit_count(frame), 4 * kMatrixSize - 4);

	/* 4 x 4 block, 12 more along the top row, 4 more down the left. */
	badge::draw(frame, Pattern::kOrientation);
	zassert_equal(lit_count(frame), 32);
	zassert_true(frame.get(15, 0));
	zassert_false(frame.get(16, 0));
	zassert_true(frame.get(0, 7));
	zassert_false(frame.get(0, 8));
}

ZTEST(test_pattern, test_row_and_column_walk)
{
	Frame frame;

	badge::draw_row(frame, 5);
	zassert_equal(lit_count(frame), kMatrixSize);
	zassert_true(frame.get(0, 5));
	zassert_true(frame.get(kMatrixSize - 1, 5));
	zassert_false(frame.get(0, 4));

	badge::draw_column(frame, 30);
	zassert_equal(lit_count(frame), kMatrixSize);
	zassert_true(frame.get(30, 0));
	zassert_false(frame.get(31, 0));
}
