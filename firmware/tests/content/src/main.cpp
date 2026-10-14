/* SPDX-License-Identifier: MIT */
#include "badge/content.hpp"
#include <zephyr/ztest.h>
#include <string.h>
using namespace badge;

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
	Artwork art;
	zassert_true(valid_artwork(art.bytes, art.length));
	art.bytes[1] = 17;
	zassert_false(valid_artwork(art.bytes, art.length));
	art.bytes[1] = 16;
	art.length = kArtworkBytes;
	zassert_true(valid_artwork(art.bytes, art.length));
	zassert_false(valid_artwork(art.bytes, art.length - 1));
}
ZTEST(content, test_animation_boundary_and_static_frame)
{
	Artwork art;
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
ZTEST_SUITE(content, nullptr, nullptr, nullptr, nullptr, nullptr);
