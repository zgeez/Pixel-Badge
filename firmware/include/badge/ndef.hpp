/* SPDX-License-Identifier: MIT */
#pragma once

#include "badge/qr.hpp"

namespace badge
{
constexpr size_t kNdefStorage = 32;
/* A short well-known URI record in a Type 2 NDEF TLV. Scheme comparison is
 * case-insensitive; the remaining URI bytes are copied exactly. */
bool encode_ndef_uri(const char *url, size_t length, uint8_t output[kNdefStorage]);
bool decode_ndef_uri(const uint8_t input[kNdefStorage], char output[kPayloadMax + 1]);
} // namespace badge
