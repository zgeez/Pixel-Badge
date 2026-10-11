/*
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "badge/bringup.hpp"
#include "badge/display_demo.hpp"

LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

#if DT_HAS_CHOSEN(zephyr_display) && defined(CONFIG_DISPLAY)
/* The display test patterns run in their own thread, so the module checks
 * below run while the display is refreshing. */
static void display_thread(void *, void *, void *)
{
	const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!device_is_ready(display)) {
		LOG_ERR("display %s not ready", display->name);
		return;
	}
	badge::run_display_demo(display);
}

K_THREAD_DEFINE(display_tid, 2048, display_thread, NULL, NULL, NULL, 7, 0, 0);
#endif

int main(void)
{
	LOG_INF("smart-led-badge starting on %s", CONFIG_BOARD_TARGET);

	badge::run_bringup_checks();

	while (true) {
		k_sleep(K_SECONDS(5));
		badge::log_bringup_readings();
	}

	return 0;
}
