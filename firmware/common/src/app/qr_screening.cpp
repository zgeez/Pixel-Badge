/*
 * SPDX-License-Identifier: MIT
 *
 * QR screening on the display (validation plan, QR scan reliability): shows
 * each host-generated test code in normal and inverted polarity at three
 * brightness levels. Button 1 steps to the next setting, Button 2 back; every
 * change is logged with its number so results can be recorded per setting.
 */

#include "badge/qr_screening.hpp"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "badge/qr_test_codes.hpp"
#include "badge/test_pattern.hpp"

LOG_MODULE_REGISTER(qr_screening, CONFIG_APP_LOG_LEVEL);

namespace badge
{
namespace
{

constexpr uint8_t kBrightness[] = {255, 64, 16};
constexpr size_t kLevels = sizeof(kBrightness);

size_t settings_count()
{
	return kQrTestCodeCount * 2 * kLevels;
}

struct Setting {
	const QrTestCode *code;
	bool inverted;
	uint8_t brightness;
};

Setting setting_at(size_t i)
{
	Setting s;

	s.code = &kQrTestCodes[i / (2 * kLevels)];
	s.inverted = ((i / kLevels) % 2) != 0;
	s.brightness = kBrightness[i % kLevels];
	return s;
}

int show(const struct device *display, size_t i)
{
	const Setting s = setting_at(i);
	struct display_buffer_descriptor desc = {};
	Frame frame;

	for (size_t b = 0; b < kFrameBytes; b++) {
		frame.bytes[b] =
			s.inverted ? static_cast<uint8_t>(~s.code->frame[b]) : s.code->frame[b];
	}
	desc.buf_size = sizeof(frame.bytes);
	desc.width = kMatrixSize;
	desc.height = kMatrixSize;
	desc.pitch = kMatrixSize;

	int err = display_set_brightness(display, s.brightness);

	if (err == 0) {
		err = display_write(display, 0, 0, &desc, frame.bytes);
	}
	LOG_INF("QR setting %u/%u: %s \"%s\", %s, brightness %u", static_cast<unsigned>(i + 1),
		static_cast<unsigned>(settings_count()), s.code->name, s.code->payload,
		s.inverted ? "inverted" : "normal", s.brightness);
	return err;
}

} // namespace

int run_qr_screening(const struct device *display)
{
	const struct gpio_dt_spec next = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
	const struct gpio_dt_spec prev = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
	size_t i = 0;
	int err;

	if (!gpio_is_ready_dt(&next) || !gpio_is_ready_dt(&prev)) {
		LOG_ERR("buttons not ready");
		return -ENODEV;
	}
	err = gpio_pin_configure_dt(&next, GPIO_INPUT);
	if (err == 0) {
		err = gpio_pin_configure_dt(&prev, GPIO_INPUT);
	}
	if (err != 0) {
		return err;
	}

	LOG_INF("QR screening: %u settings; Button 1 next, Button 2 back",
		static_cast<unsigned>(settings_count()));
	err = show(display, i);
	if (err == 0) {
		err = display_blanking_off(display);
	}

	bool next_was = false;
	bool prev_was = false;

	while (err == 0) {
		k_msleep(30);
		bool next_now = gpio_pin_get_dt(&next) > 0;
		bool prev_now = gpio_pin_get_dt(&prev) > 0;

		if (next_now && !next_was) {
			i = (i + 1) % settings_count();
			err = show(display, i);
		} else if (prev_now && !prev_was) {
			i = (i + settings_count() - 1) % settings_count();
			err = show(display, i);
		}
		next_was = next_now;
		prev_was = prev_now;
	}
	LOG_ERR("QR screening stopped: %d", err);
	return err;
}

} // namespace badge
