/* SPDX-License-Identifier: MIT */
#include "badge/text.hpp"

#include <string.h>

namespace badge
{
namespace
{
struct Glyph {
	char character;
	uint16_t pixels;
};
/* Five rows of three bits, top row first. */
constexpr Glyph font[] = {
	{'A', 0b010101111101101},
	{'B', 0b110101110101110},
	{'C', 0b011100100100011},
	{'D', 0b110101101101110},
	{'E', 0b111100110100111},
	{'F', 0b111100110100100},
	{'G', 0b011100101101011},
	{'H', 0b101101111101101},
	{'I', 0b111010010010111},
	{'J', 0b001001001101010},
	{'K', 0b101101110101101},
	{'L', 0b100100100100111},
	{'M', 0b101111111101101},
	{'N', 0b101111111111101},
	{'O', 0b010101101101010},
	{'P', 0b110101110100100},
	{'Q', 0b010101101111011},
	{'R', 0b110101110101101},
	{'S', 0b011100010001110},
	{'T', 0b111010010010010},
	{'U', 0b101101101101111},
	{'V', 0b101101101101010},
	{'W', 0b101101111111101},
	{'X', 0b101101010101101},
	{'Y', 0b101101010010010},
	{'Z', 0b111001010100111},
	{'0', 0b111101101101111},
	{'1', 0b010110010010111},
	{'2', 0b110001010100111},
	{'3', 0b110001010001110},
	{'4', 0b101101111001001},
	{'5', 0b111100110001110},
	{'6', 0b011100110101010},
	{'7', 0b111001010010010},
	{'8', 0b010101010101010},
	{'9', 0b010101011001110},
	{' ', 0},
	{'.', 0b000000000000010},
	{'-', 0b000000111000000},
	{'_', 0b000000000000111},
	{'/', 0b001001010100100},
	{':', 0b000010000010000},
	{'!', 0b010010010000010},
	{'?', 0b110001010000010},
	{'\'', 0b010010000000000},
};

uint16_t glyph(char character)
{
	if (character >= 'a' && character <= 'z') {
		character -= 'a' - 'A';
	}
	for (const auto &entry : font) {
		if (entry.character == character) {
			return entry.pixels;
		}
	}
	return 0b110001010000010;
}
} // namespace

void rotate(const Frame &source, Frame &destination, Rotation rotation)
{
	Frame rotated{};
	for (int y = 0; y < kMatrixSize; ++y) {
		for (int x = 0; x < kMatrixSize; ++x) {
			switch (rotation) {
			case Rotation::k90:
				rotated.set(31 - y, x, source.get(x, y));
				break;
			case Rotation::k180:
				rotated.set(31 - x, 31 - y, source.get(x, y));
				break;
			case Rotation::k270:
				rotated.set(y, 31 - x, source.get(x, y));
				break;
			default:
				rotated.set(x, y, source.get(x, y));
			}
		}
	}
	destination = rotated;
}

void draw_name(Frame &frame, const char *name, uint32_t elapsed_ms)
{
	frame.clear();
	if (name == nullptr || name[0] == '\0') {
		return;
	}
	const size_t length = strlen(name);
	const int scale = length <= 4 ? 2 : 1;
	const int width = static_cast<int>(length * 4 - 1) * scale;
	const bool scrolling = width > kMatrixSize;
	const int start = scrolling ? -static_cast<int>((elapsed_ms / 150) % (width + 8))
				    : (kMatrixSize - width) / 2;
	const int top = (kMatrixSize - 5 * scale) / 2;
	/* A second copy follows the eight-pixel gap for seamless wraparound. */
	for (int copy = 0; copy < (scrolling ? 2 : 1); ++copy) {
		for (size_t i = 0; i < length; ++i) {
			const uint16_t pixels = glyph(name[i]);
			for (int y = 0; y < 5; ++y) {
				for (int x = 0; x < 3; ++x) {
					if ((pixels & (1U << (14 - y * 3 - x))) == 0) {
						continue;
					}
					for (int dy = 0; dy < scale; ++dy) {
						for (int dx = 0; dx < scale; ++dx) {
							frame.set(start + copy * (width + 8) +
									  static_cast<int>(i) * 4 *
										  scale +
									  x * scale + dx,
								  top + y * scale + dy);
						}
					}
				}
			}
		}
	}
}
} // namespace badge
