/* SPDX-License-Identifier: MIT */
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#define USER DT_PATH(zephyr_user)
namespace
{
int safe_outputs()
{
	const gpio_dt_spec outputs[] = {
		GPIO_DT_SPEC_GET(USER, mtx_blank_gpios),    GPIO_DT_SPEC_GET(USER, mtx_arm_gpios),
		GPIO_DT_SPEC_GET(USER, led_en_gpios),       GPIO_DT_SPEC_GET(USER, chg_en_gpios),
		GPIO_DT_SPEC_GET(USER, vbat_meas_en_gpios), GPIO_DT_SPEC_GET(USER, nfc_pwr_gpios),
	};
	for (unsigned i = 0; i < ARRAY_SIZE(outputs); ++i) {
		if (!gpio_is_ready_dt(&outputs[i])) {
			return -ENODEV;
		}
		int rc = gpio_pin_configure_dt(&outputs[i],
					       i == 0 ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE);
		if (rc != 0) {
			return rc;
		}
	}
	return 0;
}
} // namespace
/* GPIO drivers initialize before this; the display initializes afterwards. */
SYS_INIT(safe_outputs, POST_KERNEL, 60);
