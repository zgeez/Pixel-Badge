/* SPDX-License-Identifier: MIT */
#include "badge/modes.hpp"
#include <zephyr/ztest.h>
#include <string.h>

using namespace badge;

ZTEST(modes, test_default_and_safe_mode)
{
	Modes modes;
	modes.start(kDefaultSettings, false);
	zassert_equal(modes.active(), Mode::kQr);
	modes.next(100);
	zassert_equal(modes.active(), Mode::kName);
	modes.next(200);
	zassert_equal(modes.active(), Mode::kQr);
	Settings custom = kDefaultSettings;
	custom.default_mode = Mode::kName;
	strcpy(custom.url, "HTTP://A.B");
	modes.start(custom, true);
	modes.next(100);
	zassert_equal(modes.active(), Mode::kQr);
	Frame frame, expected;
	modes.render(frame, 0);
	encode_url(kDefaultSettings.url, strlen(kDefaultSettings.url), expected);
	zassert_mem_equal(frame.bytes, expected.bytes, sizeof(frame.bytes));
}

ZTEST(modes, test_qr_rotation_locks_until_reentry)
{
	Modes modes;
	modes.start(kDefaultSettings, false);
	Frame before, after;
	modes.render(before, 0);
	modes.orientation(Rotation::k90);
	modes.render(after, 100);
	zassert_mem_equal(before.bytes, after.bytes, sizeof(before.bytes));
	modes.next(200);
	modes.next(300);
	modes.render(after, 300);
	Frame expected;
	rotate(before, expected, Rotation::k90);
	zassert_mem_equal(after.bytes, expected.bytes, sizeof(after.bytes));
}

ZTEST(modes, test_brightness_cycle)
{
	zassert_equal(next_brightness(16), 64);
	zassert_equal(next_brightness(64), 128);
	zassert_equal(next_brightness(128), 255);
	zassert_equal(next_brightness(255), 16);
	zassert_equal(next_brightness(17), 64);
}

ZTEST(modes, test_configure_preserves_rotation_and_rejects_invalid_url)
{
	Modes modes;
	modes.start(kDefaultSettings, false);
	modes.orientation(Rotation::k90);
	modes.next(10);
	modes.next(20);
	Settings changed = kDefaultSettings;
	strcpy(changed.url, "HTTPS://A.B/X");
	zassert_true(modes.configure(changed));
	Frame base, expected, actual;
	zassert_true(encode_url(changed.url, strlen(changed.url), base));
	rotate(base, expected, Rotation::k90);
	modes.render(actual, 30);
	zassert_mem_equal(expected.bytes, actual.bytes, sizeof(actual.bytes));
	strcpy(changed.url, "invalid");
	zassert_false(modes.configure(changed));
	modes.render(actual, 40);
	zassert_mem_equal(expected.bytes, actual.bytes, sizeof(actual.bytes));
	modes.start(kDefaultSettings, true);
	zassert_false(modes.configure(changed));
}

ZTEST_SUITE(modes, nullptr, nullptr, nullptr, nullptr, nullptr);
