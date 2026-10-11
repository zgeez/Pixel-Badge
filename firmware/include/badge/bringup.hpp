/*
 * SPDX-License-Identifier: MIT
 *
 * Module self-tests for bring-up: IMU, NFC tag and microphone. Each check is
 * built only when the board describes that part.
 */

#pragma once

namespace badge
{

/* Runs every available check once and logs the results. Returns the number of
 * failed checks. */
int run_bringup_checks();

/* Logs one IMU sample and the microphone level. */
void log_bringup_readings();

} // namespace badge
