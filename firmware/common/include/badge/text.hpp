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
/* A short label centred horizontally with its top row at `top`, drawn over the
 * frame without clearing it: scale 2 up to four characters, otherwise 1. */
void draw_label(Frame &frame, const char *label, int top);
void draw_passkey(Frame &frame, uint32_t passkey);
} // namespace badge
