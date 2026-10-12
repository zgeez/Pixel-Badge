/* SPDX-License-Identifier: MIT */
#include "badge/ndef.hpp"
#include <string.h>

namespace badge
{
bool encode_ndef_uri(const char *url, size_t length, uint8_t output[kNdefStorage])
{
	if (!valid_url(url, length)) {
		return false;
	}
	/* valid_url has already checked the case-insensitive HTTP(S) scheme. */
	const bool secure = url[4] != ':';
	const size_t prefix = secure ? 8 : 7;
	const size_t remaining = length - prefix;
	uint8_t encoded[kNdefStorage] = {0x03,
					 static_cast<uint8_t>(remaining + 5),
					 0xd1,
					 0x01,
					 static_cast<uint8_t>(remaining + 1),
					 'U',
					 static_cast<uint8_t>(secure ? 0x04 : 0x03)};
	memcpy(encoded + 7, url + prefix, remaining);
	encoded[7 + remaining] = 0xfe;
	memcpy(output, encoded, sizeof(encoded));
	return true;
}
bool decode_ndef_uri(const uint8_t input[kNdefStorage], char output[kPayloadMax + 1])
{
	if (input[0] != 3 || input[2] != 0xd1 || input[3] != 1 || input[5] != 'U' ||
	    (input[6] != 3 && input[6] != 4) || input[4] == 0) {
		return false;
	}
	const size_t remaining = input[4] - 1;
	const char *prefix = input[6] == 4 ? "HTTPS://" : "HTTP://";
	const size_t prefix_length = strlen(prefix);
	if (7 + remaining >= kNdefStorage || prefix_length + remaining > kPayloadMax ||
	    input[1] != remaining + 5 || input[7 + remaining] != 0xfe) {
		return false;
	}
	char decoded[kPayloadMax + 1]{};
	memcpy(decoded, prefix, prefix_length);
	memcpy(decoded + prefix_length, input + 7, remaining);
	if (!valid_url(decoded, prefix_length + remaining)) {
		return false;
	}
	memcpy(output, decoded, sizeof(decoded));
	return true;
}
} // namespace badge
