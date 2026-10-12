/* SPDX-License-Identifier: MIT */
#include "badge/settings.hpp"

#include <errno.h>
#include <string.h>

#ifdef BADGE_CONFIGURED_DEFAULT_URL
#include "badge/default_url.hpp"
#else
#define BADGE_DEFAULT_URL "HTTPS://EXAMPLE.COM"
#endif

namespace badge
{
static_assert(sizeof(BADGE_DEFAULT_URL) <= kPayloadMax + 1);
const Settings kDefaultSettings = {BADGE_DEFAULT_URL, "BADGE", 128, Mode::kQr};

const char *field_name(Field field)
{
	static const char *const names[] = {"schema", "url", "name", "brightness", "mode"};
	return field < Field::kCount ? names[static_cast<unsigned>(field)] : nullptr;
}

SettingsService::SettingsService() : values_(kDefaultSettings)
{
}

void SettingsService::begin_load()
{
	values_ = kDefaultSettings;
	loaded_ = 0;
	schema_ok_ = false;
}

bool SettingsService::apply(Field field, const void *data, size_t length)
{
	if (data == nullptr || length == 0) {
		return false;
	}
	const auto *bytes = static_cast<const uint8_t *>(data);
	switch (field) {
	case Field::kSchema:
		return length == 1 && bytes[0] == kSchemaVersion;
	case Field::kUrl:
		if (!valid_url(static_cast<const char *>(data), length)) {
			return false;
		}
		memcpy(values_.url, data, length);
		values_.url[length] = '\0';
		return true;
	case Field::kName:
		if (length > kNameMax) {
			return false;
		}
		for (size_t i = 0; i < length; ++i) {
			if (bytes[i] < 0x20 || bytes[i] > 0x7e) {
				return false;
			}
		}
		memcpy(values_.name, data, length);
		values_.name[length] = '\0';
		return true;
	case Field::kBrightness:
		if (length != 1 || bytes[0] == 0) {
			return false;
		}
		values_.brightness = bytes[0];
		return true;
	case Field::kDefaultMode:
		if (length != 1 || bytes[0] > static_cast<uint8_t>(Mode::kName)) {
			return false;
		}
		values_.default_mode = static_cast<Mode>(bytes[0]);
		return true;
	default:
		return false;
	}
}

bool SettingsService::load(Field field, const void *data, size_t length)
{
	if (!apply(field, data, length)) {
		return false;
	}
	loaded_ |= 1U << static_cast<unsigned>(field);
	if (field == Field::kSchema) {
		schema_ok_ = true;
	}
	return true;
}

uint8_t SettingsService::finish_load()
{
	if (!schema_ok_) {
		values_ = kDefaultSettings;
		loaded_ = 0;
	}
	return ((1U << static_cast<unsigned>(Field::kCount)) - 1U) & ~loaded_;
}

int SettingsService::save(Field field, const void *data, size_t length, SaveField write,
			  void *context)
{
	SettingsService candidate = *this;
	if (write == nullptr || !candidate.apply(field, data, length)) {
		return -EINVAL;
	}
	/* Replace every old application field before publishing the new schema.
	 * Until that last write succeeds, a reboot still ignores the entire set. */
	if (!schema_ok_) {
		const Settings &defaults = kDefaultSettings;
		const void *data[] = {defaults.url, defaults.name, &defaults.brightness,
				      &defaults.default_mode};
		const size_t lengths[] = {strlen(defaults.url), strlen(defaults.name),
					  sizeof(defaults.brightness),
					  sizeof(defaults.default_mode)};
		constexpr unsigned fields = sizeof(data) / sizeof(data[0]);
		static_assert(fields == static_cast<unsigned>(Field::kCount) - 1);
		for (unsigned i = 0; i < fields; ++i) {
			const int rc =
				write(context, static_cast<Field>(i + 1), data[i], lengths[i]);
			if (rc != 0) {
				return rc;
			}
		}
		const uint8_t schema = kSchemaVersion;
		int rc = write(context, Field::kSchema, &schema, sizeof(schema));
		if (rc != 0) {
			return rc;
		}
		schema_ok_ = true;
	}
	int rc = write(context, field, data, length);
	if (rc == 0) {
		values_ = candidate.values_;
	}
	return rc;
}
} // namespace badge
