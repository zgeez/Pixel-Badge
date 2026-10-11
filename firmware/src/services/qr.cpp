/* SPDX-License-Identifier: MIT */
#include "badge/qr.hpp"

#include <string.h>
#include <qrcodegen.h>

namespace badge
{
namespace
{
bool prefix_equal(const char *text, const char *prefix, size_t length)
{
	for (size_t i = 0; i < length; ++i) {
		char c = text[i];
		if (c >= 'a' && c <= 'z') {
			c -= 'a' - 'A';
		}
		if (c != prefix[i]) {
			return false;
		}
	}
	return true;
}

bool syntax_valid(const char *text, size_t length)
{
	if (text == nullptr || length == 0 || length > kPayloadMax) {
		return false;
	}
	const size_t start = length >= 8 && prefix_equal(text, "HTTPS://", 8)  ? 8
			     : length >= 7 && prefix_equal(text, "HTTP://", 7) ? 7
									       : 0;
	if (start == 0 || start == length || text[start] == '/' || text[start] == '?' ||
	    text[start] == '#' || text[start] == ':') {
		return false;
	}
	constexpr char allowed[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-"
				   "._~:/?#[]!$&'()*+,;=%";
	for (size_t i = 0; i < length; ++i) {
		if (text[i] == '\0' || strchr(allowed, text[i]) == nullptr) {
			return false;
		}
		if (text[i] == '%' &&
		    (i + 2 >= length || strchr("0123456789ABCDEFabcdef", text[i + 1]) == nullptr ||
		     strchr("0123456789ABCDEFabcdef", text[i + 2]) == nullptr)) {
			return false;
		}
	}
	return true;
}
} // namespace

bool encode_url(const char *text, size_t length, Frame &output)
{
	if (!syntax_valid(text, length)) {
		return false;
	}
	char terminated[kPayloadMax + 1]{};
	memcpy(terminated, text, length);
	uint8_t scratch[qrcodegen_BUFFER_LEN_FOR_VERSION(1)];
	uint8_t symbol[qrcodegen_BUFFER_LEN_FOR_VERSION(1)];
	if (!qrcodegen_encodeText(terminated, scratch, symbol, qrcodegen_Ecc_MEDIUM, 1, 1,
				  qrcodegen_Mask_AUTO, false)) {
		return false;
	}
	Frame next;
	memset(next.bytes, 0xff, sizeof(next.bytes));
	for (int y = 0; y < 21; ++y) {
		for (int x = 0; x < 21; ++x) {
			next.set(x + 5, y + 5, !qrcodegen_getModule(symbol, x, y));
		}
	}
	output = next;
	return true;
}

bool valid_url(const char *text, size_t length)
{
	Frame unused;
	return encode_url(text, length, unused);
}
} // namespace badge
