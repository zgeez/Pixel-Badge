/*
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "badge/bringup.hpp"
#include "badge/matrix_diag.hpp"
#include "badge/display_demo.hpp"
#include "badge/qr_screening.hpp"
#include "badge/prototype.hpp"
#include "badge/audio_screening.hpp"
#if defined(CONFIG_APP_POWER_MANAGER)
#include "badge/power_manager.hpp"
#endif
#if defined(CONFIG_APP_POWER_STATES)
#include "badge/charging_only.hpp"
#endif

LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

#if DT_HAS_CHOSEN(zephyr_display) && defined(CONFIG_DISPLAY) && !defined(CONFIG_APP_PROTOTYPE) &&  \
	!defined(CONFIG_APP_AUDIO_SCREENING)
/* The display test patterns run in their own thread, so the module checks
 * below run while the display is refreshing. */
static void display_thread(void *, void *, void *)
{
	const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!device_is_ready(display)) {
		LOG_ERR("display %s not ready", display->name);
		return;
	}
#if defined(CONFIG_APP_MATRIX_BENCH)
	badge::run_matrix_bench(display);
#elif defined(CONFIG_APP_QR_SCREENING)
	badge::run_qr_screening(display);
#else
	badge::run_display_demo(display);
#endif
}

K_THREAD_DEFINE(display_tid, 2048, display_thread, NULL, NULL, NULL, 7, 0, 0);
#endif

#if defined(CONFIG_APP_BRINGUP_CHECKS)
static void bringup_thread(void *, void *, void *)
{
	/* Allow the badge loop to start; self-checks cannot block its watchdog. */
	k_sleep(K_SECONDS(2));
	badge::run_bringup_checks();
	while (true) {
		k_sleep(K_SECONDS(5));
		badge::log_bringup_readings();
	}
}
K_THREAD_DEFINE(bringup_tid, 3072, bringup_thread, NULL, NULL, NULL, 8, 0, 0);
#endif

int main(void)
{
	LOG_INF("smart-led-badge starting on %s", CONFIG_BOARD_TARGET);

#if defined(CONFIG_APP_POWER_MANAGER) && !defined(CONFIG_APP_BRINGUP_CHECKS)
	/* Bring-up images start it from power_diag_check() instead. */
	(void)badge::power_manager_start(false);
#endif
#if defined(CONFIG_APP_POWER_STATES)
	/* Switch OFF while running means USB holds the logic rail up. An
	 * unreadable switch counts as on. */
	if (badge::power_switch_state(500) == 0) {
		badge::run_charging_only();
	}
#endif

#if defined(CONFIG_APP_PROTOTYPE)
	badge::run_prototype();
#endif
#if defined(CONFIG_APP_AUDIO_SCREENING)
	badge::run_audio_screening();
#endif

	while (true) {
		k_sleep(K_SECONDS(5));
	}

	return 0;
}
