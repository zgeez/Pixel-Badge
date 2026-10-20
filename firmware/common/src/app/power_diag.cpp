/*
 * SPDX-License-Identifier: MIT
 *
 * Rev A power bring-up diagnostics: reset cause and 32.768 kHz clock source,
 * then the shared charger and battery manager (power_manager.cpp) in verbose
 * mode: SAADC rails, BQ25620 identity, faults, TS zone, D+/D- result and its
 * ADC, the input-current policy, gated charging with CONFIG_APP_CHARGING,
 * button and switch edges and a status-LED heartbeat.
 */

#include "badge/power_diag.hpp"
#include "badge/power_manager.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(power_diag, CONFIG_APP_LOG_LEVEL);

/* Bring-up images only: the product image does not log this. */
#if defined(CONFIG_APP_BRINGUP_CHECKS) && defined(CONFIG_APP_POWER_MANAGER)

#if defined(CONFIG_HWINFO)
#include <zephyr/drivers/hwinfo.h>
#endif
#include <soc.h>

namespace badge
{
namespace
{

void log_reset_and_clock()
{
#if defined(CONFIG_HWINFO)
	uint32_t cause = 0;

	if (hwinfo_get_reset_cause(&cause) == 0) {
		LOG_INF("Reset cause 0x%08x (pin %d, por %d, watchdog %d, software %d, debug %d)",
			cause, !!(cause & RESET_PIN), !!(cause & RESET_POR),
			!!(cause & RESET_WATCHDOG), !!(cause & RESET_SOFTWARE),
			!!(cause & RESET_DEBUG));
		(void)hwinfo_clear_reset_cause();
	}
#endif
	const uint32_t lf = NRF_CLOCK->LFCLKSTAT;
	const uint32_t src = (lf & CLOCK_LFCLKSTAT_SRC_Msk) >> CLOCK_LFCLKSTAT_SRC_Pos;

	LOG_INF("LFCLK %s, source %s", (lf & CLOCK_LFCLKSTAT_STATE_Msk) ? "running" : "stopped",
		src == CLOCK_LFCLKSTAT_SRC_Xtal ? "32.768 kHz crystal"
						: (src == CLOCK_LFCLKSTAT_SRC_RC ? "RC" : "synth"));
}

} // namespace

int power_diag_check()
{
	log_reset_and_clock();
	return power_manager_start(true);
}

} // namespace badge

#else

namespace badge
{
int power_diag_check()
{
	return 0;
}
} // namespace badge

#endif
