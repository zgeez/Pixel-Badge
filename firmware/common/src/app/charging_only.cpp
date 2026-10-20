/* SPDX-License-Identifier: MIT */
#include "badge/charging_only.hpp"
#include "badge/power_manager.hpp"

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(charging_only, CONFIG_APP_LOG_LEVEL);

namespace badge
{
/* docs/power.md, Power states: with the switch OFF the logic rail is up only
 * because USB is present. The matrix, microphone, NFC I2C side and Bluetooth
 * are never started here and the IMU is put to sleep; the power manager keeps
 * charging and the input limit. USB removal turns the rail off in hardware. */
void run_charging_only()
{
	power_enter_charging_only();

	const device *watchdog = DEVICE_DT_GET(DT_NODELABEL(wdt0));
	wdt_timeout_cfg timeout{};
	timeout.window.max = 8000;
	timeout.flags = WDT_FLAG_RESET_SOC;
	int channel = device_is_ready(watchdog) ? wdt_install_timeout(watchdog, &timeout) : -ENODEV;
	if (channel >= 0 && wdt_setup(watchdog, 0) != 0) {
		channel = -1;
	}
	if (channel < 0) {
		LOG_ERR("watchdog unavailable (%d)", channel);
	}
	while (true) {
		if (power_switch_state() == 1) {
			LOG_INF("power switch on: restarting into normal operation");
			LOG_PANIC();
			sys_reboot(SYS_REBOOT_COLD);
		}
		/* A stalled manager stops the feed: the reset drops CHG_EN. */
		if (channel >= 0 && power_manager_healthy()) {
			wdt_feed(watchdog, channel);
		}
		k_sleep(K_MSEC(100));
	}
}
} // namespace badge
