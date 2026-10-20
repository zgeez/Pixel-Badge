/*
 * SPDX-License-Identifier: MIT
 *
 * Rev A power bring-up diagnostics: rails, charger, inputs. See
 * src/app/power_diag.cpp. A no-op on boards without the charger bus.
 */

#pragma once

namespace badge
{

/* Logs the reset cause and clock source, identifies and configures the
 * charger, reads the rails once, then starts the periodic power thread.
 * Returns the number of failed checks. */
int power_diag_check();

} // namespace badge
