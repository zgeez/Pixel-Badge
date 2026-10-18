/*
 * SPDX-License-Identifier: MIT
 */

#include "badge/test_pattern.hpp"

#include <string.h>

namespace badge
{

void Frame::clear()
{
	memset(bytes, 0, sizeof(bytes));
	memset(levels, 0, sizeof(levels));
	grayscale = false;
}

void Frame::set(int x, int y, bool lit)
{
	if (x < 0 || y < 0 || x >= kMatrixSize || y >= kMatrixSize) {
		return;
	}
	if (grayscale) {
		levels[y * kMatrixSize + x] = lit ? 255 : 0;
	}
	uint8_t &b = bytes[y * kFrameStride + x / 8];
	const uint8_t mask = static_cast<uint8_t>(0x80U >> (x % 8));

	b = lit ? static_cast<uint8_t>(b | mask) : static_cast<uint8_t>(b & ~mask);
}

bool Frame::get(int x, int y) const
{
	if (x < 0 || y < 0 || x >= kMatrixSize || y >= kMatrixSize) {
		return false;
	}
	return (bytes[y * kFrameStride + x / 8] & (0x80U >> (x % 8))) != 0;
}

uint8_t Frame::level(int x, int y) const
{
	if (x < 0 || y < 0 || x >= kMatrixSize || y >= kMatrixSize) {
		return 0;
	}
	return grayscale ? levels[y * kMatrixSize + x] : (get(x, y) ? 255 : 0);
}
void Frame::set_level(int x, int y, uint8_t value)
{
	if (x < 0 || y < 0 || x >= kMatrixSize || y >= kMatrixSize) {
		return;
	}
	if (!grayscale) {
		for (int yy = 0; yy < kMatrixSize; ++yy) {
			for (int xx = 0; xx < kMatrixSize; ++xx) {
				levels[yy * kMatrixSize + xx] = get(xx, yy) ? 255 : 0;
			}
		}
		grayscale = true;
	}
	set(x, y, value != 0);
	levels[y * kMatrixSize + x] = value;
}
void draw(Frame &frame, Pattern pattern)
{
	frame.clear();
	for (int y = 0; y < kMatrixSize; y++) {
		for (int x = 0; x < kMatrixSize; x++) {
			bool lit = false;

			switch (pattern) {
			case Pattern::kAllOn:
				lit = true;
				break;
			case Pattern::kCheckerboard:
				lit = ((x + y) % 2) == 0;
				break;
			case Pattern::kBorder:
				lit = x == 0 || y == 0 || x == kMatrixSize - 1 ||
				      y == kMatrixSize - 1;
				break;
			case Pattern::kOrientation:
				/* 4 x 4 block at the origin, a long arm along the top row
				 * and a short arm down the left column. */
				lit = (x < 4 && y < 4) || (y == 0 && x < 16) || (x == 0 && y < 8);
				break;
			}
			frame.set(x, y, lit);
		}
	}
}

void draw_row(Frame &frame, int y)
{
	frame.clear();
	for (int x = 0; x < kMatrixSize; x++) {
		frame.set(x, y);
	}
}

void draw_column(Frame &frame, int x)
{
	frame.clear();
	for (int y = 0; y < kMatrixSize; y++) {
		frame.set(x, y);
	}
}

} // namespace badge
