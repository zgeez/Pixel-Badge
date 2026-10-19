/* SPDX-License-Identifier: MIT */
#include "badge/settings_store.hpp"

#include <zephyr/settings/settings.h>
#include <zephyr/ztest.h>
#include <errno.h>
#include <string.h>

using namespace badge;

namespace
{
bool inject_reads;
ssize_t read_result;
size_t record_length;
unsigned reads;
uint8_t record = 64;
const char *record_key = "brightness";

ssize_t exact_read(void *data, void *destination, size_t length)
{
	memcpy(destination, data, length);
	return length;
}

ssize_t injected_read(void *, void *destination, size_t length)
{
	++reads;
	memset(destination, record, length);
	return read_result;
}

void assert_defaults(const SettingsService &service)
{
	zassert_equal(strcmp(service.values().url, kDefaultSettings.url), 0);
	zassert_equal(strcmp(service.values().name, kDefaultSettings.name), 0);
	zassert_equal(service.values().brightness, kDefaultSettings.brightness);
	zassert_equal(service.values().default_mode, kDefaultSettings.default_mode);
}
} // namespace

extern "C" int __real_settings_load_subtree_direct(const char *, settings_load_direct_cb, void *);

extern "C" int __wrap_settings_load_subtree_direct(const char *subtree,
						   settings_load_direct_cb callback, void *context)
{
	if (!inject_reads) {
		return __real_settings_load_subtree_direct(subtree, callback, context);
	}
	if (strcmp(subtree, "badge/art-meta") == 0) {
		return 0;
	}
	zassert_equal(strcmp(subtree, "badge"), 0);
	uint8_t schema = kSchemaVersion;
	zassert_ok(callback("schema", 1, exact_read, &schema, context));
	char name[] = "LOADED NAME";
	zassert_ok(callback("name", sizeof(name) - 1, exact_read, name, context));
	/* Match the pinned SDK: its direct loader discards the backend's return.
	 * Also simulate another backend callback after a failed read. */
	const int rc = callback(record_key, record_length, injected_read, nullptr, context);
	if (rc != 0) {
		zassert_equal(callback("mode", 1, injected_read, nullptr, context), rc);
	}
	return 0;
}

ZTEST(settings_store, test_read_errors_discard_partial_load)
{
	const ssize_t results[] = {-EIO, -ENOMEM, 0};
	inject_reads = true;
	record_length = 1;
	for (ssize_t result : results) {
		read_result = result;
		reads = 0;
		SettingsService service;
		zassert_equal(load_settings(service, false), result < 0 ? result : -EIO);
		assert_defaults(service);
		zassert_equal(reads, 1);
	}
	/* A positive short read is also an I/O failure. */
	record_key = "name"; /* Two bytes are valid for this field's size bound. */
	record_length = 2;
	read_result = 1;
	SettingsService service;
	zassert_equal(load_settings(service, false), -EIO);
	assert_defaults(service);
	record_key = "brightness";
	inject_reads = false;
}

ZTEST(settings_store, test_invalid_records_default_without_read_error)
{
	inject_reads = true;
	const size_t lengths[] = {0, kNameMax + 1, 1};
	for (size_t length : lengths) {
		record_length = length;
		read_result = length;
		record = 0; /* Invalid brightness, successfully read. */
		reads = 0;
		SettingsService service;
		zassert_ok(load_settings(service, false));
		zassert_equal(strcmp(service.values().name, "LOADED NAME"), 0);
		zassert_equal(service.values().brightness, kDefaultSettings.brightness);
		zassert_equal(reads, length == 1 ? 1 : 0);
	}
	record = 64;
	inject_reads = false;
}
