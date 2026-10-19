/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>

namespace badge
{
class SettingsService;
class Modes;
void start_nfc(const char *url, bool writable);
/* Main-loop owner applies debug URL requests; NFC transactions run in their
 * worker so reader contention cannot stall the display or watchdog. */
bool poll_nfc(SettingsService &settings, Modes &modes);
void nfc_display_result(int result);
/* Blocking worker API; never call from a Bluetooth callback or main loop. */
int change_nfc_url(const char *url, size_t length);
int resync_nfc();
int nfc_last_result();
bool nfc_ready();
bool nfc_update_busy();
} // namespace badge
