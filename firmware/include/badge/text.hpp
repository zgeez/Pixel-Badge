/* SPDX-License-Identifier: MIT */
#pragma once

#include "badge/test_pattern.hpp"

namespace badge
{
enum class Rotation {
	k0,
	k90,
	k180,
	k270
};
void rotate(const Frame &source, Frame &destination, Rotation rotation);
/* Original 3x5 uppercase bitmap font, digits and common punctuation.
 * Lowercase uses uppercase glyphs; unsupported printable glyphs use '?'. */
void draw_name(Frame &frame, const char *name, uint32_t elapsed_ms);
void draw_passkey(Frame &frame, uint32_t passkey);
} // namespace badge
