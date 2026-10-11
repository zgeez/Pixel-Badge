/* SPDX-License-Identifier: MIT */
#include "badge/settings_store.hpp"

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
	static uint8_t baseline[0x4000];
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

ZTEST_SUITE(settings_store, nullptr, nullptr, nullptr, nullptr, nullptr);
