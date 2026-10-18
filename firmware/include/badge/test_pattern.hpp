/*
 * SPDX-License-Identifier: MIT
 *
 * 32 x 32 one-bit frames and display test patterns. Hardware independent.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace badge
{

constexpr int kMatrixSize = 32;
constexpr size_t kFrameStride = kMatrixSize / 8;
constexpr size_t kFrameBytes = kMatrixSize * kFrameStride;

/* Rows top to bottom; in each byte the most significant bit is the leftmost
 * pixel; 1 = lit. Matches Zephyr's PIXEL_FORMAT_MONO01 with
 * SCREEN_INFO_MONO_MSB_FIRST. */
struct Frame {
	uint8_t bytes[kFrameBytes];
	uint8_t levels[kMatrixSize * kMatrixSize]{};
	bool grayscale = false;
	uint8_t level(int x, int y) const;
	void set_level(int x, int y, uint8_t value);

	void clear();
	void set(int x, int y, bool lit = true);
	bool get(int x, int y) const;
};

enum class Pattern {
	kAllOn,
	kCheckerboard,
	kBorder,
	/* Asymmetric marker in the top-left corner, to check orientation. */
	kOrientation,
};

void draw(Frame &frame, Pattern pattern);
void draw_row(Frame &frame, int y);
void draw_column(Frame &frame, int x);

} // namespace badge
