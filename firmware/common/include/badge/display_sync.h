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
#if defined(CONFIG_REVA_MATRIX_STALL_TEST)
/* Matrix bench only. With the matrix blanked, lights one LED (row 0, column 0)
 * after a single LAT pulse and sends no more, so the external one-shot must
 * blank it. Returns 0 with the time from the pulse to MTX_SAFE_SENSE high in
 * *safe_us; -ETIMEDOUT if SAFE stayed low for limit_us (a timer compare then
 * re-blanked through PPI); -EIO if SAFE did not drop at the pulse; -EBUSY
 * unless the matrix is blanked with a power ceiling above 0. */
int badge_display_stall_test(const struct device *dev, unsigned limit_us, unsigned *safe_us);
#endif
#ifdef __cplusplus
}
#endif
