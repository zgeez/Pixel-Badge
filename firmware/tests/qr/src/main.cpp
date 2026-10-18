/* SPDX-License-Identifier: MIT */
#include "badge/qr.hpp"
#include "badge/qr_test_codes.hpp"

#include <zephyr/ztest.h>
#include <qrcodegen.h>
#include <string.h>

using namespace badge;

static Frame reference_frame(const char *payload, qrcodegen_Ecc ec, int mask)
{
	uint8_t scratch[qrcodegen_BUFFER_LEN_FOR_VERSION(1)];
	uint8_t symbol[qrcodegen_BUFFER_LEN_FOR_VERSION(1)];
	zassert_true(qrcodegen_encodeText(payload, scratch, symbol, ec, 1, 1,
					  static_cast<qrcodegen_Mask>(mask), false));
	Frame frame;
	memset(frame.bytes, 0xff, sizeof(frame.bytes));
	for (int y = 0; y < 21; ++y) {
		for (int x = 0; x < 21; ++x) {
			frame.set(5 + x, 5 + y, !qrcodegen_getModule(symbol, x, y));
		}
	}
	return frame;
}

ZTEST(qr, test_segno_committed_bitmaps)
{
	zassert_equal(kQrTestCodeCount, 3);
	const qrcodegen_Ecc levels[] = {qrcodegen_Ecc_LOW, qrcodegen_Ecc_MEDIUM,
					qrcodegen_Ecc_QUARTILE};
	for (size_t i = 0; i < kQrTestCodeCount; ++i) {
		bool matched = false;
		/* Encoders may choose different automatic masks. Exact bit-for-bit
		 * equality at Segno's mask verifies every module for each fixture. */
		for (int mask = 0; mask < 8; ++mask) {
			const Frame frame =
				reference_frame(kQrTestCodes[i].payload, levels[i], mask);
			matched |= memcmp(frame.bytes, kQrTestCodes[i].frame,
					  sizeof(frame.bytes)) == 0;
		}
		zassert_true(matched, "fixture %s differs at every mask", kQrTestCodes[i].name);
	}
}

ZTEST(qr, test_v1_m_full_quiet_zone)
{
	Frame frame;
	zassert_true(encode_url("HTTPS://EXAMPLE.COM", 19, frame));
	bool matched = false;
	for (int mask = 0; mask < 8; ++mask) {
		const Frame reference =
			reference_frame("HTTPS://EXAMPLE.COM", qrcodegen_Ecc_MEDIUM, mask);
		matched |= memcmp(frame.bytes, reference.bytes, sizeof(frame.bytes)) == 0;
	}
	zassert_true(matched);
	for (int y = 0; y < 32; ++y) {
		for (int x = 0; x < 32; ++x) {
			if (x < 5 || y < 5 || x > 25 || y > 25) {
				zassert_true(frame.get(x, y), "quiet zone at %d,%d", x, y);
			}
		}
	}
}

ZTEST(qr, test_strongest_fitting_correction)
{
	const char *urls[] = {"HTTP://A", "HTTPS://A.B/L", "HTTPS://EXAMPLE.COM",
			      "HTTPS://EXAMPLE.COM/ABCDE"};
	const qrcodegen_Ecc levels[] = {qrcodegen_Ecc_HIGH, qrcodegen_Ecc_QUARTILE,
					qrcodegen_Ecc_MEDIUM, qrcodegen_Ecc_LOW};
	for (unsigned i = 0; i < 4; ++i) {
		Frame actual;
		zassert_true(encode_url(urls[i], strlen(urls[i]), actual));
		bool matched = false;
		for (int mask = 0; mask < 8; ++mask) {
			const Frame reference = reference_frame(urls[i], levels[i], mask);
			matched |= memcmp(actual.bytes, reference.bytes, sizeof(actual.bytes)) == 0;
		}
		zassert_true(matched, "incorrect QR correction for %s", urls[i]);
	}
}

ZTEST(qr, test_capacity_and_charset)
{
	Frame frame;
	zassert_true(encode_url("HTTPS://EXAMPLE.COM/", 20, frame));
	zassert_true(valid_url("https://a.b/abc", 14));
	zassert_true(valid_url("https://a.b/abcd", 15));
	zassert_true(valid_url("https://a.b/abcde", 17));
	zassert_false(valid_url("https://a.b/abcdef", 18));
	zassert_true(valid_url("HTTPS://EXAMPLE.COM/ABCDE", 25));
	zassert_false(valid_url(nullptr, 1));
	zassert_false(valid_url("", 0));
	zassert_false(valid_url("HTTPS://", 8));
	zassert_false(valid_url("HTTPS:///A", 10));
	zassert_false(valid_url("FTP://A.B", 9));
	zassert_false(valid_url("HTTP://A B", 10));
	zassert_false(valid_url("HTTP://A\n", 9));
	zassert_false(valid_url("HTTP://A/%", 10));
	zassert_false(valid_url("HTTP://A/%ZZ", 12));
	const char embedded[] = {'H', 'T', 'T', 'P', ':', '/', '/', 'A', '\0', 'B'};
	zassert_false(valid_url(embedded, sizeof(embedded)));
	const char unicode[] = "HTTP://A/\xc3\xa9";
	zassert_false(valid_url(unicode, sizeof(unicode) - 1));
	const Frame previous = frame;
	zassert_false(encode_url("HTTPS://EXAMPLE.COM/ABCDEF", 26, frame));
	zassert_mem_equal(frame.bytes, previous.bytes, sizeof(frame.bytes));
}

ZTEST_SUITE(qr, nullptr, nullptr, nullptr, nullptr, nullptr);
