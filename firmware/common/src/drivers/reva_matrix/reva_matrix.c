/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT smartledbadge_reva_matrix
#include "badge/display_sync.h"
#include "badge/hub75_timing.h"
#include "badge/matrix_scan.h"
#include <errno.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <gpiote_nrfx.h>
#include <hal/nrf_gpiote.h>
#include <hal/nrf_timer.h>
#include <helpers/nrfx_gppi.h>
#include <nrfx_gpiote.h>

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1);
#define TIMER_NODE DT_INST_PHANDLE(0, timer)
struct matrix_config {
	const struct device *spi;
	struct gpio_dt_spec lat, blank, arm, safe, enable;
	NRF_TIMER_Type *timer;
	nrfx_gpiote_t *gpiote;
};
struct matrix_data {
	struct k_mutex mutex;
	struct k_sem pulse;
	struct k_sem wake; /* leaves the dark park: unblank or cap raised */
	struct k_thread thread;
	uint8_t fb[2][1024];
	uint8_t front, brightness, blank_channel, power_cap;
	enum display_pixel_format format;
	bool blanked, pending, stale, rail_on;
	int fault;
	uint32_t epoch;
	atomic_t scans;
	nrfx_gppi_handle_t on_link, off_link;
};
static K_THREAD_STACK_DEFINE(matrix_stack, 2048);
static const struct spi_config spi_config = {
	.frequency = 8000000,
	.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
};

static void blank(const struct matrix_config *cfg, struct matrix_data *data)
{
	unsigned key = irq_lock();
	nrfx_gppi_conn_disable(data->on_link);
	nrf_timer_task_trigger(cfg->timer, NRF_TIMER_TASK_STOP);
	nrf_timer_event_clear(cfg->timer, NRF_TIMER_EVENT_COMPARE0);
	NVIC_ClearPendingIRQ((IRQn_Type)DT_IRQN(TIMER_NODE));
	nrf_gpiote_task_trigger(cfg->gpiote->p_reg, nrf_gpiote_set_task_get(data->blank_channel));
	irq_unlock(key);
}
static void off(const struct matrix_config *cfg, struct matrix_data *data)
{
	blank(cfg, data);
	(void)gpio_pin_set_dt(&cfg->arm, 0);
	(void)gpio_pin_set_dt(&cfg->enable, 0);
	data->rail_on = false;
}
/* Blanked by the application or held off by the power policy. */
static bool dark(const struct matrix_data *data)
{
	return data->blanked || data->power_cap == 0;
}
static void slot_isr(const void *arg)
{
	const struct device *dev = arg;
	const struct matrix_config *cfg = dev->config;
	struct matrix_data *data = dev->data;
	nrf_timer_event_clear(cfg->timer, NRF_TIMER_EVENT_COMPARE0);
	/* PPI already asserted BLANK; the one-shot timer is stopped. */
	k_sem_give(&data->pulse);
}
static void worker(void *pointer, void *unused1, void *unused2)
{
	ARG_UNUSED(unused1);
	ARG_UNUSED(unused2);
	const struct device *dev = pointer;
	const struct matrix_config *cfg = dev->config;
	struct matrix_data *data = dev->data;
	unsigned row = 0, plane = 0;
	bool powered = false;
	uint32_t epoch = 0;
	while (true) {
		uint8_t bytes[8];
		k_mutex_lock(&data->mutex, K_FOREVER);
		if (epoch != data->epoch) {
			epoch = data->epoch;
			powered = false;
			row = plane = 0;
		}
		if (dark(data)) {
			off(cfg, data);
			powered = false;
			row = plane = 0;
			k_mutex_unlock(&data->mutex);
			/* Parked while dark (blanked, shed, charging-only): woken at once
			 * when that changes, otherwise once a second. */
			(void)k_sem_take(&data->wake, K_SECONDS(1));
			continue;
		}
		if (!powered) {
			/* ARM remains low and BLANK high while VLED settles. */
			int rc = gpio_pin_set_dt(&cfg->enable, 1);
			if (rc != 0) {
				data->fault = rc;
				data->blanked = true;
			}
			data->rail_on = rc == 0;
			k_mutex_unlock(&data->mutex);
			k_sleep(K_MSEC(5));
			powered = rc == 0;
			continue;
		}
		if (row == 0 && plane == 0 && data->pending) {
			data->front ^= 1;
			data->pending = false;
			data->stale = true;
		}
		(void)badge_matrix_encode_row(bytes, data->fb[data->front] + row * 32, row, plane);
		k_mutex_unlock(&data->mutex);
		struct spi_buf buffer = {.buf = bytes, .len = sizeof(bytes)};
		const struct spi_buf_set tx = {.buffers = &buffer, .count = 1};
		/* Thread context only; the SDK driver owns EasyDMA, locks and errata. */
		int rc = spi_write(cfg->spi, &spi_config, &tx);
		k_mutex_lock(&data->mutex, K_FOREVER);
		/* A blank/recovery cycle may have happened while SPI owned the bus.
		 * Discard that old row rather than latching it into the new session. */
		if (epoch != data->epoch) {
			k_mutex_unlock(&data->mutex);
			continue;
		}
		if (rc == 0 && !dark(data)) {
			rc = gpio_pin_set_dt(&cfg->arm, 1);
			if (rc == 0) {
				rc = gpio_pin_set_dt(&cfg->lat, 1);
			}
			k_busy_wait(1);
			if (rc == 0) {
				rc = gpio_pin_set_dt(&cfg->lat, 0);
			}
			/* Initial pulse retriggers the external one-shot before light. */
			int safe = gpio_pin_get_dt(&cfg->safe);
			if (rc == 0 && safe != 0) {
				rc = safe < 0 ? safe : -EIO;
			}
			if (rc == 0) {
				uint32_t on = badge_gray_on_ticks(
					plane, MIN(data->brightness, data->power_cap));
				/* Reserve 2us setup and 2us blank after the pulse. */
				uint32_t period = badge_reva_slot_ticks(on);
				blank(cfg, data);
				k_sem_reset(&data->pulse);
				nrf_timer_task_trigger(cfg->timer, NRF_TIMER_TASK_CLEAR);
				nrf_timer_cc_set(cfg->timer, NRF_TIMER_CC_CHANNEL0, period);
				nrf_timer_cc_set(cfg->timer, NRF_TIMER_CC_CHANNEL1, 32U + on);
				nrf_timer_cc_set(cfg->timer, NRF_TIMER_CC_CHANNEL2, 32U);
				nrf_timer_event_clear(cfg->timer, NRF_TIMER_EVENT_COMPARE0);
				nrf_timer_event_clear(cfg->timer, NRF_TIMER_EVENT_COMPARE1);
				nrf_timer_event_clear(cfg->timer, NRF_TIMER_EVENT_COMPARE2);
				if (on) {
					nrfx_gppi_conn_enable(data->on_link);
				}
				nrf_timer_task_trigger(cfg->timer, NRF_TIMER_TASK_START);
			}
		}
		if (rc != 0) {
			data->fault = rc;
			data->blanked = true;
			off(cfg, data);
		}
		bool active = !dark(data);
		k_mutex_unlock(&data->mutex);
		if (!active) {
			continue;
		}
		rc = k_sem_take(&data->pulse, K_MSEC(20));
		if (rc != 0) {
			k_mutex_lock(&data->mutex, K_FOREVER);
			data->fault = -ETIMEDOUT;
			data->blanked = true;
			off(cfg, data);
			k_mutex_unlock(&data->mutex);
			continue;
		}
		if (++row == 32) {
			row = 0;
			if (++plane == 8) {
				plane = 0;
				atomic_inc(&data->scans);
			}
		}
	}
}
static int blanking_on(const struct device *dev)
{
	const struct matrix_config *cfg = dev->config;
	struct matrix_data *data = dev->data;
	k_mutex_lock(&data->mutex, K_FOREVER);
	data->blanked = true;
	/* A queued swap cannot complete while scanning is stopped. Preserve the
	 * submitted frame and release the pending slot before recovery writes. */
	if (data->pending) {
		data->front ^= 1;
		data->pending = false;
	}
	data->stale = true;
	data->fault = 0;
	data->epoch++;
	off(cfg, data);
	k_sem_give(&data->pulse);
	k_mutex_unlock(&data->mutex);
	return 0;
}
static int blanking_off(const struct device *dev)
{
	if (!IS_ENABLED(CONFIG_REVA_MATRIX_OUTPUT)) {
		return -EPERM;
	}
	struct matrix_data *data = dev->data;
	k_mutex_lock(&data->mutex, K_FOREVER);
	data->fault = 0;
	data->blanked = false;
	k_mutex_unlock(&data->mutex);
	k_sem_give(&data->wake);
	return 0;
}
static int write_frame(const struct device *dev, uint16_t x, uint16_t y,
		       const struct display_buffer_descriptor *desc, const void *source)
{
	struct matrix_data *data = dev->data;
	if (!source || !desc || desc->width == 0 || desc->height == 0 || x >= 32 || y >= 32 ||
	    desc->width > 32 - x || desc->height > 32 - y || desc->pitch < desc->width) {
		return -EINVAL;
	}
	k_mutex_lock(&data->mutex, K_FOREVER);
	const bool gray = data->format == PIXEL_FORMAT_L_8;
	size_t pixels = (size_t)desc->pitch * (desc->height - 1) + desc->width;
	if (desc->buf_size < (gray ? pixels : DIV_ROUND_UP(pixels, 8))) {
		k_mutex_unlock(&data->mutex);
		return -EINVAL;
	}
	if (data->pending) {
		k_mutex_unlock(&data->mutex);
		return -EBUSY;
	}
	uint8_t back = data->front ^ 1;
	if (data->stale) {
		memcpy(data->fb[back], data->fb[data->front], 1024);
	}
	const uint8_t *input = source;
	for (unsigned yy = 0; yy < desc->height; ++yy) {
		for (unsigned xx = 0; xx < desc->width; ++xx) {
			size_t at = (size_t)yy * desc->pitch + xx;
			data->fb[back][(y + yy) * 32 + x + xx] =
				gray ? input[at] : (input[at / 8] & BIT(7 - at % 8) ? 255 : 0);
		}
	}
	data->stale = false;
	if (!desc->frame_incomplete) {
		if (dark(data)) {
			data->front = back;
			data->stale = true;
		} else {
			data->pending = true;
		}
	}
	int rc = data->fault;
	k_mutex_unlock(&data->mutex);
	return rc;
}
static int brightness(const struct device *dev, uint8_t level)
{
	struct matrix_data *data = dev->data;
	k_mutex_lock(&data->mutex, K_FOREVER);
	data->brightness = level;
	k_mutex_unlock(&data->mutex);
	return 0;
}
static int pixel_format(const struct device *dev, enum display_pixel_format format)
{
	if (format != PIXEL_FORMAT_MONO01 && format != PIXEL_FORMAT_L_8) {
		return -ENOTSUP;
	}
	struct matrix_data *data = dev->data;
	k_mutex_lock(&data->mutex, K_FOREVER);
	data->format = format;
	k_mutex_unlock(&data->mutex);
	return 0;
}
static void capabilities(const struct device *dev, struct display_capabilities *caps)
{
	struct matrix_data *data = dev->data;
	memset(caps, 0, sizeof(*caps));
	caps->x_resolution = caps->y_resolution = 32;
	caps->supported_pixel_formats = PIXEL_FORMAT_MONO01 | PIXEL_FORMAT_L_8;
	caps->screen_info = SCREEN_INFO_MONO_MSB_FIRST;
	k_mutex_lock(&data->mutex, K_FOREVER);
	caps->current_pixel_format = data->format;
	k_mutex_unlock(&data->mutex);
}
int badge_display_wait_visible(const struct device *dev, unsigned timeout_ms)
{
	struct matrix_data *data = dev->data;
	const int64_t deadline = k_uptime_get() + timeout_ms;
	atomic_val_t previous = atomic_get(&data->scans);
	while (k_uptime_get() < deadline) {
		k_mutex_lock(&data->mutex, K_FOREVER);
		bool pending = data->pending, blanked = data->blanked;
		bool held = data->power_cap == 0;
		int fault = data->fault;
		k_mutex_unlock(&data->mutex);
		if (fault) {
			return fault;
		}
		if (blanked) {
			return -EIO;
		}
		if (held) {
			return -EAGAIN;
		}
		/* Two completed scans guarantee a full scan after any queued swap. */
		if (!pending && ((uint32_t)atomic_get(&data->scans) - (uint32_t)previous) >= 2) {
			return 0;
		}
		k_msleep(1);
	}
	return -ETIMEDOUT;
}
void badge_display_set_power_cap(const struct device *dev, uint8_t ceiling)
{
	struct matrix_data *data = dev->data;
	k_mutex_lock(&data->mutex, K_FOREVER);
	data->power_cap = ceiling;
	/* As in blanking_on: a swap cannot complete while scanning is stopped. */
	if (ceiling == 0 && data->pending) {
		data->front ^= 1;
		data->pending = false;
		data->stale = true;
	}
	k_mutex_unlock(&data->mutex);
	if (ceiling != 0) {
		k_sem_give(&data->wake);
	}
}
bool badge_display_rail_on(const struct device *dev)
{
	struct matrix_data *data = dev->data;
	k_mutex_lock(&data->mutex, K_FOREVER);
	bool on = data->rail_on;
	k_mutex_unlock(&data->mutex);
	return on;
}
static DEVICE_API(display, matrix_api) = {.blanking_on = blanking_on,
					  .blanking_off = blanking_off,
					  .write = write_frame,
					  .set_brightness = brightness,
					  .set_pixel_format = pixel_format,
					  .get_capabilities = capabilities};
static int initialize(const struct device *dev)
{
	const struct matrix_config *cfg = dev->config;
	struct matrix_data *data = dev->data;
	k_mutex_init(&data->mutex);
	k_sem_init(&data->pulse, 0, 1);
	k_sem_init(&data->wake, 0, 1);
	data->blanked = true;
	data->stale = true;
	data->brightness = 128;
	data->power_cap = IS_ENABLED(CONFIG_REVA_MATRIX_POWER_CAP) ? 0 : 255;
	data->format = PIXEL_FORMAT_MONO01;
	if (!device_is_ready(cfg->spi)) {
		return -ENODEV;
	}
	const struct gpio_dt_spec outputs[] = {cfg->blank, cfg->arm, cfg->enable, cfg->lat};
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
	int rc = gpio_pin_configure_dt(&cfg->safe, GPIO_INPUT);
	if (rc != 0) {
		return rc;
	}
	rc = nrfx_gpiote_channel_alloc(cfg->gpiote, &data->blank_channel);
	if (rc != 0) {
		return rc;
	}
	nrf_gpiote_task_configure(cfg->gpiote->p_reg, data->blank_channel,
				  NRF_DT_GPIOS_TO_PSEL(DT_DRV_INST(0), blank_gpios),
				  NRF_GPIOTE_POLARITY_NONE, NRF_GPIOTE_INITIAL_VALUE_HIGH);
	nrf_gpiote_task_enable(cfg->gpiote->p_reg, data->blank_channel);
	nrf_timer_mode_set(cfg->timer, NRF_TIMER_MODE_TIMER);
	nrf_timer_bit_width_set(cfg->timer, NRF_TIMER_BIT_WIDTH_32);
	nrf_timer_prescaler_set(cfg->timer, 0);
	nrf_timer_shorts_enable(cfg->timer, NRF_TIMER_SHORT_COMPARE0_STOP_MASK);
	rc = nrfx_gppi_conn_alloc(
		nrf_timer_event_address_get(cfg->timer, NRF_TIMER_EVENT_COMPARE1),
		nrf_gpiote_task_address_get(cfg->gpiote->p_reg,
					    nrf_gpiote_set_task_get(data->blank_channel)),
		&data->off_link);
	if (rc != 0) {
		return rc;
	}
	nrfx_gppi_conn_enable(data->off_link);
	rc = nrfx_gppi_conn_alloc(
		nrf_timer_event_address_get(cfg->timer, NRF_TIMER_EVENT_COMPARE2),
		nrf_gpiote_task_address_get(cfg->gpiote->p_reg,
					    nrf_gpiote_clr_task_get(data->blank_channel)),
		&data->on_link);
	if (rc != 0) {
		return rc;
	}
	IRQ_CONNECT(DT_IRQN(TIMER_NODE), DT_IRQ(TIMER_NODE, priority), slot_isr,
		    DEVICE_DT_INST_GET(0), 0);
	nrf_timer_int_enable(cfg->timer, NRF_TIMER_INT_COMPARE0_MASK);
	irq_enable(DT_IRQN(TIMER_NODE));
	k_thread_create(&data->thread, matrix_stack, K_THREAD_STACK_SIZEOF(matrix_stack), worker,
			(void *)dev, NULL, NULL, 5, 0, K_NO_WAIT);
	return 0;
}
static struct matrix_data matrix_data;
static const struct matrix_config matrix_config = {
	.spi = DEVICE_DT_GET(DT_INST_PHANDLE(0, spi)),
	.lat = GPIO_DT_SPEC_INST_GET(0, lat_gpios),
	.blank = GPIO_DT_SPEC_INST_GET(0, blank_gpios),
	.arm = GPIO_DT_SPEC_INST_GET(0, arm_gpios),
	.safe = GPIO_DT_SPEC_INST_GET(0, safe_gpios),
	.enable = GPIO_DT_SPEC_INST_GET(0, enable_gpios),
	.timer = (NRF_TIMER_Type *)DT_REG_ADDR(TIMER_NODE),
	.gpiote = &GPIOTE_NRFX_INST_BY_NODE(NRF_DT_GPIOTE_NODE(DT_DRV_INST(0), blank_gpios)),
};
DEVICE_DT_INST_DEFINE(0, initialize, NULL, &matrix_data, &matrix_config, POST_KERNEL,
		      CONFIG_DISPLAY_INIT_PRIORITY, &matrix_api);
