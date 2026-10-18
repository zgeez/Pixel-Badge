/*
 * SPDX-License-Identifier: MIT
 *
 * Display bring-up: cycles test patterns through the Zephyr display API, so
 * it runs unchanged on any display backend.
 */

#include "badge/display_demo.hpp"

#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "badge/test_pattern.hpp"

LOG_MODULE_REGISTER(display_demo, CONFIG_APP_LOG_LEVEL);

namespace badge
{
namespace
{

int show(const struct device *display, const Frame &frame)
{
	struct display_buffer_descriptor desc = {};

	desc.buf_size = sizeof(frame.bytes);
	desc.width = kMatrixSize;
	desc.height = kMatrixSize;
	desc.pitch = kMatrixSize;
	return display_write(display, 0, 0, &desc, frame.bytes);
}

int hold(const struct device *display, const char *name, Pattern pattern, int ms)
{
	Frame frame;

	draw(frame, pattern);
	LOG_INF("%s", name);
	int err = show(display, frame);

	k_msleep(ms);
	return err;
}

} // namespace

int run_display_demo(const struct device *display)
{
	struct display_capabilities caps;
	Frame frame;
	int err;

	display_get_capabilities(display, &caps);
	if (caps.x_resolution != kMatrixSize || caps.y_resolution != kMatrixSize ||
	    !(caps.supported_pixel_formats & PIXEL_FORMAT_MONO01)) {
		LOG_ERR("display is %ux%u, expected %dx%d mono", caps.x_resolution,
			caps.y_resolution, kMatrixSize, kMatrixSize);
		return -ENOTSUP;
	}

	frame.clear();
	err = show(display, frame);
	if (err == 0) {
		err = display_blanking_off(display);
	}

	while (err == 0) {
		err = hold(display, "orientation: block top-left, long arm along the top",
			   Pattern::kOrientation, 5000);
		if (err == 0) {
			err = hold(display, "all on", Pattern::kAllOn, 2000);
		}
		if (err == 0) {
			err = hold(display, "checkerboard", Pattern::kCheckerboard, 2000);
		}
		if (err == 0) {
			err = hold(display, "border", Pattern::kBorder, 2000);
		}
		LOG_INF("row walk, top to bottom");
		for (int y = 0; err == 0 && y < kMatrixSize; y++) {
			draw_row(frame, y);
			err = show(display, frame);
			k_msleep(100);
		}
		LOG_INF("column walk, left to right");
		for (int x = 0; err == 0 && x < kMatrixSize; x++) {
			draw_column(frame, x);
			err = show(display, frame);
			k_msleep(100);
		}
		draw(frame, Pattern::kAllOn);
		if (err == 0) {
			err = show(display, frame);
		}
		for (int level = 255; err == 0 && level > 0; level /= 2) {
			LOG_INF("brightness %d", level);
			err = display_set_brightness(display, static_cast<uint8_t>(level));
			k_msleep(1000);
		}
		if (err == 0) {
			err = display_set_brightness(display, CONFIG_APP_DISPLAY_BRIGHTNESS);
		}
	}

	LOG_ERR("display demo stopped: %d", err);
	return err;
}

} // namespace badge
