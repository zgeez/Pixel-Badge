/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Four active-low row bytes shift through both TLCs into the HC595 chain;
 * four active-high column bytes remain in the TLCs. MSB first, SPI mode0. */
static inline bool badge_matrix_encode_row(uint8_t out[8], const uint8_t levels[32], unsigned row,
					   unsigned plane)
{
	uint32_t rows = UINT32_MAX, columns = 0;
	const bool valid = row < 32 && plane < 8;
	if (valid) {
		rows &= ~(UINT32_C(1) << row);
		for (unsigned x = 0; x < 32; ++x) {
			if (levels[x] & (1U << plane)) {
				columns |= UINT32_C(1) << x;
			}
		}
	}
	for (unsigned i = 0; i < 4; ++i) {
		out[i] = rows >> (24 - 8 * i);
		out[i + 4] = columns >> (24 - 8 * i);
	}
	return valid;
}
