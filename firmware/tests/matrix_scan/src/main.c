/* SPDX-License-Identifier: MIT */
#include "badge/matrix_scan.h"
#include <zephyr/ztest.h>
#include <zephyr/sys/byteorder.h>
#include <string.h>
ZTEST(matrix_scan, test_active_low_row_chain_and_each_column_plane)
{
	uint8_t out[8], pixels[32];
	for (unsigned row = 0; row < 32; ++row) {
		for (unsigned plane = 0; plane < 8; ++plane) {
			for (unsigned col = 0; col < 32; ++col) {
				memset(pixels, 0, sizeof(pixels));
				pixels[col] = 1U << plane;
				zassert_true(badge_matrix_encode_row(out, pixels, row, plane));
				zassert_equal(sys_get_be32(out), ~(UINT32_C(1) << row));
				zassert_equal(sys_get_be32(out + 4), UINT32_C(1) << col);
			}
		}
	}
}
ZTEST(matrix_scan, test_bounds_fail_to_all_rows_and_columns_off)
{
	uint8_t out[8], pixels[32];
	memset(pixels, 255, sizeof(pixels));
	zassert_false(badge_matrix_encode_row(out, pixels, 32, 0));
	zassert_equal(sys_get_be32(out), UINT32_MAX);
	zassert_equal(sys_get_be32(out + 4), 0);
	zassert_false(badge_matrix_encode_row(out, pixels, 0, 8));
	zassert_equal(sys_get_be32(out), UINT32_MAX);
	zassert_equal(sys_get_be32(out + 4), 0);
}
ZTEST_SUITE(matrix_scan, NULL, NULL, NULL, NULL, NULL);
