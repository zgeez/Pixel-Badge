/* SPDX-License-Identifier: MIT */
#pragma once

#include "badge/settings.hpp"

namespace badge
{
/* Call from the application thread before any consumer uses settings.
 * Factory reset erases the entire settings partition before mounting NVS. */
int load_settings(SettingsService &service, bool factory_reset);
int store_field(void *context, Field field, const void *data, size_t length);
} // namespace badge
