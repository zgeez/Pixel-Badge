/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/device.h>

namespace badge
{

/* Shows the QR screening settings, stepped with Buttons 1 and 2. Returns only
 * on an error. */
int run_qr_screening(const struct device *display);

} // namespace badge
