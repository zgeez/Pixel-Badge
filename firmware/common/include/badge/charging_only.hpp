/* SPDX-License-Identifier: MIT */
#pragma once

namespace badge
{
/* Switch OFF with USB present: run only the charger manager until the switch
 * turns on, then restart into normal operation. Never returns. */
[[noreturn]] void run_charging_only();
} // namespace badge
