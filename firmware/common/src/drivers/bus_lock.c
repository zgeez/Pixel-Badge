/* SPDX-License-Identifier: MIT */
#include "bus_lock.h"
#include <zephyr/kernel.h>

K_MUTEX_DEFINE(badge_i2c_mutex);

int badge_bus_lock(void)
{
	return k_mutex_lock(&badge_i2c_mutex, K_MSEC(50));
}
void badge_bus_unlock(void)
{
	k_mutex_unlock(&badge_i2c_mutex);
}
