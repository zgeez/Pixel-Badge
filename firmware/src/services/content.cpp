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
	if (layout == 3) {
		size_t count = 0, start = 0;
		int longest = 0;
		for (size_t i = 0; i <= length; ++i) {
			if (i == length || text[i] == '\n') {
				lines[count++] = {start, i - start};
				const int travel = static_cast<int>((i - start) * 6) * scale -
						   scale - kMatrixSize;
				if (travel > longest) {
					longest = travel;
				}
				start = i + 1;
			}
		}
		const size_t rows = kMatrixSize / (8 * scale), pages = (count + rows - 1) / rows;
		/* All overflowing lines share a phase; their travel sets their speed.
		 * Three physical pixels of overshoot replace endpoint holds. */
		const uint32_t half_ms = uint32_t(longest + 6) * interval;
		const uint32_t cycle_ms = longest == 0 ? 40U * interval : 2U * half_ms;
		const uint32_t phase = elapsed % cycle_ms;
		const uint32_t progress = phase <= half_ms ? phase : cycle_ms - phase;
		const size_t first = (elapsed / cycle_ms % pages) * rows;
		const size_t visible = count - first < rows ? count - first : rows;
		const int top = (kMatrixSize - static_cast<int>(visible) * 8 * scale) / 2;
		for (size_t row = 0; row < visible; ++row) {
			const Line line = lines[first + row];
			const int width = line.length == 0
						  ? 0
						  : static_cast<int>(line.length * 6 - 1) * scale;
			const int travel = width > kMatrixSize ? width - kMatrixSize : 0;
			const int shift = static_cast<int>(
				(uint64_t(travel + 6) * progress + half_ms / 2) / half_ms);
			const int left = travel == 0 ? aligned(width, alignment) : 3 - shift;
			for (size_t i = 0; i < line.length; ++i) {
				letter(output, text[line.start + i],
				       left + static_cast<int>(i) * 6 * scale,
				       top + static_cast<int>(row) * 8 * scale, scale);
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

void ArtworkPlayer::frame(Frame &out, const Artwork &art, unsigned index)
{
	if (art.bytes == nullptr || art.length < 4) {
		reset();
		out.clear();
		return;
	}
	if (source_ != art.bytes || length_ != art.length) {
		reset();
		if (!valid_artwork(art.bytes, art.length)) {
			out.clear();
			return;
		}
		source_ = art.bytes;
		length_ = art.length;
	}
	if (index >= art.bytes[1]) {
		out.clear();
		return;
	}
	if (index != index_) {
		size_t offset = 6;
		if (art.bytes[0] == 2 && index == index_ + 1) {
			for (unsigned i = 0; i < index; i++) {
				offset += 5 + content_interval(art.bytes + offset + 1);
			}
		}
		if (art.bytes[0] == 2 && index == index_ + 1 && art.bytes[offset] == 3) {
			const size_t size = content_interval(art.bytes + offset + 1);
			offset += 5;
			unsigned pixel = 0;
			for (size_t i = 0; i < size; i += 3) {
				const unsigned run = content_interval(art.bytes + offset + i);
				const uint8_t shade = art.bytes[offset + i + 2];
				for (unsigned n = 0; n < run; n++, pixel++) {
					cached_.levels[pixel] ^= shade;
				}
			}
			memset(cached_.bytes, 0, sizeof(cached_.bytes));
			for (unsigned i = 0; i < 1024; i++) {
				if (cached_.levels[i]) {
					cached_.bytes[i / 8] |= 128 >> (i % 8);
				}
			}
		} else {
			(void)artwork_frame(art, index, cached_, false);
		}
		index_ = index;
	}
	out = cached_;
}
void ArtworkPlayer::render(Frame &out, const Artwork &art, uint32_t elapsed, bool animate)
{
	if (art.bytes == nullptr || art.length < 4) {
		reset();
		out.clear();
		return;
	}
	if (source_ != art.bytes || length_ != art.length) {
		reset();
		if (!valid_artwork(art.bytes, art.length)) {
			out.clear();
			return;
		}
		source_ = art.bytes;
		length_ = art.length;
	}
	frame(out, art, animate ? artwork_index(art, elapsed, false) : 0);
}
void render_artwork(Frame &output, const Artwork &content, uint32_t elapsed, bool animate)
{
	if (!valid_artwork(content.bytes, content.length)) {
		output.clear();
		return;
	}
	const unsigned index = animate ? artwork_index(content, elapsed, false) : 0;
	(void)artwork_frame(content, index, output, false);
}
bool artwork_present(const Artwork &content, bool first_frame_only)
{
	if (!valid_artwork(content.bytes, content.length)) {
		return false;
	}
	Frame frame;
	ArtworkPlayer player;
	const unsigned count = first_frame_only ? 1 : content.bytes[1];
	for (unsigned i = 0; i < count; ++i) {
		player.frame(frame, content, i);
		for (unsigned n = 0; n < kFrameBytes; ++n) {
			if (frame.bytes[n]) {
				return true;
			}
		}
	}
	return false;
}
} // namespace badge
