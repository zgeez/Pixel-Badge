/* SPDX-License-Identifier: MIT */
#pragma once
#include "badge/test_pattern.hpp"

namespace badge
{
constexpr size_t kTextMax = 96;
constexpr size_t kTextHeaderBytes = 6;
constexpr size_t kTextBytes = kTextHeaderBytes + kTextMax;
constexpr size_t kArtworkFrames = 128;
constexpr size_t kArtworkHeaderBytes = 4;
constexpr size_t kLegacyArtworkBytes = 8192;
constexpr size_t kArtworkBytes = 6 + kArtworkFrames * (1024 + 5);
inline constexpr uint8_t kBlankArtwork[132] = {1, 1, 150, 0};

/* Version, layout (fit/wrap/scroll/bounce-lines), scale (auto/1/2/3), alignment,
 * pixel interval u16le, followed by printable ASCII or LF. Empty = name. */
struct TextContent {
	uint8_t bytes[kTextBytes] = {1, 0, 0, 1, 150, 0};
	uint16_t length = kTextHeaderBytes;
};
inline bool text_animated(const TextContent &text)
{
	return text.bytes[1] == 2 || text.bytes[1] == 3;
}
/* Immutable lifetime-owned view: flash on target; caller-owned fixtures in tests.
 * Copying settings never copies an animation. Do not mutate a published view. */
struct Artwork {
	const uint8_t *bytes = kBlankArtwork;
	uint32_t length = sizeof(kBlankArtwork);
};
inline uint16_t content_interval(const uint8_t *bytes)
{
	return uint16_t(bytes[0]) | (uint16_t(bytes[1]) << 8);
}
inline bool valid_text(const uint8_t *bytes, size_t length)
{
	if (bytes == nullptr || length < kTextHeaderBytes || length > kTextBytes || bytes[0] != 1 ||
	    bytes[1] > 3 || bytes[2] > 3 || bytes[3] > 2 || content_interval(bytes + 4) < 50 ||
	    content_interval(bytes + 4) > 500) {
		return false;
	}
	for (size_t i = kTextHeaderBytes; i < length; ++i) {
		if (bytes[i] != '\n' && (bytes[i] < 0x20 || bytes[i] > 0x7e)) {
			return false;
		}
	}
	return true;
}
inline bool valid_artwork(const uint8_t *bytes, size_t length)
{
	if (bytes == nullptr || length < 4 || length > kArtworkBytes || bytes[1] < 1 ||
	    bytes[1] > kArtworkFrames || content_interval(bytes + 2) < 50 ||
	    content_interval(bytes + 2) > 2000) {
		return false;
	}
	if (bytes[0] == 1) {
		return length == 4 + size_t(bytes[1]) * kFrameBytes;
	}
	if (bytes[0] != 2 || length < 6 || bytes[4] > 3 || bytes[5] != 0) {
		return false;
	}
	size_t offset = 6;
	for (unsigned frame = 0; frame < bytes[1]; ++frame) {
		if (offset + 5 > length) {
			return false;
		}
		const unsigned mode = bytes[offset];
		const size_t size = content_interval(bytes + offset + 1);
		const unsigned duration = content_interval(bytes + offset + 3);
		if (duration < 50 || duration > 2000) {
			return false;
		}
		offset += 5;
		if (size > length - offset || mode > 3 || (mode == 3 && frame == 0)) {
			return false;
		}
		if ((mode == 0 && size != 128) || (mode == 1 && size != 1024)) {
			return false;
		}
		if (mode >= 2) {
			if (size == 0 || size % 3 != 0) {
				return false;
			}
			size_t pixels = 0;
			for (size_t i = 0; i < size; i += 3) {
				const size_t run = content_interval(bytes + offset + i);
				if (run == 0 || run > 1024 - pixels) {
					return false;
				}
				pixels += run;
			}
			if (pixels != 1024) {
				return false;
			}
		}
		offset += size;
	}
	return offset == length;
}
inline unsigned artwork_index(const Artwork &content, uint32_t elapsed, bool validate = true)
{
	const auto *bytes = content.bytes;
	if (validate && !valid_artwork(bytes, content.length)) {
		return 0;
	}
	if (bytes[0] == 1) {
		return (elapsed / content_interval(bytes + 2)) % bytes[1];
	}
	uint16_t durations[kArtworkFrames]{};
	size_t offset = 6;
	for (unsigned i = 0; i < bytes[1]; ++i) {
		durations[i] = content_interval(bytes + offset + 3);
		offset += 5 + content_interval(bytes + offset + 1);
	}
	const unsigned count = bytes[4] == 3 && bytes[1] > 1 ? 2 * bytes[1] - 2 : bytes[1];
	auto index = [bytes](unsigned i) {
		return bytes[4] == 2 ? bytes[1] - 1 - i : i >= bytes[1] ? 2 * bytes[1] - 2 - i : i;
	};
	uint32_t total = 0;
	for (unsigned i = 0; i < count; ++i) {
		total += durations[index(i)];
	}
	uint32_t at = bytes[4] == 1 && elapsed >= total ? total - 1 : elapsed % total;
	for (unsigned i = 0; i < count; ++i) {
		const unsigned frame = index(i);
		if (at < durations[frame]) {
			return frame;
		}
		at -= durations[frame];
	}
	return 0;
}
inline bool artwork_frame(const Artwork &content, unsigned index, Frame &out, bool validate = true)
{
	out.clear();
	if ((validate && !valid_artwork(content.bytes, content.length)) ||
	    index >= content.bytes[1]) {
		return false;
	}
	const uint8_t *bytes = content.bytes;
	if (bytes[0] == 1) {
		for (unsigned i = 0; i < 128; ++i) {
			out.bytes[i] = bytes[4 + index * 128 + i];
		}
		return true;
	}
	size_t offset = 6, anchor = 6;
	unsigned first = 0;
	for (unsigned frame = 0; frame <= index; ++frame) {
		if (bytes[offset] != 3) {
			anchor = offset;
			first = frame;
		}
		offset += 5 + content_interval(bytes + offset + 1);
	}
	offset = anchor;
	out.grayscale = true;
	for (unsigned frame = first; frame <= index; ++frame) {
		const unsigned mode = bytes[offset];
		const size_t size = content_interval(bytes + offset + 1);
		offset += 5;
		if (mode == 0) {
			for (unsigned i = 0; i < 1024; ++i) {
				out.levels[i] = bytes[offset + i / 8] & (128 >> (i % 8)) ? 255 : 0;
			}
		} else if (mode == 1) {
			for (unsigned i = 0; i < 1024; ++i) {
				out.levels[i] = bytes[offset + i];
			}
		} else {
			unsigned pixel = 0;
			for (size_t i = 0; i < size; i += 3) {
				const unsigned run = content_interval(bytes + offset + i);
				const uint8_t value = bytes[offset + i + 2];
				for (unsigned n = 0; n < run; ++n, ++pixel) {
					out.levels[pixel] =
						mode == 3 ? out.levels[pixel] ^ value : value;
				}
			}
		}
		offset += size;
	}
	for (unsigned i = 0; i < 1024; ++i) {
		if (out.levels[i]) {
			out.bytes[i / 8] |= 128 >> (i % 8);
		}
	}
	return true;
}

class ArtworkPlayer
{
public:
	void reset()
	{
		source_ = nullptr;
		length_ = 0;
		index_ = kArtworkFrames;
	}
	void frame(Frame &out, const Artwork &art, unsigned index);
	void render(Frame &out, const Artwork &art, uint32_t elapsed, bool animate);

private:
	const uint8_t *source_ = nullptr;
	uint32_t length_ = 0;
	unsigned index_ = kArtworkFrames;
	Frame cached_{};
};

void render_text(Frame &output, const TextContent &content, const char *fallback, uint32_t elapsed);
void render_artwork(Frame &output, const Artwork &content, uint32_t elapsed, bool animate);
bool artwork_present(const Artwork &content, bool first_frame_only = false);
} // namespace badge
