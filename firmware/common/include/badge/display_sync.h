/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/device.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Succeeds only after the queued frame becomes the scanning front buffer. */
int badge_display_wait_visible(const struct device *dev, unsigned timeout_ms);
#if defined(CONFIG_REVA_MATRIX)
/* Power-policy brightness ceiling, enforced on every slot whatever level the
 * application sets. 0 turns the LED rail off and stops scanning without a
 * driver fault; a frame written meanwhile shows once the ceiling rises. */
void badge_display_set_power_cap(const struct device *dev, uint8_t ceiling);
/* True while the driver has the LED rail enabled (loaded battery readings). */
bool badge_display_rail_on(const struct device *dev);
#endif
#ifdef __cplusplus
}
#endif
