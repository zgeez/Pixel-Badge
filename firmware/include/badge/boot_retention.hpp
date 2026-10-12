/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdint.h>

namespace badge
{
uint8_t retained_boot_count();
void retain_boot_count(uint8_t value);
} // namespace badge
