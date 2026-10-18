/* SPDX-License-Identifier: MIT */
#include "badge/artwork_store.hpp"
#include <zephyr/ztest.h>
#include <errno.h>
#include <string.h>
using namespace badge;
namespace
{
uint8_t memory[2 * kArtworkBankBytes], original[kArtworkBytes], replacement[kArtworkBytes];
size_t budget = SIZE_MAX;
int erase(void *, size_t offset, size_t length)
{
	if (offset % 4096 || length != 4096 || offset + length > sizeof(memory)) {
		return -EINVAL;
	}
	const size_t available = budget < length ? budget : length;
	memset(memory + offset, 0xff, available);
	budget -= available;
	return available == length ? 0 : -EIO;
}
int write(void *, size_t offset, const void *source, size_t length)
{
	if (offset % 4 || length % 4 || offset + length > sizeof(memory)) {
		return -EINVAL;
	}
	const auto *bytes = static_cast<const uint8_t *>(source);
	for (size_t i = 0; i < length; i++) {
		if (budget == 0) {
			return -EIO;
		}
		if ((memory[offset + i] & bytes[i]) != bytes[i]) {
			return -EIO;
		}
		memory[offset + i] = bytes[i];
		budget--;
	}
	return 0;
}
void initialize(ArtworkStore &store)
{
	store.initialize({nullptr, memory, erase, write});
}
void make(uint8_t *blob, uint8_t seed)
{
	blob[0] = 2;
	blob[1] = 128;
	blob[2] = 100;
	blob[3] = blob[4] = blob[5] = 0;
	for (unsigned i = 0; i < 128; i++) {
		const size_t offset = 6 + i * 1029;
		blob[offset] = 1;
		blob[offset + 1] = 0;
		blob[offset + 2] = 4;
		blob[offset + 3] = 100;
		blob[offset + 4] = 0;
		for (unsigned p = 0; p < 1024; p++) {
			blob[offset + 5 + p] = seed + i + p;
		}
	}
}
int upload(ArtworkStore &store, const uint8_t *blob)
{
	for (size_t offset = 0; offset < kArtworkBytes;) {
		const size_t count = kArtworkBytes - offset < 235 ? kArtworkBytes - offset : 235;
		const int rc = store.stage(offset, kArtworkBytes, blob + offset, count);
		if (rc != 0) {
			return rc;
		}
		offset += count;
	}
	return 0;
}
} // namespace
ZTEST(artwork_store, test_maximum_upload_render_and_small_workspaces)
{
	static_assert(sizeof(Artwork) <= 16);
	static_assert(sizeof(ArtworkStore) < 384);
	static_assert(sizeof(ArtworkPlayer) < 1400);
	memset(memory, 0xff, sizeof(memory));
	budget = SIZE_MAX;
	make(original, 17);
	ArtworkStore store;
	initialize(store);
	zassert_ok(upload(store, original));
	uint8_t manifest[12];
	zassert_ok(store.prepare(store.staged_data(), kArtworkBytes, manifest));
	store.committed(manifest);
	Artwork art;
	zassert_ok(store.load(manifest, art));
	zassert_equal(art.length, 131718);
	zassert_mem_equal(art.bytes, original, kArtworkBytes);
	ArtworkPlayer player;
	Frame frame;
	player.render(frame, art, 12700, true);
	zassert_equal(frame.level(0, 0), uint8_t(17 + 127));
	zassert_equal(frame.level(31, 31), uint8_t(17 + 127 + 1023));
	player.render(frame, art, 12800, true);
	zassert_equal(frame.level(0, 0), 17);
	memory[0] ^= 1;
	zassert_equal(store.load(manifest, art), -EIO);
}
ZTEST(artwork_store, test_interrupted_inactive_upload_never_changes_committed_artwork)
{
	memset(memory, 0xff, sizeof(memory));
	budget = SIZE_MAX;
	make(original, 29);
	make(replacement, 83);
	ArtworkStore initial;
	initialize(initial);
	zassert_ok(upload(initial, original));
	uint8_t old_manifest[12];
	zassert_ok(initial.prepare(initial.staged_data(), kArtworkBytes, old_manifest));
	initial.committed(old_manifest);
	const size_t cuts[] = {0, 1, 4095, 4096, 65535, 131718, 270000};
	for (size_t cut : cuts) {
		budget = SIZE_MAX;
		ArtworkStore store;
		initialize(store);
		Artwork old;
		zassert_ok(store.load(old_manifest, old));
		budget = cut;
		const int rc = upload(store, replacement);
		budget = SIZE_MAX;
		ArtworkStore reboot;
		initialize(reboot);
		Artwork retained;
		zassert_ok(reboot.load(old_manifest, retained));
		zassert_mem_equal(retained.bytes, original, kArtworkBytes);
		if (rc == 0) {
			uint8_t committed[12];
			zassert_ok(store.prepare(store.staged_data(), kArtworkBytes, committed));
			store.committed(committed);
			zassert_ok(reboot.load(committed, retained));
			zassert_mem_equal(retained.bytes, replacement, kArtworkBytes);
		} else {
			zassert_equal(rc, -EIO);
		}
	}
}
ZTEST(artwork_store, test_restart_bounds_alignment_and_malformed_frames)
{
	memset(memory, 0xff, sizeof(memory));
	budget = SIZE_MAX;
	make(original, 19);
	ArtworkStore store;
	initialize(store);
	Artwork invalid{nullptr, 0};
	ArtworkPlayer player;
	Frame frame;
	frame.set(1, 1);
	player.render(frame, invalid, 0, true);
	zassert_false(frame.get(1, 1));
	zassert_equal(store.stage(1, kArtworkBytes, original, 1), -EINVAL);
	zassert_equal(store.stage(0, kArtworkBytes + 1, original, 1), -EINVAL);
	zassert_equal(store.stage(0, kArtworkBytes, original, 236), -EINVAL);
	zassert_ok(store.stage(0, kArtworkBytes, original, 11));
	zassert_is_null(store.staged_data());
	zassert_equal(store.stage(12, kArtworkBytes, original + 12, 11), -EINVAL);
	zassert_ok(upload(store, original));
	uint8_t bad[14] = {2, 1, 100, 0, 0, 0, 2, 3, 0, 100, 0, 0, 0, 64};
	zassert_equal(store.stage(0, sizeof(bad), bad, sizeof(bad)), -EINVAL);
	zassert_is_null(store.staged_data());
	zassert_ok(upload(store, original));
	zassert_not_null(store.staged_data());
}
ZTEST_SUITE(artwork_store, nullptr, nullptr, nullptr, nullptr, nullptr);
