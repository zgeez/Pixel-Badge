/* SPDX-License-Identifier: MIT */
#pragma once

namespace badge
{
class SettingsService;
class Modes;
void start_nfc(const char *url, bool writable);
/* Main-loop owner applies debug URL requests; NFC transactions run in their
 * worker so reader contention cannot stall the display or watchdog. */
bool poll_nfc(SettingsService &settings, Modes &modes);
void nfc_display_result(int result);
} // namespace badge
