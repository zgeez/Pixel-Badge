/* SPDX-License-Identifier: MIT */
#include "../../common/artwork_fixture.hpp"
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
	zassert_equal(next_brightness(255), 13);
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

ZTEST(modes, test_artwork_selection_requires_content_and_preserves_mode)
{
	Modes modes;
	modes.start(kDefaultSettings, false);
	zassert_false(modes.select(Mode::kCanvas, 10));
	zassert_false(modes.select(Mode::kAnimation, 20));
	zassert_equal(modes.active(), Mode::kQr);

	OwnedSettings drawing;
	mutable_art(drawing)[kArtworkHeaderBytes] = 0x80;
	zassert_true(modes.configure(drawing));
	zassert_true(modes.select(Mode::kCanvas, 30));
	zassert_false(modes.select(Mode::kAnimation, 40));
	zassert_equal(modes.active(), Mode::kCanvas);

	mutable_art(drawing)[1] = 2;
	drawing.artwork.length += kFrameBytes;
	mutable_art(drawing)[kArtworkHeaderBytes + kFrameBytes] = 0x40;
	zassert_true(modes.configure(drawing));
	zassert_true(modes.select(Mode::kAnimation, 50));
	Frame before, after;
	modes.render(before, 200);
	zassert_false(modes.select(static_cast<Mode>(4), 200));
	modes.render(after, 200);
	zassert_equal(modes.active(), Mode::kAnimation);
	zassert_mem_equal(before.bytes, after.bytes, sizeof(before.bytes));

	memset(mutable_art(drawing) + kArtworkHeaderBytes, 0, 2 * kFrameBytes);
	zassert_true(modes.configure(drawing));
	zassert_false(modes.select(Mode::kCanvas, 300));
	zassert_false(modes.select(Mode::kAnimation, 300));
	zassert_equal(modes.active(), Mode::kQr);
	modes.next(300);
	zassert_equal(modes.active(), Mode::kName);
}

ZTEST(modes, test_boot_falls_back_when_saved_artwork_mode_is_unavailable)
{
	OwnedSettings settings;
	Modes modes;
	settings.default_mode = Mode::kCanvas;
	modes.start(settings, false);
	zassert_equal(modes.active(), Mode::kQr);
	mutable_art(settings)[kArtworkHeaderBytes] = 0x80;
	modes.start(settings, false);
	zassert_equal(modes.active(), Mode::kCanvas);
	settings.default_mode = Mode::kAnimation;
	modes.start(settings, false);
	zassert_equal(modes.active(), Mode::kQr);
	mutable_art(settings)[1] = 2;
	settings.artwork.length += kFrameBytes;
	modes.start(settings, false);
	zassert_equal(modes.active(), Mode::kAnimation);
}

ZTEST(modes, test_drawing_requires_pixels_in_the_rendered_first_frame)
{
	OwnedSettings design;
	design.default_mode = Mode::kCanvas;
	mutable_art(design)[1] = 2;
	design.artwork.length += kFrameBytes;
	mutable_art(design)[kArtworkHeaderBytes + kFrameBytes] = 0x40;
	Modes modes;
	modes.start(design, false);
	zassert_equal(modes.active(), Mode::kQr);
	zassert_false(modes.select(Mode::kCanvas, 10));
	modes.next(10);
	zassert_equal(modes.active(), Mode::kName);
	modes.next(20);
	zassert_equal(modes.active(), Mode::kAnimation);
	Frame before, after;
	modes.render(before, 170);
	zassert_mem_equal(before.bytes, design.artwork.bytes + kArtworkHeaderBytes + kFrameBytes,
			  kFrameBytes);
	zassert_false(modes.select(Mode::kCanvas, 170));
	modes.render(after, 170);
	zassert_mem_equal(before.bytes, after.bytes, kFrameBytes);
	design.default_mode = Mode::kAnimation;
	modes.start(design, false);
	zassert_equal(modes.active(), Mode::kAnimation);

	mutable_art(design)[kArtworkHeaderBytes] = 0x80;
	zassert_true(modes.configure(design));
	zassert_true(modes.select(Mode::kCanvas, 200));
	modes.render(after, 500);
	zassert_mem_equal(after.bytes, design.artwork.bytes + kArtworkHeaderBytes, kFrameBytes);
	design.artwork.length = kArtworkHeaderBytes;
	zassert_true(modes.configure(design));
	zassert_false(modes.select(Mode::kCanvas, 600));
}

ZTEST(modes, test_replacing_artwork_revalidates_active_mode)
{
	OwnedSettings settings;
	mutable_art(settings)[kArtworkHeaderBytes] = 0x80;
	Modes modes;
	modes.start(settings, false);
	zassert_true(modes.select(Mode::kCanvas, 0));
	mutable_art(settings)[kArtworkHeaderBytes] = 0;
	zassert_true(modes.configure(settings));
	zassert_equal(modes.active(), Mode::kQr);
	Frame frame, expected;
	modes.render(frame, 10);
	encode_url(settings.url, strlen(settings.url), expected);
	zassert_mem_equal(frame.bytes, expected.bytes, sizeof(frame.bytes));
}

ZTEST_SUITE(modes, nullptr, nullptr, nullptr, nullptr, nullptr);
