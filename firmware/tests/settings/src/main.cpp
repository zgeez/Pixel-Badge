/* SPDX-License-Identifier: MIT */
#include "badge/settings.hpp"

#include <zephyr/ztest.h>
#include <errno.h>
#include <string.h>

using namespace badge;

static int fail_write(void *, Field, const void *, size_t)
{
	return -EIO;
}
static int accept_write(void *, Field, const void *, size_t)
{
	return 0;
}

ZTEST(settings, test_missing_schema_and_invalid_fields)
{
	SettingsService service;
	service.begin_load();
	const uint8_t brightness = 255;
	zassert_true(service.load(Field::kBrightness, &brightness, 1));
	zassert_equal(service.finish_load(), 255);
	zassert_equal(service.values().brightness, 128);
	const uint8_t version = kSchemaVersion;
	service.begin_load();
	zassert_true(service.load(Field::kSchema, &version, 1));
	const uint8_t invalid = 0;
	zassert_false(service.load(Field::kBrightness, &invalid, 1));
	const uint8_t wrong_mode = 7;
	zassert_false(service.load(Field::kDefaultMode, &wrong_mode, 1));
	zassert_false(service.load(Field::kName, "bad\n", 4));
	zassert_false(service.load(Field::kName, "", 0));
	zassert_false(service.load(Field::kUrl, "https://example.com", 19));
	zassert_equal(service.finish_load(), 254);
	zassert_equal(strcmp(service.values().name, "BADGE"), 0);
}

ZTEST(settings, test_valid_load_and_unknown_schema)
{
	SettingsService service;
	service.begin_load();
	const uint8_t version = kSchemaVersion;
	const uint8_t brightness = 16;
	const uint8_t mode = 1;
	zassert_true(service.load(Field::kName, "Abc", 3));
	zassert_true(service.load(Field::kSchema, &version, 1));
	zassert_true(service.load(Field::kUrl, "https://a.b/X", 13));
	zassert_true(service.load(Field::kBrightness, &brightness, 1));
	zassert_true(service.load(Field::kDefaultMode, &mode, 1));
	zassert_equal(service.finish_load(), 224);
	zassert_equal(strcmp(service.values().url, "https://a.b/X"), 0);
	zassert_equal(service.values().default_mode, Mode::kName);
	service.begin_load();
	const uint8_t future = 2;
	zassert_false(service.load(Field::kSchema, &future, 1));
	zassert_true(service.load(Field::kName, "NEW", 3));
	zassert_equal(service.finish_load(), 255);
	zassert_equal(strcmp(service.values().name, "BADGE"), 0);
}

ZTEST(settings, test_write_error_and_invalid_edit_keep_previous)
{
	SettingsService service;
	zassert_equal(service.save(Field::kName, "OLD", 3, accept_write, nullptr), 0);
	zassert_equal(service.save(Field::kName, "NEW", 3, fail_write, nullptr), -EIO);
	zassert_equal(strcmp(service.values().name, "OLD"), 0);
	zassert_equal(service.save(Field::kUrl, "https://example.com", 19, accept_write, nullptr),
		      -EINVAL);
	zassert_equal(strcmp(service.values().url, kDefaultSettings.url), 0);
	char long_name[kNameMax + 1];
	memset(long_name, 'A', sizeof(long_name));
	zassert_equal(
		service.save(Field::kName, long_name, sizeof(long_name), accept_write, nullptr),
		-EINVAL);
	zassert_equal(kDefaultSettings.brightness, 128);
}

struct Records {
	uint8_t bytes[static_cast<unsigned>(Field::kCount)][kArtworkBytes]{};
	size_t lengths[static_cast<unsigned>(Field::kCount)]{};
	int fail_at = -1;
	int calls = 0;
};

static int save_record(void *context, Field field, const void *data, size_t length)
{
	auto &records = *static_cast<Records *>(context);
	if (records.calls++ == records.fail_at) {
		return -EIO;
	}
	const unsigned index = static_cast<unsigned>(field);
	memcpy(records.bytes[index], data, length);
	records.lengths[index] = length;
	return 0;
}

static void reload(SettingsService &service, const Records &records)
{
	service.begin_load();
	for (unsigned i = 0; i < static_cast<unsigned>(Field::kCount); ++i) {
		if (records.lengths[i] != 0) {
			service.load(static_cast<Field>(i), records.bytes[i], records.lengths[i]);
		}
	}
	service.finish_load();
}

ZTEST(settings, test_schema_recovery_never_revives_old_fields)
{
	/* Missing, unsupported and malformed schemas; fail each initialization
	 * write, the schema commit and the requested write, then simulate reboot. */
	for (unsigned kind = 0; kind < 3; ++kind) {
		for (int cut = 0; cut <= 9; ++cut) {
			Records records;
			const uint8_t schema[] = {2, 1};
			if (kind != 0) {
				save_record(&records, Field::kSchema, schema, kind);
			}
			save_record(&records, Field::kUrl, "https://a.b/X", 13);
			save_record(&records, Field::kName, "OLD BADGE", 9);
			const uint8_t old_brightness = 16, old_mode = 1;
			save_record(&records, Field::kBrightness, &old_brightness, 1);
			save_record(&records, Field::kDefaultMode, &old_mode, 1);
			SettingsService service;
			reload(service, records);
			records.calls = 0;
			records.fail_at = cut;
			const uint8_t brightness = 64;
			const int rc = service.save(Field::kBrightness, &brightness, 1, save_record,
						    &records);
			zassert_equal(rc, cut < 9 ? -EIO : 0);
			zassert_equal(service.values().brightness, cut < 9 ? 128 : 64);
			SettingsService rebooted;
			reload(rebooted, records);
			zassert_equal(strcmp(rebooted.values().url, kDefaultSettings.url), 0);
			zassert_equal(strcmp(rebooted.values().name, kDefaultSettings.name), 0);
			zassert_equal(rebooted.values().default_mode, Mode::kQr);
			zassert_equal(rebooted.values().brightness, cut < 9 ? 128 : 64);
			/* A same-boot retry must safely finish a partial initialization. */
			records.fail_at = -1;
			zassert_ok(service.save(Field::kBrightness, &brightness, 1, save_record,
						&records));
			reload(rebooted, records);
			zassert_equal(rebooted.values().brightness, 64);
			zassert_equal(strcmp(rebooted.values().name, kDefaultSettings.name), 0);
		}
	}
}

ZTEST(settings, test_motion_is_atomic_and_survives_reload)
{
	Records records;
	SettingsService service;
	MotionConfig config;
	config.bytes[2] = 5;
	config.bytes[3] = 1;
	config.bytes[5] = 192;
	zassert_ok(service.save(Field::kMotion, config.bytes, kMotionBytes, save_record, &records));
	SettingsService rebooted;
	reload(rebooted, records);
	zassert_mem_equal(rebooted.values().motion.bytes, config.bytes, kMotionBytes);
	config.bytes[2] = 0; // Opposite signs of the same physical axis are invalid.
	zassert_equal(
		service.save(Field::kMotion, config.bytes, kMotionBytes, save_record, &records),
		-EINVAL);
	zassert_equal(service.values().motion.bytes[2], 5);
	config.bytes[2] = 4;
	zassert_equal(service.save(Field::kMotion, config.bytes, kMotionBytes, fail_write, nullptr),
		      -EIO);
	zassert_equal(service.values().motion.bytes[2], 5);
}
ZTEST_SUITE(settings, nullptr, nullptr, nullptr, nullptr, nullptr);
