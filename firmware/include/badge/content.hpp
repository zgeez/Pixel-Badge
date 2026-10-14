/* SPDX-License-Identifier: MIT */
#pragma once
#include "badge/test_pattern.hpp"

namespace badge
{
constexpr size_t kTextMax = 96;
constexpr size_t kTextHeaderBytes = 6;
constexpr size_t kTextBytes = kTextHeaderBytes + kTextMax;
constexpr size_t kArtworkFrames = 16;
constexpr size_t kArtworkHeaderBytes = 4;
constexpr size_t kArtworkBytes = kArtworkHeaderBytes + kArtworkFrames * kFrameBytes;

/* Version, layout (fit/wrap/scroll), scale (auto/1/2/3), alignment,
 * pixel interval u16le, followed by printable ASCII or LF. Empty = name. */
struct TextContent {
	uint8_t bytes[kTextBytes] = {1, 0, 0, 1, 150, 0};
	uint16_t length = kTextHeaderBytes;
};
/* Version, frame count, frame interval u16le, then MSB-first 128-byte frames. */
struct Artwork {
	uint8_t bytes[kArtworkBytes] = {1, 1, 150, 0};
	uint16_t length = kArtworkHeaderBytes + kFrameBytes;
};
inline uint16_t content_interval(const uint8_t *bytes)
{
	return uint16_t(bytes[0]) | (uint16_t(bytes[1]) << 8);
}
inline bool valid_text(const uint8_t *bytes, size_t length)
{
	if (bytes == nullptr || length < kTextHeaderBytes || length > kTextBytes || bytes[0] != 1 ||
	    bytes[1] > 2 || bytes[2] > 3 || bytes[3] > 2 || content_interval(bytes + 4) < 50 ||
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
	return bytes != nullptr && length >= kArtworkHeaderBytes && bytes[0] == 1 &&
	       bytes[1] >= 1 && bytes[1] <= kArtworkFrames &&
	       length == kArtworkHeaderBytes + size_t(bytes[1]) * kFrameBytes &&
	       content_interval(bytes + 2) >= 50 && content_interval(bytes + 2) <= 2000;
}
void render_text(Frame &output, const TextContent &content, const char *fallback, uint32_t elapsed);
void render_artwork(Frame &output, const Artwork &content, uint32_t elapsed, bool animate);
bool artwork_present(const Artwork &content);
} // namespace badge
