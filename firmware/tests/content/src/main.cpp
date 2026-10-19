/* SPDX-License-Identifier: MIT */
#include "../../common/artwork_fixture.hpp"
#include "badge/content.hpp"
#include "badge/font5x8.hpp"
#include "badge/hub75_timing.h"
#include <zephyr/ztest.h>
#include <string.h>
using namespace badge;

ZTEST(content, test_moving_text_uses_fast_cadence)
{
	TextContent text;
	for (unsigned layout = 0; layout <= 3; ++layout) {
		text.bytes[1] = layout;
		zassert_equal(text_animated(text), layout >= 2);
	}
}
ZTEST(content, test_reject_malformed_and_oversized_assets)
{
	TextContent text;
	zassert_true(valid_text(text.bytes, text.length));
	text.bytes[4] = 49;
	zassert_false(valid_text(text.bytes, text.length));
	text.bytes[4] = 150;
	text.bytes[6] = 0x80;
	zassert_false(valid_text(text.bytes, 7));
	text.bytes[6] = '\n';
	zassert_true(valid_text(text.bytes, 7));
	zassert_false(valid_text(text.bytes, sizeof(text.bytes) + 1));
	OwnedArtwork art;
	zassert_true(valid_artwork(art.bytes, art.length));
	art.bytes[1] = 17;
	zassert_false(valid_artwork(art.bytes, art.length));
	art.bytes[1] = 16;
	art.length = (4 + 16 * kFrameBytes);
	zassert_true(valid_artwork(art.bytes, art.length));
	zassert_false(valid_artwork(art.bytes, art.length - 1));
}
ZTEST(content, test_animation_boundary_and_static_frame)
{
	OwnedArtwork art;
	art.bytes[1] = 2;
	art.bytes[2] = 100;
	art.length = 4 + 2 * kFrameBytes;
	art.bytes[4] = 0x80;
	art.bytes[4 + kFrameBytes + 127] = 1;
	Frame frame;
	render_artwork(frame, art, 99, true);
	zassert_true(frame.get(0, 0));
	zassert_false(frame.get(31, 31));
	render_artwork(frame, art, 100, true);
	zassert_false(frame.get(0, 0));
	zassert_true(frame.get(31, 31));
	render_artwork(frame, art, 200, true);
	zassert_true(frame.get(0, 0));
	render_artwork(frame, art, 100, false);
	zassert_true(frame.get(0, 0));
	zassert_true(artwork_present(art));
}
ZTEST(content, test_case_sensitive_text_and_multiline_pagination)
{
	TextContent text;
	text.bytes[2] = 1;
	memcpy(text.bytes + 6, "Ab", 2);
	text.length = 8;
	Frame lower, upper, before, after;
	render_text(lower, text, "", 0);
	text.bytes[7] = 'B';
	render_text(upper, text, "", 0);
	zassert_not_equal(memcmp(lower.bytes, upper.bytes, kFrameBytes), 0);
	const char message[] = "First\nSecond\nThird\nFourth\nFifth";
	memcpy(text.bytes + 6, message, sizeof(message) - 1);
	text.length = 6 + sizeof(message) - 1;
	text.bytes[1] = 1;
	render_text(before, text, "", 0);
	render_text(after, text, "", 3000);
	zassert_not_equal(memcmp(before.bytes, after.bytes, kFrameBytes), 0);
	render_text(after, text, "", 6000);
	zassert_mem_equal(before.bytes, after.bytes, kFrameBytes);
}
ZTEST(content, test_app_preview_frame_fixtures)
{
	/* FNV-1a reference frames shared with app/studio_model_test.dart. */
	struct Fixture {
		const char *text;
		uint8_t layout, scale, interval;
		uint32_t time, hash;
	};
	const Fixture fixtures[] = {
		{"Hi!", 0, 0, 150, 0, 0x85cd5ee5},
		{"A\nb\nc\nD\nE", 1, 1, 100, 0, 0x19a00905},
		{"A\nb\nc\nD\nE", 1, 1, 100, 2000, 0x744791b8},
		{"Hello world!", 2, 2, 90, 1530, 0x2897dd65},
		{"", 0, 0, 150, 0, 0xd0fb099b},
	};
	for (const auto &fixture : fixtures) {
		TextContent text;
		text.bytes[1] = fixture.layout;
		text.bytes[2] = fixture.scale;
		text.bytes[4] = fixture.interval;
		text.length = 6 + strlen(fixture.text);
		memcpy(text.bytes + 6, fixture.text, strlen(fixture.text));
		Frame frame;
		render_text(frame, text, "BADGE", fixture.time);
		uint32_t hash = 2166136261U;
		for (uint8_t byte : frame.bytes) {
			hash = (hash ^ byte) * 16777619U;
		}
		zassert_equal(hash, fixture.hash, "preview mismatch for %s at %u", fixture.text,
			      fixture.time);
	}
}
ZTEST(content, test_grayscale_runs_delta_rotation_and_timing)
{
	OwnedArtwork art;
	const uint8_t encoded[] = {2,  2, 150, 0, 0,   0, 2, 3, 0,   100, 0, 0, 4,
				   64, 3, 6,   0, 250, 0, 1, 0, 192, 255, 3, 0};
	memcpy(art.bytes, encoded, sizeof(encoded));
	art.length = sizeof(encoded);
	zassert_true(valid_artwork(art.bytes, art.length));
	Frame first, second;
	render_artwork(first, art, 99, true);
	render_artwork(second, art, 100, true);
	zassert_equal(first.level(0, 0), 64);
	zassert_equal(first.level(31, 31), 64);
	zassert_equal(second.level(0, 0), 128);
	zassert_equal(second.level(31, 31), 64);
	render_artwork(second, art, 350, true);
	zassert_equal(second.level(0, 0), 64);
	zassert_true(artwork_present(art, true));
	for (size_t length = 0; length < sizeof(encoded); ++length) {
		zassert_false(valid_artwork(art.bytes, length));
	}
	art.bytes[11] = 0;
	art.bytes[12] = 0;
	zassert_false(valid_artwork(art.bytes, art.length));
}
ZTEST(content, test_all_levels_raw_frames_and_playback)
{
	OwnedArtwork art;
	art.bytes[0] = 2;
	art.bytes[1] = 3;
	art.bytes[4] = 0;
	art.bytes[5] = 0;
	size_t offset = 6;
	for (unsigned i = 0; i < 3; i++) {
		art.bytes[offset] = 1;
		art.bytes[offset + 1] = 0;
		art.bytes[offset + 2] = 4;
		const unsigned duration = (i + 1) * 100;
		art.bytes[offset + 3] = duration;
		art.bytes[offset + 4] = duration >> 8;
		for (unsigned n = 0; n < 1024; n++) {
			art.bytes[offset + 5 + n] = i == 0 ? n % 256 : 64 * (i + 1);
		}
		offset += 1029;
	}
	art.length = offset;
	zassert_true(valid_artwork(art.bytes, art.length));
	Frame frame;
	render_artwork(frame, art, 0, true);
	for (unsigned n = 0; n < 256; n++) {
		zassert_equal(frame.level(n % 32, n / 32), n);
	}
	zassert_equal(artwork_index(art, 300), 2);
	zassert_equal(artwork_index(art, 600), 0);
	art.bytes[4] = 1;
	zassert_equal(artwork_index(art, 9999), 2);
	art.bytes[4] = 2;
	zassert_equal(artwork_index(art, 0), 2);
	zassert_equal(artwork_index(art, 300), 1);
	art.bytes[4] = 3;
	zassert_equal(artwork_index(art, 600), 1);
	zassert_equal(artwork_index(art, 800), 0);
	art.bytes[4] = 4;
	zassert_false(valid_artwork(art.bytes, art.length));
}
ZTEST_SUITE(content, nullptr, nullptr, nullptr, nullptr, nullptr);

ZTEST(content, test_grayscale_timer_pulses_are_bounded_and_weighted)
{
	uint32_t total = 0;
	for (unsigned plane = 0; plane < 8; plane++) {
		zassert_equal(badge_gray_on_ticks(plane, 255), 4U << plane);
		total += badge_gray_on_ticks(plane, 255);
		for (unsigned brightness = 0; brightness < 256; brightness++) {
			const auto on = badge_gray_on_ticks(plane, brightness);
			zassert_true(on <= (4U << plane));
			zassert_true(badge_gray_slot_ticks(on) >= 16U + on + 32U);
			zassert_true(badge_gray_slot_ticks(on) >= 256U);
			if (brightness == 0) {
				zassert_equal(on, 0);
			}
		}
	}
	zassert_equal(total, 1020);
}

ZTEST(content, test_bounce_lines_synchronize_with_overshoot)
{
	TextContent text;
	text.bytes[1] = 3;
	text.bytes[2] = 1;
	text.bytes[4] = 100;
	const char message[] = "ABCDEFG\nHIJKLM\nZ";
	memcpy(text.bytes + 6, message, sizeof(message) - 1);
	text.length = 6 + sizeof(message) - 1;
	struct Sample {
		uint32_t time;
		int first, second;
	};
	const Sample samples[] = {{0, 3, 3},       {100, 2, 2},    {750, -5, -2}, {1500, -12, -6},
				  {1600, -11, -5}, {2250, -5, -2}, {3000, 3, 3}};
	Frame initial;
	for (const auto &sample : samples) {
		render_text(initial, text, "", sample.time);
		struct Expected {
			const char *value;
			int left, top;
		};
		const Expected rows[] = {
			{"ABCDEFG", sample.first, 4}, {"HIJKLM", sample.second, 12}, {"Z", 13, 20}};
		for (const auto &row : rows) {
			for (int x = 0; x < 32; ++x) {
				for (int y = 0; y < 8; ++y) {
					const int at = x - row.left, letter = at / 6,
						  column = at % 6;
					const bool expected =
						at >= 0 &&
						static_cast<size_t>(letter) < strlen(row.value) &&
						column < 5 &&
						(kFont5x8[(row.value[letter] - 32) * 5 + column] &
						 (1U << y)) != 0;
					zassert_equal(initial.get(x, row.top + y), expected,
						      "time %u line %s (%d,%d)", sample.time,
						      row.value, x, y);
				}
			}
		}
	}
	memset(text.bytes + 6, '\n', kTextMax);
	text.length = kTextBytes;
	render_text(initial, text, "", UINT32_MAX);
	for (uint8_t byte : initial.bytes) {
		zassert_equal(byte, 0);
	}
}
