/*
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

int main(void)
{
	LOG_INF("smart-led-badge starting on %s", CONFIG_BOARD_TARGET);

	return 0;
}
