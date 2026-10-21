/*
 * SPDX-License-Identifier: MIT
 *
 * Rev A matrix bring-up:
 * - matrix_chain_check(): with the matrix output disabled (LED rail off,
 *   ARM low, BLANK high), reads MTX_SAFE_SENSE and shifts known patterns
 *   through the two TLC59283 registers, reading them back on MTX_MISO
 *   (bring-up step 13). No LAT pulse is sent, so no output latch changes.
 * - run_matrix_bench(): button-stepped staged tests for the current-limited
 *   matrix bench (steps 14-19): rows with zero column current, single LEDs
 *   at minimum brightness, row/column walks, patterns, rising brightness,
 *   then the scan-stall test (bring-up step 16). MODE advances, ACTION toggles
 *   blanking (stop), or repeats the stall test on its step.
 */

#include "badge/matrix_diag.hpp"
#include "badge/display_sync.h"
#include "badge/matrix_chain.hpp"
#include "badge/test_pattern.hpp"

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(matrix_diag, CONFIG_APP_LOG_LEVEL);

#define MATRIX_NODE DT_CHOSEN(zephyr_display)
#define USER_NODE   DT_PATH(zephyr_user)

/* Bring-up images only. */
#if defined(CONFIG_APP_BRINGUP_CHECKS) && DT_HAS_CHOSEN(zephyr_display) &&                         \
	DT_NODE_HAS_COMPAT(MATRIX_NODE, smartledbadge_reva_matrix) && defined(CONFIG_SPI)
#define HAVE_REVA_MATRIX 1
#include <zephyr/drivers/spi.h>
#else
#define HAVE_REVA_MATRIX 0
#endif

namespace badge
{

#if HAVE_REVA_MATRIX
namespace
{

const struct device *const matrix_spi = DEVICE_DT_GET(DT_PHANDLE(MATRIX_NODE, spi));
const struct gpio_dt_spec safe_sense = GPIO_DT_SPEC_GET(MATRIX_NODE, safe_gpios);

/* 1 MHz, mode 0, MSB first: well inside the TLC59283 and SPIM limits. */
struct spi_config make_readback_config()
{
	struct spi_config config = {};

	config.frequency = 1000000;
	config.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB;
	return config;
}
const struct spi_config readback_config = make_readback_config();

/* Non-repeating 32-bit words, so only one offset can match each pattern. */
constexpr uint8_t kPatterns[][4] = {
	{0x80, 0x00, 0x00, 0x01}, {0xC3, 0x5A, 0x0F, 0x96}, {0x01, 0x23, 0x45, 0x67},
	{0xFE, 0xDC, 0xBA, 0x98}, {0x00, 0xFF, 0x13, 0xEC},
};

int shift(const uint8_t tx[8], uint8_t rx[8])
{
	struct spi_buf tx_buf = {.buf = const_cast<uint8_t *>(tx), .len = 8};
	struct spi_buf rx_buf = {.buf = rx, .len = 8};
	const struct spi_buf_set txs = {.buffers = &tx_buf, .count = 1};
	const struct spi_buf_set rxs = {.buffers = &rx_buf, .count = 1};

	return spi_transceive(matrix_spi, &readback_config, &txs, &rxs);
}

} // namespace

int matrix_chain_check()
{
	if (!gpio_is_ready_dt(&safe_sense) || gpio_pin_configure_dt(&safe_sense, GPIO_INPUT) != 0) {
		LOG_ERR("Matrix: FAIL, MTX_SAFE_SENSE input");
		return 1;
	}
	const int safe = gpio_pin_get_dt(&safe_sense);

	/* ARM low clears the one-shot: SCAN_OK low, SAFE high, columns blanked
	 * and HC595 outputs disabled (docs/hardware.md). */
	LOG_INF("Matrix: MTX_SAFE_SENSE %d (expect 1: safe-off while ARM is low)", safe);
	int failed = safe == 1 ? 0 : 1;

	if (IS_ENABLED(CONFIG_REVA_MATRIX_OUTPUT)) {
		LOG_WRN("Matrix: chain readback skipped (matrix output enabled in this image)");
		return failed;
	}
	if (!device_is_ready(matrix_spi)) {
		LOG_ERR("Matrix: FAIL, SPIM3 not ready");
		return failed + 1;
	}

	int offset = -2;

	for (const auto &pattern : kPatterns) {
		uint8_t tx[8] = {};
		uint8_t rx[8] = {};

		memcpy(tx, pattern, 4);
		int err = shift(tx, rx);

		if (err != 0) {
			LOG_ERR("Matrix: FAIL, SPI transfer %d", err);
			return failed + 1;
		}
		const int found =
			chain_offset(tx, rx, sizeof(tx), kTlcChainBits - 1, kTlcChainBits + 1);

		LOG_INF("Matrix chain: sent %02x%02x%02x%02x, read %02x%02x%02x%02x "
			"%02x%02x%02x%02x, "
			"offset %d",
			tx[0], tx[1], tx[2], tx[3], rx[0], rx[1], rx[2], rx[3], rx[4], rx[5], rx[6],
			rx[7], found);
		if (found < 0 || (offset != -2 && found != offset)) {
			LOG_ERR("Matrix: FAIL, readback does not match a %d-bit TLC chain%s",
				kTlcChainBits,
				stream_constant(rx, sizeof(rx)) ? " (MISO stuck at one level)"
								: "");
			return failed + 1;
		}
		offset = found;
	}
	/* Leave zeros (all columns off) in the registers; still no LAT pulse. */
	uint8_t zeros[8] = {};
	uint8_t rx[8] = {};

	(void)shift(zeros, rx);
	LOG_INF("Matrix: TLC chain readback OK (offset %d bits)", offset);
	return failed;
}

#if defined(CONFIG_APP_MATRIX_BENCH)
namespace
{

const struct gpio_dt_spec btn_next = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
const struct gpio_dt_spec btn_stop = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);

int show(const struct device *display, const Frame &frame)
{
	struct display_buffer_descriptor desc = {};

	desc.buf_size = sizeof(frame.bytes);
	desc.width = kMatrixSize;
	desc.height = kMatrixSize;
	desc.pitch = kMatrixSize;
	return display_write(display, 0, 0, &desc, frame.bytes);
}

enum class Step {
	kBlanked,
	kRowsOnly,
	kSingleTopLeft,
	kSingleBottomRight,
	kRowWalk,
	kColumnWalk,
	kOrientation,
	kCheckerboard,
	kInverseCheckerboard,
	kAllOn8,
	kAllOn32,
	kAllOn128,
	kStall,
	kCount,
};

const char *step_name(Step step)
{
	switch (step) {
	case Step::kBlanked:
		return "blanked, LED rail off";
	case Step::kRowsOnly:
		return "rows scanning, zero column current (brightness 0): check row gates, no LED "
		       "lit";
	case Step::kSingleTopLeft:
		return "single LED top-left, brightness 1";
	case Step::kSingleBottomRight:
		return "single LED bottom-right, brightness 1";
	case Step::kRowWalk:
		return "row walk top to bottom, brightness 8";
	case Step::kColumnWalk:
		return "column walk left to right, brightness 8";
	case Step::kOrientation:
		return "orientation marker (block top-left, long arm along the top), brightness 8";
	case Step::kCheckerboard:
		return "checkerboard, brightness 8";
	case Step::kInverseCheckerboard:
		return "inverse checkerboard, brightness 8";
	case Step::kAllOn8:
		return "all on, brightness 8: record current and VLED";
	case Step::kAllOn32:
		return "all on, brightness 32: record current and VLED";
	case Step::kAllOn128:
		return "all on, brightness 128: record current and VLED";
	case Step::kStall:
		return "scan stall: one LED lit with LAT stopped; the one-shot must blank it "
		       "(ACTION repeats)";
	case Step::kCount:
		break;
	}
	return "?";
}

/* Static frame and brightness for each step; walks animate separately. */
uint8_t prepare(Step step, Frame &frame)
{
	frame.clear();
	switch (step) {
	case Step::kSingleTopLeft:
		frame.set(0, 0);
		return 1;
	case Step::kSingleBottomRight:
		frame.set(kMatrixSize - 1, kMatrixSize - 1);
		return 1;
	case Step::kOrientation:
		draw(frame, Pattern::kOrientation);
		return 8;
	case Step::kCheckerboard:
		draw(frame, Pattern::kCheckerboard);
		return 8;
	case Step::kInverseCheckerboard:
		draw(frame, Pattern::kCheckerboard);
		for (uint8_t &b : frame.bytes) {
			b = static_cast<uint8_t>(~b);
		}
		return 8;
	case Step::kAllOn8:
		draw(frame, Pattern::kAllOn);
		return 8;
	case Step::kAllOn32:
		draw(frame, Pattern::kAllOn);
		return 32;
	case Step::kAllOn128:
		draw(frame, Pattern::kAllOn);
		return 128;
	case Step::kRowWalk:
	case Step::kColumnWalk:
		return 8;
	default:
		return 0;
	}
}

/* Debounced press: the level must hold for two 50 ms samples. Starts as
 * "pressed" so a button held at boot needs a release first. */
struct Button {
	const struct gpio_dt_spec *gpio;
	int stable = 1, candidate = 1;

	bool pressed()
	{
		const int now = gpio_pin_get_dt(gpio);

		if (now != candidate) {
			candidate = now;
			return false;
		}
		if (now == stable) {
			return false;
		}
		stable = now;
		return stable == 1;
	}
};

/* A frame queued in the driver swaps at the next scan boundary (about
 * 10 ms); a second write before then returns -EBUSY, so wait for it. */
int show_retry(const struct device *display, const Frame &frame)
{
	int err = -EBUSY;

	for (int i = 0; i < 20 && err == -EBUSY; ++i) {
		err = show(display, frame);
		if (err == -EBUSY) {
			k_msleep(5);
		}
	}
	return err;
}

/* The one-shot's width by calculation (docs/hardware.md); to be measured. */
constexpr unsigned kStallMinUs = 760;
constexpr unsigned kStallMaxUs = 1410;
/* Hard limit: the driver's timer re-blanks through PPI if SAFE stays low. */
constexpr unsigned kStallLimitUs = 3000;

int run_stall(const struct device *display)
{
	unsigned safe_us = 0;
	int err = display_blanking_on(display);

	if (err == 0) {
		err = badge_display_stall_test(display, kStallLimitUs, &safe_us);
	}
	if (err == 0) {
		LOG_INF("Matrix stall: one-shot blanked the LED %u us after the last LAT pulse",
			safe_us);
		if (safe_us < kStallMinUs || safe_us > kStallMaxUs) {
			LOG_WRN("Matrix stall: CHECK, outside the calculated %u-%u us; measure "
				"the one-shot on a scope",
				kStallMinUs, kStallMaxUs);
		}
		return 0;
	}
	if (err == -ETIMEDOUT) {
		LOG_ERR("Matrix stall: FAIL, MTX_SAFE_SENSE still low after %u us; the timer "
			"re-blanked it. Do not scan the matrix until the one-shot is fixed",
			kStallLimitUs);
	} else if (err == -EIO) {
		LOG_ERR("Matrix stall: FAIL, MTX_SAFE_SENSE did not drop at the LAT pulse "
			"(one-shot not triggered)");
	} else {
		LOG_ERR("Matrix stall: error %d", err);
	}
	/* The driver left the matrix blanked; the bench stays on this step. */
	return 0;
}

} // namespace

int run_matrix_bench(const struct device *display)
{
	Frame frame;
	Step step = Step::kBlanked;
	bool stopped = false;
	Button next_button{&btn_next};
	Button stop_button{&btn_stop};
	unsigned tick = 0;
	int err = 0;

	if (gpio_pin_configure_dt(&btn_next, GPIO_INPUT) != 0 ||
	    gpio_pin_configure_dt(&btn_stop, GPIO_INPUT) != 0) {
		return -ENODEV;
	}
	(void)display_blanking_on(display);
	LOG_WRN("Matrix bench: current-limited supply only. MODE = next step, ACTION = "
		"stop/resume");
	LOG_INF("Matrix bench step 0: %s", step_name(step));

	while (true) {
		const bool next = next_button.pressed();

		const bool stop = stop_button.pressed();

		if (stop && step == Step::kStall) {
			err = run_stall(display);
		} else if (stop) {
			stopped = !stopped;
			if (stopped) {
				(void)display_blanking_on(display);
				LOG_WRN("Matrix bench: STOPPED (blanked). ACTION resumes this "
					"step");
			} else if (step != Step::kBlanked) {
				LOG_INF("Matrix bench: resumed");
				err = display_blanking_off(display);
				if (err == 0) {
					err = badge_display_wait_visible(display, 200);
				}
			}
		}
		if (next && stopped) {
			LOG_WRN("Matrix bench: stopped; press ACTION to resume before stepping");
		} else if (next) {
			step = static_cast<Step>((static_cast<int>(step) + 1) %
						 static_cast<int>(Step::kCount));
			tick = 0;
			LOG_INF("Matrix bench step %d: %s", static_cast<int>(step),
				step_name(step));
			if (step == Step::kBlanked) {
				err = display_blanking_on(display);
			} else if (step == Step::kStall) {
				err = run_stall(display);
			} else {
				const uint8_t level = prepare(step, frame);

				err = display_set_brightness(display, level);
				if (err == 0) {
					err = show_retry(display, frame);
				}
				if (err == 0) {
					err = display_blanking_off(display);
				}
				/* Confirm the driver is scanning, not faulted (SAFE, timeout). */
				if (err == 0) {
					err = badge_display_wait_visible(display, 200);
				}
			}
		}
		/* The stall step leaves the matrix blanked: nothing to keep scanning. */
		const bool active =
			!stopped && step != Step::kBlanked && step != Step::kStall && err == 0;

		if (active && (step == Step::kRowWalk || step == Step::kColumnWalk) &&
		    tick % 6 == 0) {
			/* One line every 300 ms. */
			const int line = static_cast<int>((tick / 6) % kMatrixSize);

			frame.clear();
			if (step == Step::kRowWalk) {
				draw_row(frame, line);
			} else {
				draw_column(frame, line);
			}
			err = show_retry(display, frame);
		} else if (active && tick % 20 == 19) {
			/* Static steps: re-check once a second that the driver still scans. */
			err = badge_display_wait_visible(display, 200);
		}
		if (err != 0) {
			(void)display_blanking_on(display);
			LOG_ERR("Matrix bench: driver error %d (matrix blanked); back to step 0",
				err);
			err = 0;
			step = Step::kBlanked;
			stopped = false;
		}
		tick++;
		k_msleep(50);
	}
	return err;
}
#endif /* CONFIG_APP_MATRIX_BENCH */

#else

int matrix_chain_check()
{
	return 0;
}

#endif /* HAVE_REVA_MATRIX */

} // namespace badge
