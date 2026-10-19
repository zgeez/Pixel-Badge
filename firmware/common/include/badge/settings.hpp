/* SPDX-License-Identifier: MIT */
#pragma once

#include "badge/qr.hpp"
#include "badge/content.hpp"
#include "badge/motion.hpp"

namespace badge
{

constexpr size_t kNameMax = 32;
constexpr uint8_t kSchemaVersion = 1;
enum class Mode : uint8_t {
	kQr,
	kName,
	kCanvas,
	kAnimation,
	kAudioLevel,
	kAudioSpectrum,
	kFluid
};
enum class Field : uint8_t {
	kSchema,
	kUrl,
	kName,
	kBrightness,
	kDefaultMode,
	kText,
	kArtwork,
	kMotion,
	kCount
};
/* Nonpersistent command, kept outside the storage field namespace. */
constexpr Field kNfcSyncField = static_cast<Field>(127);

struct Settings {
	char url[kPayloadMax + 1];
	char name[kNameMax + 1];
	uint8_t brightness;
	Mode default_mode;
	TextContent text{};
	Artwork artwork{};
	MotionConfig motion{};
};

extern const Settings kDefaultSettings;
const char *field_name(Field field);
using SaveField = int (*)(void *context, Field field, const void *data, size_t length);

/* Single owner (the application loop). No multi-field transaction is implied.
 * Storage callback must atomically replace one field or return an error.
 * Artwork is a borrowed immutable view: its backing must outlive all consumers
 * (mapped inactive/active flash on target, static legacy load cache at boot). */
class SettingsService
{
public:
	SettingsService();
	const Settings &values() const
	{
		return values_;
	}
	void begin_load();
	bool load(Field field, const void *data, size_t length);
	uint8_t finish_load(); /* bit per missing/invalid/defaulted field */
	int save(Field field, const void *data, size_t length, SaveField write, void *context);

private:
	Settings values_;
	uint8_t loaded_ = 0;
	bool schema_ok_ = false;
	bool apply(Field field, const void *data, size_t length);
};

} // namespace badge
