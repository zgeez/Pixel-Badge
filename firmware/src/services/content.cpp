/* SPDX-License-Identifier: MIT */
#include "badge/content.hpp"
#include "badge/font5x8.hpp"
#include <string.h>

namespace badge
{
namespace
{
void letter(Frame &frame, char character, int left, int top, int scale)
{
	const unsigned value = static_cast<unsigned char>(character);
	if (value < 32 || value > 126) {
		return;
	}
	for (int x = 0; x < 5; ++x) {
		for (int y = 0; y < 8; ++y) {
			if ((kFont5x8[(value - 32) * 5 + x] & (1U << y)) == 0) {
				continue;
			}
			for (int dy = 0; dy < scale; ++dy) {
				for (int dx = 0; dx < scale; ++dx) {
					frame.set(left + x * scale + dx, top + y * scale + dy);
				}
			}
		}
	}
}
struct Line {
	size_t start, length;
};

size_t wrap(const char *text, size_t length, size_t columns, Line *lines)
{
	size_t count = 0, cursor = 0;
	while (cursor < length && count < kTextMax + 1) {
		const size_t start = cursor;
		size_t end = start, space = length;
		while (end < length && text[end] != '\n' && end - start < columns) {
			if (text[end] == ' ') {
				space = end;
			}
			++end;
		}
		if (end < length && text[end] != '\n' && end - start == columns && space < end &&
		    space > start) {
			end = space;
		}
		lines[count++] = {start, end - start};
		cursor = end;
		if (cursor < length && (text[cursor] == '\n' || text[cursor] == ' ')) {
			++cursor;
		}
	}
	return count;
}
int aligned(int width, uint8_t alignment)
{
	return alignment == 0   ? 0
	       : alignment == 2 ? kMatrixSize - width
				: (kMatrixSize - width) / 2;
}
} // namespace

void render_text(Frame &output, const TextContent &content, const char *fallback, uint32_t elapsed)
{
	output.clear();
	if (!valid_text(content.bytes, content.length)) {
		return;
	}
	char text[kTextMax + 1]{};
	size_t length = content.length - kTextHeaderBytes;
	if (length == 0 && fallback != nullptr) {
		length = strnlen(fallback, kTextMax);
		memcpy(text, fallback, length);
	} else {
		memcpy(text, content.bytes + kTextHeaderBytes, length);
	}
	if (length == 0) {
		return;
	}
	const uint8_t layout = content.bytes[1], alignment = content.bytes[3];
	const uint16_t interval = content_interval(content.bytes + 4);
	int scale = content.bytes[2];
	Line lines[kTextMax + 1]{};
	if (scale == 0) {
		scale = 1;
		if (layout == 0) {
			/* Choose the largest size whose wrapped message fits one page. */
			for (int candidate = 3; candidate >= 1; --candidate) {
				if (wrap(text, length, kMatrixSize / (6 * candidate), lines) <=
				    static_cast<size_t>(kMatrixSize / (8 * candidate))) {
					scale = candidate;
					break;
				}
			}
		} else if (layout == 2 && strchr(text, '\n') == nullptr) {
			scale = length == 1 ? 3 : length == 2 ? 2 : 1;
		}
	}
	if (layout == 2) {
		const int width = static_cast<int>(length * 6 - 1) * scale;
		const bool scrolling = width > kMatrixSize;
		const int left = scrolling ? -static_cast<int>((elapsed / interval) % (width + 8))
					   : aligned(width, alignment);
		for (int copy = 0; copy < (scrolling ? 2 : 1); ++copy) {
			for (size_t i = 0; i < length; ++i) {
				letter(output, text[i] == '\n' ? ' ' : text[i],
				       left + copy * (width + 8) + static_cast<int>(i) * 6 * scale,
				       (kMatrixSize - 8 * scale) / 2, scale);
			}
		}
		return;
	}
	const size_t count = wrap(text, length, kMatrixSize / (6 * scale), lines);
	const size_t rows = kMatrixSize / (8 * scale);
	const size_t pages = (count + rows - 1) / rows;
	const size_t first = (elapsed / (uint32_t(interval) * 20) % pages) * rows;
	const size_t visible = count - first < rows ? count - first : rows;
	const int top = (kMatrixSize - static_cast<int>(visible) * 8 * scale) / 2;
	for (size_t row = 0; row < visible; ++row) {
		const Line line = lines[first + row];
		const int width =
			line.length == 0 ? 0 : static_cast<int>(line.length * 6 - 1) * scale;
		for (size_t i = 0; i < line.length; ++i) {
			letter(output, text[line.start + i],
			       aligned(width, alignment) + static_cast<int>(i) * 6 * scale,
			       top + static_cast<int>(row) * 8 * scale, scale);
		}
	}
}

void render_artwork(Frame &output, const Artwork &content, uint32_t elapsed, bool animate)
{
	output.clear();
	if (!valid_artwork(content.bytes, content.length)) {
		return;
	}
	const size_t index =
		animate ? (elapsed / content_interval(content.bytes + 2)) % content.bytes[1] : 0;
	memcpy(output.bytes, content.bytes + kArtworkHeaderBytes + index * kFrameBytes,
	       kFrameBytes);
}
bool artwork_present(const Artwork &content, bool first_frame_only)
{
	if (!valid_artwork(content.bytes, content.length)) {
		return false;
	}
	const size_t end = first_frame_only ? kArtworkHeaderBytes + kFrameBytes : content.length;
	for (size_t i = kArtworkHeaderBytes; i < end; ++i) {
		if (content.bytes[i] != 0) {
			return true;
		}
	}
	return false;
}
} // namespace badge
