/* SPDX-License-Identifier: MIT */
#pragma once

#include "badge/test_pattern.hpp"

namespace badge
{

constexpr size_t kPayloadMax = 20;

/* ASCII HTTP(S) URL, kept byte-for-byte (including case-sensitive paths).
 * Output is replaced only on success: V1-M, 21 modules at offset (5,5). */
bool encode_url(const char *text, size_t length, Frame &output);
bool valid_url(const char *text, size_t length);

} // namespace badge
