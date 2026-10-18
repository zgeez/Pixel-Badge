/* SPDX-License-Identifier: MIT */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif
/* Shared prototype I2C bus. Recursive Zephyr mutex; never call from an ISR. */
int badge_bus_lock(void);
void badge_bus_unlock(void);
#ifdef __cplusplus
}
#endif
