/* SPDX-License-Identifier: MIT */
#include "badge/matrix_chain.hpp"
#include <zephyr/ztest.h>
#include <string.h>

using namespace badge;

namespace
{

/* Model of an n-bit shift register clocked once per bit. MISO is sampled
 * before the shift (offset n) or, with a half-cycle output skew, after it
 * (offset n - 1). prior holds the register contents before the transfer. */
void simulate(const uint8_t tx[8], uint8_t rx[8], unsigned n, bool skew, uint64_t prior)
{
	uint64_t reg = prior;
	const uint64_t top = 1ULL << (n - 1);

	memset(rx, 0, 8);
	for (size_t i = 0; i < 64; ++i) {
		int out = (reg & top) ? 1 : 0;

		reg = ((reg << 1) | static_cast<uint64_t>(stream_bit(tx, i))) & ((top << 1) - 1);
		if (skew) {
			out = (reg & top) ? 1 : 0;
		}
		rx[i / 8] |= static_cast<uint8_t>(out << (7 - i % 8));
	}
}

constexpr uint8_t kPatterns[][4] = {
	{0x80, 0x00, 0x00, 0x01}, {0xC3, 0x5A, 0x0F, 0x96}, {0x01, 0x23, 0x45, 0x67},
	{0xFE, 0xDC, 0xBA, 0x98}, {0x00, 0xFF, 0x13, 0xEC},
};

} // namespace

ZTEST(matrix_chain, test_offset_for_32_bit_chain)
{
	for (const auto &p : kPatterns) {
		uint8_t tx[8] = {}, rx[8];

		memcpy(tx, p, 4);
		simulate(tx, rx, 32, false, 0x5A5A5A5AULL);
		zassert_equal(chain_offset(tx, rx, 8, 31, 33), 32);
		simulate(tx, rx, 32, true, 0x12345678ULL);
		zassert_equal(chain_offset(tx, rx, 8, 31, 33), 31, "half-cycle output skew");
	}
}

ZTEST(matrix_chain, test_wrong_chain_length_rejected)
{
	for (const auto &p : kPatterns) {
		uint8_t tx[8] = {}, rx[8];

		memcpy(tx, p, 4);
		simulate(tx, rx, 16, false, 0); /* one TLC missing from the chain */
		zassert_equal(chain_offset(tx, rx, 8, 31, 33), -1);
	}
}

ZTEST(matrix_chain, test_stuck_miso)
{
	uint8_t tx[8] = {0xC3, 0x5A, 0x0F, 0x96};
	uint8_t low[8] = {}, high[8];

	memset(high, 0xFF, sizeof(high));
	zassert_equal(chain_offset(tx, low, 8, 31, 33), -1);
	zassert_equal(chain_offset(tx, high, 8, 31, 33), -1);
	zassert_true(stream_constant(low, 8));
	zassert_true(stream_constant(high, 8));
	zassert_false(stream_constant(tx, 8));
}

ZTEST(matrix_chain, test_stream_bit_msb_first)
{
	const uint8_t b[2] = {0x80, 0x01};

	zassert_equal(stream_bit(b, 0), 1);
	zassert_equal(stream_bit(b, 7), 0);
	zassert_equal(stream_bit(b, 15), 1);
}

ZTEST_SUITE(matrix_chain, NULL, NULL, NULL, NULL, NULL);
