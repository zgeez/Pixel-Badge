/* SPDX-License-Identifier: MIT */
#pragma once
#include "badge/modes.hpp"
namespace badge
{
void start_motion();
/* Main-loop owner consumes snapshots; BLE only reads the published status. */
void poll_motion(Modes &modes, uint32_t now);
void motion_status(uint8_t output[16]);
} // namespace badge
