/* SPDX-License-Identifier: MIT */
#pragma once
#include <zephyr/device.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Succeeds only after the queued frame becomes the scanning front buffer. */
int badge_hub75_wait_visible(const struct device *dev, unsigned timeout_ms);
#ifdef __cplusplus
}
#endif
