/* SPDX-License-Identifier: MIT */
#pragma once
#include "badge/ble_protocol.hpp"

namespace badge
{
class Modes;
using ApplyConfig = int (*)(void *, const ConfigCommand &);
void start_ble();
/* Called only by the main-loop settings/display owner. Never blocks on NFC. */
void poll_ble(const Settings &settings, Modes &modes, bool writable, ApplyConfig apply,
	      void *context);
void open_ble_pairing();
bool ble_pairing_frame(Frame &frame);
bool ble_config_busy();
bool ble_artwork_busy();
} // namespace badge
