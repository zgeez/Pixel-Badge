/*
 * SPDX-License-Identifier: MIT
 *
 * Rev A LED-driver chain readback helpers. MTX_MOSI feeds U601 SIN; U601
 * SOUT feeds U602; U602 SOUT is both MTX_MISO and the translator input to
 * the HC595 row chain. With the LED rail off the translator's VLED side is
 * unpowered, so shifting only exercises the two 16-bit TLC59283 registers:
 * a bit sent at stream position i returns on MISO at about i + 32.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace badge
{

constexpr int kTlcChainBits = 32;

/* Bit i of an MSB-first byte stream. */
constexpr int stream_bit(const uint8_t *bytes, size_t i)
{
	return (bytes[i / 8] >> (7 - i % 8)) & 1;
}

/* Returns the smallest offset in [min_offset, max_offset] for which every
 * received bit rx[i + offset] equals tx[i] (for all i with i + offset inside
 * the stream), or -1 if none matches. len is the stream length in bytes. */
constexpr int chain_offset(const uint8_t *tx, const uint8_t *rx, size_t len, int min_offset,
			   int max_offset)
{
	const size_t bits = len * 8;

	for (int offset = min_offset; offset <= max_offset; ++offset) {
		if (offset < 0 || static_cast<size_t>(offset) >= bits) {
			continue;
		}
		bool match = true;

		for (size_t i = 0; i + static_cast<size_t>(offset) < bits && match; ++i) {
			match = stream_bit(tx, i) ==
				stream_bit(rx, i + static_cast<size_t>(offset));
		}
		if (match) {
			return offset;
		}
	}
	return -1;
}

/* A MISO line stuck at one level reads all zeros or all ones. */
constexpr bool stream_constant(const uint8_t *rx, size_t len)
{
	for (size_t i = 1; i < len; ++i) {
		if (rx[i] != rx[0]) {
			return false;
		}
	}
	return len == 0 || rx[0] == 0x00 || rx[0] == 0xFF;
}

} // namespace badge
