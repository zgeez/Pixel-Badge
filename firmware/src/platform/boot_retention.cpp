/* SPDX-License-Identifier: MIT */
#include "badge/boot_retention.hpp"

#include <hal/nrf_power.h>

namespace badge
{
/* Application owns POWER GPREGRET[1]. The boot-mode retention used by
 * MCUboot is not enabled in these images. No ISR accesses this register. */
uint8_t retained_boot_count()
{
	return static_cast<uint8_t>(nrf_power_gpregret_get(NRF_POWER, 1));
}

void retain_boot_count(uint8_t value)
{
	nrf_power_gpregret_set(NRF_POWER, 1, value);
}
} // namespace badge
