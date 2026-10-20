/* SPDX-License-Identifier: MIT */
#pragma once
#include <zephyr/device.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ICM-42688-P I2C driver (CONFIG_ICM42688_I2C): gyro and accel off. */
int badge_imu_sleep(const struct device *dev);
#ifdef __cplusplus
}
#endif
