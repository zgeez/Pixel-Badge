/*
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "badge/bringup.hpp"
#include "badge/display_demo.hpp"
#include "badge/qr_screening.hpp"
#include "badge/prototype.hpp"
#include "badge/audio_screening.hpp"

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
#if defined(CONFIG_APP_QR_SCREENING)
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
