/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/device.h>

namespace badge
{

/* Cycles the display test patterns forever. Returns only on an error. */
int run_display_demo(const struct device *display);

} // namespace badge
