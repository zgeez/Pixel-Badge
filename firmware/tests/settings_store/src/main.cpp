/* SPDX-License-Identifier: MIT */
#include "../../common/artwork_fixture.hpp"
#include "badge/settings_store.hpp"
#include "badge/artwork_store.hpp"

#include <zephyr/ztest.h>
#include <zephyr/drivers/flash/flash_simulator.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/kvss/nvs.h>
#include <zephyr/settings/settings.h>
#include <errno.h>
#include <string.h>

using namespace badge;

static unsigned budget;
static int cut_write(const device *, off_t, uint8_t data)
{
	if (budget == 0) {
		return -EIO;
	}
	--budget;
	return data;
}

ZTEST(settings_store, test_factory_reset_reload_invalid_value_and_power_cuts)
{
	const flash_area *area;
	const int opened = flash_area_open(PARTITION_ID(storage_partition), &area);
	zassert_ok(opened, "open returned %d, partition ID %d", opened,
		   PARTITION_ID(storage_partition));
	const device *flash = area->fa_dev;
	/* Dirty flash before the very first mount proves the boot reset erases
	 * the whole area, including bytes outside the application records. */
	const uint8_t dirty = 0;
	zassert_ok(flash_area_write(area, area->fa_size - 1, &dirty, 1));
	SettingsService service;
	zassert_ok(load_settings(service, true));
	uint8_t erased = 0;
	zassert_ok(flash_area_read(area, area->fa_size - 1, &erased, 1));
	zassert_equal(erased, 0xff);
	zassert_equal(strcmp(service.values().name, "BADGE"), 0);
	zassert_ok(service.save(Field::kName, "OLD BADGE", 9, store_field, nullptr));
	zassert_ok(service.save(Field::kUrl, "https://a.b/X", 13, store_field, nullptr));
	const uint8_t name_mode = static_cast<uint8_t>(Mode::kName);
	zassert_ok(service.save(Field::kDefaultMode, &name_mode, 1, store_field, nullptr));
	const uint8_t brightness = 64;
	zassert_ok(service.save(Field::kBrightness, &brightness, 1, store_field, nullptr));
	SettingsService reloaded;
	zassert_ok(load_settings(reloaded, false));
	zassert_equal(strcmp(reloaded.values().name, "OLD BADGE"), 0);
	zassert_equal(reloaded.values().brightness, 64);
	zassert_equal(strcmp(reloaded.values().url, "https://a.b/X"), 0);
	zassert_equal(reloaded.values().default_mode, Mode::kName);
	const uint8_t invalid = 0;
	zassert_ok(settings_save_one("badge/brightness", &invalid, 1));
	zassert_ok(load_settings(reloaded, false));
	zassert_equal(reloaded.values().brightness, kDefaultSettings.brightness);
	zassert_equal(strcmp(reloaded.values().name, "OLD BADGE"), 0);

	void *storage;
	zassert_ok(settings_storage_get(&storage));
	auto *fs = static_cast<nvs_fs *>(storage);
	size_t size;
	auto *memory = static_cast<uint8_t *>(flash_simulator_get_memory(flash, &size));
	zassert_true(area->fa_off + area->fa_size <= size);
	static uint8_t baseline[0x8000];
	zassert_equal(area->fa_size, sizeof(baseline));
	memcpy(baseline, memory + area->fa_off, sizeof(baseline));
	const flash_simulator_cb callbacks = {cut_write, nullptr};
	bool saw_old = false, saw_new = false;
	/* Every byte boundary through the new value, data CRC and commit
	 * metadata, plus completed writes. Remount discards volatile NVS state. */
	for (unsigned cut = 0; cut <= 40; ++cut) {
		memcpy(memory + area->fa_off, baseline, sizeof(baseline));
		zassert_ok(nvs_mount(fs));
		zassert_ok(load_settings(reloaded, false));
		budget = cut;
		flash_simulator_set_callbacks(flash, &callbacks);
		const int rc = reloaded.save(Field::kName, "NEW BADGE", 9, store_field, nullptr);
		flash_simulator_set_callbacks(flash, nullptr);
		zassert_true(rc == 0 || rc == -EIO);
		zassert_ok(nvs_mount(fs));
		zassert_ok(load_settings(reloaded, false));
		const bool old = strcmp(reloaded.values().name, "OLD BADGE") == 0;
		const bool updated = strcmp(reloaded.values().name, "NEW BADGE") == 0;
		zassert_true(old || updated, "corrupt/default value after cut %u", cut);
		saw_old |= old;
		saw_new |= updated;
		zassert_equal(strcmp(reloaded.values().url, "https://a.b/X"), 0);
	}
	zassert_true(saw_old && saw_new);
	/* A full animation exceeds a single ATT attribute and spans significant
	 * NVS space. Interrupted flash writes must retain one complete version. */
	OwnedArtwork old_art, new_art;
	old_art.length = new_art.length = (4 + 16 * kFrameBytes);
	old_art.bytes[1] = new_art.bytes[1] = 16;
	memset(old_art.bytes + 4, 0x55, (4 + 16 * kFrameBytes) - 4);
	memset(new_art.bytes + 4, 0xaa, (4 + 16 * kFrameBytes) - 4);
	zassert_ok(reloaded.save(Field::kArtwork, old_art.bytes, old_art.length, store_field,
				 nullptr));
	memcpy(baseline, memory + area->fa_off, sizeof(baseline));
	saw_old = saw_new = false;
	const unsigned cuts[] = {0, 1, 15, 128, 512, 1024, 2048, 2052, 2060, 2100, 4096};
	for (unsigned cut : cuts) {
		memcpy(memory + area->fa_off, baseline, sizeof(baseline));
		zassert_ok(nvs_mount(fs));
		zassert_ok(load_settings(reloaded, false));
		budget = cut;
		flash_simulator_set_callbacks(flash, &callbacks);
		const int rc = reloaded.save(Field::kArtwork, new_art.bytes, new_art.length,
					     store_field, nullptr);
		flash_simulator_set_callbacks(flash, nullptr);
		zassert_true(rc == 0 || rc == -EIO);
		zassert_ok(nvs_mount(fs));
		zassert_ok(load_settings(reloaded, false));
		const auto &art = reloaded.values().artwork;
		zassert_equal(art.length, (4 + 16 * kFrameBytes));
		const bool old = memcmp(art.bytes, old_art.bytes, (4 + 16 * kFrameBytes)) == 0;
		const bool updated = memcmp(art.bytes, new_art.bytes, (4 + 16 * kFrameBytes)) == 0;
		zassert_true(old || updated, "partial animation after cut %u", cut);
		saw_old |= old;
		saw_new |= updated;
	}
	zassert_true(saw_old && saw_new);
	/* Unknown schema with otherwise valid old records must never reactivate
	 * those records, even if recovery is interrupted at any flash byte. */
	const uint8_t future_schema = 2;
	zassert_ok(settings_save_one("badge/schema", &future_schema, 1));
	zassert_ok(load_settings(reloaded, false));
	zassert_equal(strcmp(reloaded.values().name, kDefaultSettings.name), 0);
	memcpy(baseline, memory + area->fa_off, sizeof(baseline));
	budget = UINT32_MAX;
	flash_simulator_set_callbacks(flash, &callbacks);
	zassert_ok(reloaded.save(Field::kBrightness, &brightness, 1, store_field, nullptr));
	flash_simulator_set_callbacks(flash, nullptr);
	const unsigned recovery_bytes = UINT32_MAX - budget;
	zassert_true(recovery_bytes > 0 && recovery_bytes < 1024);
	bool saw_defaults = false, saw_saved = false;
	for (unsigned cut = 0; cut <= recovery_bytes; ++cut) {
		memcpy(memory + area->fa_off, baseline, sizeof(baseline));
		zassert_ok(nvs_mount(fs));
		zassert_ok(load_settings(reloaded, false));
		budget = cut;
		flash_simulator_set_callbacks(flash, &callbacks);
		const int rc =
			reloaded.save(Field::kBrightness, &brightness, 1, store_field, nullptr);
		flash_simulator_set_callbacks(flash, nullptr);
		zassert_true(rc == 0 || rc == -EIO);
		zassert_ok(nvs_mount(fs));
		zassert_ok(load_settings(reloaded, false));
		zassert_equal(strcmp(reloaded.values().name, kDefaultSettings.name), 0,
			      "old name revived after recovery cut %u", cut);
		zassert_equal(strcmp(reloaded.values().url, kDefaultSettings.url), 0,
			      "old URL revived after recovery cut %u", cut);
		zassert_equal(reloaded.values().default_mode, Mode::kQr);
		const bool defaults = reloaded.values().brightness == kDefaultSettings.brightness;
		const bool saved = reloaded.values().brightness == brightness;
		zassert_true(defaults || saved);
		saw_defaults |= defaults;
		saw_saved |= saved;
	}
	zassert_true(saw_defaults && saw_saved);
	TC_PRINT("Schema recovery checked %u interrupted flash byte positions\n",
		 recovery_bytes + 1);
	/* Corrupt committed value bytes after NVS enumeration can still find
	 * the record. The full read detects its CRC and must fail the load even
	 * though the pinned direct-loader API returns success. */
	const char crc_name[] = "CRC TEST NAME";
	zassert_ok(
		reloaded.save(Field::kName, crc_name, sizeof(crc_name) - 1, store_field, nullptr));
	unsigned matches = 0;
	for (size_t offset = 0; offset + sizeof(crc_name) - 1 <= area->fa_size; ++offset) {
		if (memcmp(memory + area->fa_off + offset, crc_name, sizeof(crc_name) - 1) == 0) {
			memory[area->fa_off + offset] ^= 1;
			++matches;
		}
	}
	zassert_equal(matches, 1);
	zassert_equal(load_settings(reloaded, false), -EIO);
	zassert_equal(strcmp(reloaded.values().name, kDefaultSettings.name), 0);
	zassert_equal(strcmp(reloaded.values().url, kDefaultSettings.url), 0);
	zassert_equal(reloaded.values().brightness, kDefaultSettings.brightness);
	zassert_equal(reloaded.values().default_mode, kDefaultSettings.default_mode);
	flash_area_close(area);
}

ZTEST(settings_store, test_large_banked_artwork_survives_interrupted_commits)
{
	const flash_area *area;
	zassert_ok(flash_area_open(PARTITION_ID(storage_partition), &area));
	const device *flash = area->fa_dev;
	SettingsService service;
	zassert_ok(load_settings(service, true));
	const uint8_t schema = kSchemaVersion;
	zassert_ok(store_field(nullptr, Field::kSchema, &schema, 1));
	zassert_true(service.load(Field::kSchema, &schema, 1));
	static OwnedArtwork old_art, new_art;
	auto make = [](OwnedArtwork &art, uint8_t seed) {
		art.bytes[0] = 2;
		art.bytes[1] = 8;
		art.bytes[2] = 150;
		art.bytes[3] = 0;
		art.bytes[4] = 0;
		art.bytes[5] = 0;
		size_t offset = 6;
		for (unsigned i = 0; i < 7; i++) {
			art.bytes[offset] = 1;
			art.bytes[offset + 1] = 0;
			art.bytes[offset + 2] = 4;
			art.bytes[offset + 3] = 150;
			art.bytes[offset + 4] = 0;
			for (unsigned n = 0; n < 1024; n++) {
				art.bytes[offset + 5 + n] = seed + n;
			}
			offset += 1029;
		}
		art.bytes[offset] = 2;
		art.bytes[offset + 1] = 978 & 255;
		art.bytes[offset + 2] = 978 >> 8;
		art.bytes[offset + 3] = 150;
		art.bytes[offset + 4] = 0;
		offset += 5;
		for (unsigned n = 0; n < 326; n++) {
			const unsigned run = n == 325 ? 49 : 3;
			art.bytes[offset++] = run;
			art.bytes[offset++] = 0;
			art.bytes[offset++] = seed + n;
		}
		art.length = offset;
	};
	make(old_art, 17);
	make(new_art, 93);
	zassert_equal(old_art.length, kLegacyArtworkBytes);
	zassert_true(valid_artwork(old_art.bytes, old_art.length));
	zassert_ok(
		service.save(Field::kArtwork, old_art.bytes, old_art.length, store_field, nullptr));
	void *storage;
	zassert_ok(settings_storage_get(&storage));
	auto *fs = static_cast<nvs_fs *>(storage);
	size_t size;
	auto *memory = static_cast<uint8_t *>(flash_simulator_get_memory(flash, &size));
	static uint8_t baseline[0x8000];
	memcpy(baseline, memory + area->fa_off, sizeof(baseline));
	const flash_simulator_cb callbacks = {cut_write, nullptr};
	const unsigned cuts[] = {0,    1,    100,  1024, 1032, 2048, 4096,
				 6144, 8192, 8220, 8300, 9000, 16000};
	bool saw_old = false, saw_new = false;
	for (unsigned cut : cuts) {
		memcpy(memory + area->fa_off, baseline, sizeof(baseline));
		zassert_ok(nvs_mount(fs));
		zassert_ok(load_settings(service, false));
		budget = cut;
		flash_simulator_set_callbacks(flash, &callbacks);
		const int rc = service.save(Field::kArtwork, new_art.bytes, new_art.length,
					    store_field, nullptr);
		flash_simulator_set_callbacks(flash, nullptr);
		zassert_true(rc == 0 || rc == -EIO || rc == -ENOSPC);
		zassert_ok(nvs_mount(fs));
		zassert_ok(load_settings(service, false));
		const auto &actual = service.values().artwork;
		const bool old = actual.length == old_art.length &&
				 memcmp(actual.bytes, old_art.bytes, actual.length) == 0;
		const bool fresh = actual.length == new_art.length &&
				   memcmp(actual.bytes, new_art.bytes, actual.length) == 0;
		zassert_true(old || fresh, "partial bank after cut %u", cut);
		saw_old |= old;
		saw_new |= fresh;
	}
	zassert_true(saw_old && saw_new);
	// Repeated complete replacements exercise both banks and NVS garbage collection.
	for (unsigned i = 0; i < 16; i++) {
		const auto &next = i % 2 ? new_art : old_art;
		zassert_ok(service.save(Field::kArtwork, next.bytes, next.length, store_field,
					nullptr));
		zassert_ok(load_settings(service, false));
		zassert_mem_equal(service.values().artwork.bytes, next.bytes, next.length);
	}
	flash_area_close(area);
}
ZTEST_SUITE(settings_store, nullptr, nullptr, nullptr, nullptr, nullptr);

ZTEST(settings_store, test_maximum_flash_artwork_manifest_power_cuts)
{
	static uint8_t old_blob[kArtworkBytes], new_blob[kArtworkBytes], baseline[0x8000];
	auto make = [](uint8_t *blob, unsigned seed) {
		blob[0] = 2;
		blob[1] = 128;
		blob[2] = 100;
		for (unsigned i = 0; i < 128; i++) {
			const size_t at = 6 + i * 1029;
			blob[at] = 1;
			blob[at + 2] = 4;
			blob[at + 3] = 100;
			for (unsigned p = 0; p < 1024; p++) {
				blob[at + 5 + p] = seed + i + p;
			}
		}
	};
	make(old_blob, 17);
	make(new_blob, 81);
	SettingsService service;
	zassert_ok(load_settings(service, true));
	const uint8_t schema = kSchemaVersion;
	zassert_ok(store_field(nullptr, Field::kSchema, &schema, 1));
	zassert_true(service.load(Field::kSchema, &schema, 1));
	zassert_ok(service.save(Field::kArtwork, old_blob, sizeof(old_blob), store_field, nullptr));
	const flash_area *area;
	zassert_ok(flash_area_open(PARTITION_ID(storage_partition), &area));
	size_t size;
	auto *memory = static_cast<uint8_t *>(flash_simulator_get_memory(area->fa_dev, &size));
	memcpy(baseline, memory + area->fa_off, sizeof(baseline));
	void *storage;
	zassert_ok(settings_storage_get(&storage));
	auto *fs = static_cast<nvs_fs *>(storage);
	const flash_simulator_cb callbacks = {cut_write, nullptr};
	const size_t cuts[] = {0, 1, 65535, 131719, 131720, 131724, 131736, 131744, 200000};
	bool saw_old = false, saw_new = false;
	for (size_t cut : cuts) {
		memcpy(memory + area->fa_off, baseline, sizeof(baseline));
		zassert_ok(nvs_mount(fs));
		zassert_ok(load_settings(service, false));
		budget = cut;
		flash_simulator_set_callbacks(area->fa_dev, &callbacks);
		const int rc = service.save(Field::kArtwork, new_blob, sizeof(new_blob),
					    store_field, nullptr);
		flash_simulator_set_callbacks(area->fa_dev, nullptr);
		zassert_true(rc == 0 || rc == -EIO);
		zassert_ok(nvs_mount(fs));
		zassert_ok(load_settings(service, false));
		const auto &art = service.values().artwork;
		zassert_equal(art.length, kArtworkBytes);
		const bool old = memcmp(art.bytes, old_blob, art.length) == 0,
			   fresh = memcmp(art.bytes, new_blob, art.length) == 0;
		zassert_true(old || fresh, "partial maximum artwork after cut %zu", cut);
		saw_old |= old;
		saw_new |= fresh;
	}
	zassert_true(saw_old && saw_new);
	flash_area_close(area);
}
ZTEST(settings_store, test_legacy_chunk_artwork_remains_loadable_then_migrates)
{
	SettingsService service;
	zassert_ok(load_settings(service, true));
	const uint8_t schema = kSchemaVersion;
	zassert_ok(settings_save_one("badge/schema", &schema, 1));
	OwnedArtwork art;
	art.bytes[4] = 0x80;
	zassert_ok(settings_save_one("badge/art0/0", art.bytes, art.length));
	uint8_t manifest[12] = {1, 0, 1, 0};
	auto put = [](uint8_t *out, uint32_t value) {
		for (unsigned i = 0; i < 4; i++) {
			out[i] = value >> (8 * i);
		}
	};
	put(manifest + 4, art.length);
	put(manifest + 8, artwork_crc(art.bytes, art.length));
	zassert_ok(settings_save_one("badge/art-meta", manifest, sizeof(manifest)));
	zassert_ok(load_settings(service, false));
	zassert_mem_equal(service.values().artwork.bytes, art.bytes, art.length);
	art.bytes[4] = 0x40;
	zassert_ok(service.save(Field::kArtwork, art.bytes, art.length, store_field, nullptr));
	zassert_ok(load_settings(service, false));
	zassert_mem_equal(service.values().artwork.bytes, art.bytes, art.length);
	zassert_not_equal(service.values().artwork.bytes, art.bytes);
	// An unknown manifest must fail closed rather than revive legacy artwork.
	manifest[0] = 0xff;
	zassert_ok(settings_save_one("badge/art-meta", manifest, sizeof(manifest)));
	zassert_equal(load_settings(service, false), -EIO);
	zassert_equal(service.values().artwork.length, 132);
}

namespace
{
bool lose_manifest_ack;
}
extern "C" int __real_settings_save_one(const char *, const void *, size_t);
extern "C" int __wrap_settings_save_one(const char *key, const void *value, size_t length)
{
	const int rc = __real_settings_save_one(key, value, length);
	if (lose_manifest_ack && rc == 0 && strcmp(key, "badge/art-meta") == 0) {
		lose_manifest_ack = false;
		return -EIO;
	}
	return rc;
}
ZTEST(settings_store, test_ambiguous_manifest_error_pins_both_banks_until_reload)
{
	SettingsService service;
	zassert_ok(load_settings(service, true));
	const uint8_t schema = 1;
	zassert_ok(store_field(nullptr, Field::kSchema, &schema, 1));
	zassert_true(service.load(Field::kSchema, &schema, 1));
	OwnedArtwork old_art, new_art;
	old_art.bytes[4] = 0x80;
	new_art.bytes[4] = 0x40;
	zassert_ok(
		service.save(Field::kArtwork, old_art.bytes, old_art.length, store_field, nullptr));
	zassert_ok(load_settings(service, false));
	const Artwork prior = service.values().artwork;
	lose_manifest_ack = true;
	zassert_equal(
		service.save(Field::kArtwork, new_art.bytes, new_art.length, store_field, nullptr),
		-EIO);
	const uint8_t *view = nullptr;
	zassert_equal(stage_artwork_flash(0, 132, old_art.bytes, 11, &view), -EIO);
	zassert_mem_equal(prior.bytes, old_art.bytes, old_art.length);
	zassert_ok(load_settings(service, false));
	zassert_mem_equal(service.values().artwork.bytes, new_art.bytes, new_art.length);
	zassert_ok(
		service.save(Field::kArtwork, old_art.bytes, old_art.length, store_field, nullptr));
}
