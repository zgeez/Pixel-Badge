/* SPDX-License-Identifier: MIT */
#pragma once

#include "badge/settings.hpp"

namespace badge
{
/* Call from the application thread before any consumer uses settings.
 * Factory reset erases the entire settings partition before mounting NVS. */
int load_settings(SettingsService &service, bool factory_reset);
/* Serialized by the dedicated BLE flash worker, or the main owner at commit. */
int stage_artwork_flash(uint32_t offset, uint32_t total, const uint8_t *bytes, size_t length,
			const uint8_t **view);
uint32_t current_artwork_crc(const Artwork &view);
int store_field(void *context, Field field, const void *data, size_t length);
} // namespace badge
