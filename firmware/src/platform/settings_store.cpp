/* SPDX-License-Identifier: MIT */
#include "badge/settings_store.hpp"

#include <zephyr/settings/settings.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(badge_settings, CONFIG_APP_LOG_LEVEL);

namespace badge
{
namespace
{
struct LoadContext {
	SettingsService &service;
	int read_error = 0;
};

int read_setting(const char *key, size_t length, settings_read_cb read, void *argument,
		 void *context)
{
	auto &load = *static_cast<LoadContext *>(context);
	if (load.read_error != 0) {
		return load.read_error;
	}
	for (unsigned i = 0; i < static_cast<unsigned>(Field::kCount); ++i) {
		const Field field = static_cast<Field>(i);
		if (strcmp(key, field_name(field)) != 0) {
			continue;
		}
		/* Startup loader has one owner; avoid a large transient stack buffer. */
		static uint8_t bytes[kArtworkBytes];
		constexpr size_t maxima[] = {1, kPayloadMax, kNameMax,     1,
					     1, kTextBytes,  kArtworkBytes};
		static_assert(sizeof(maxima) / sizeof(maxima[0]) ==
			      static_cast<unsigned>(Field::kCount));
		if (length == 0 || length > maxima[i]) {
			LOG_WRN("invalid setting %s; using default", field_name(field));
			break;
		}
		const ssize_t rc = read(argument, bytes, length);
		if (rc != static_cast<ssize_t>(length)) {
			load.read_error = rc < 0 ? static_cast<int>(rc) : -EIO;
			LOG_ERR("setting %s read failed (%d)", field_name(field), load.read_error);
			return load.read_error;
		}
		if (!load.service.load(field, bytes, length)) {
			LOG_WRN("invalid setting %s; using default", field_name(field));
		}
		break;
	}
	return 0;
}
} // namespace

int load_settings(SettingsService &service, bool factory_reset)
{
	service.begin_load();
	if (factory_reset) {
		const struct flash_area *area;
		int rc = flash_area_open(PARTITION_ID(storage_partition), &area);
		if (rc != 0) {
			return rc;
		}
		rc = flash_area_flatten(area, 0, area->fa_size);
		flash_area_close(area);
		if (rc != 0) {
			return rc;
		}
		LOG_INF("factory reset: settings partition erased");
	}
	int rc = settings_subsys_init();
	if (rc == 0) {
		LoadContext load{service};
		rc = settings_load_subtree_direct("badge", read_setting, &load);
		/* The pinned SDK discards backend callback errors in the direct
		 * loader. Preserve the first read failure independently. */
		if (rc == 0) {
			rc = load.read_error;
		}
	}
	if (rc != 0) {
		service.begin_load();
	}
	const uint8_t missing = service.finish_load();
	for (unsigned i = 0; i < static_cast<unsigned>(Field::kCount); ++i) {
		if ((missing & (1U << i)) != 0) {
			LOG_WRN("setting %s defaulted", field_name(static_cast<Field>(i)));
		}
	}
	return rc;
}

int store_field(void *, Field field, const void *data, size_t length)
{
	const char *name = field_name(field);
	if (name == nullptr) {
		return -EINVAL;
	}
	char key[32] = "badge/";
	strcat(key, name);
	return settings_save_one(key, data, length);
}
} // namespace badge
