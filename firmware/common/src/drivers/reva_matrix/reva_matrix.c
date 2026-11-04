/* SPDX-License-Identifier: MIT */
/*
 * Rev A matrix, hardware-driven scan.
 *
 * Each slot runs from TIMER compares through PPI and GPIOTE: SPIM2 shifts the
 * slot's 64 bits (EasyDMA ArrayList walks the pre-encoded slot table), its END
 * raises LAT, a compare lowers LAT, then BLANK goes low for the plane's
 * on-time and high again before the next slot. A counter TIMER stops the slot
 * TIMER by PPI after the 32 rows of a plane; the plane interrupt sets the next
 * plane's on-time and restarts it. If the CPU stops servicing that interrupt,
 * at most one plane runs, LAT pulses stop and the external one-shot blanks the
 * matrix.
 *
 * SPIM2 rather than SPIM3: nRF52840 anomaly 198 corrupts SPIM3 transmit data
 * when the CPU touches the TX buffer's RAM block during EasyDMA fetches, which
 * a free-running scan cannot avoid. The legacy SPIM3 instance (diagnostics
 * chain readback, matrix output off) is disconnected from the shared pins
 * before SPIM2 drives them.
 */
#define DT_DRV_COMPAT smartledbadge_reva_matrix
#include "badge/display_sync.h"
#include "badge/hub75_timing.h"
#include "badge/matrix_scan.h"
#include <errno.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <gpiote_nrfx.h>
#include <hal/nrf_gpiote.h>
#include <hal/nrf_spim.h>
#include <hal/nrf_timer.h>
#include <helpers/nrfx_gppi.h>
#include <nrfx_gpiote.h>

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1);
BUILD_ASSERT(NRF_SPIM_HAS_ARRAY_LIST, "the scan needs SPIM EasyDMA ArrayList");
#define SLOT_TIMER_NODE  DT_INST_PHANDLE(0, timer)
#define PLANE_TIMER_NODE DT_INST_PHANDLE(0, plane_timer)
BUILD_ASSERT(DT_REG_ADDR(DT_INST_PHANDLE(0, scan_spim)) != DT_REG_ADDR(DT_INST_PHANDLE(0, spi)),
	     "the scan SPIM must not be the legacy SPI instance");

/* Slot timeline in 16 MHz TIMER ticks, from the slot start (BLANK high). */
#define SPI_START_TICKS      16U  /* 1 us: after the PPI stop at a plane end lands */
#define LAT_CLEAR_TICKS      192U /* 12 us: 64 bits at 8 MHz end near 9 us; LAT high >= 1 us */
#define LED_ON_TICKS         (LAT_CLEAR_TICKS + 32U) /* 2 us setup after the latch */
#define BLANK_TAIL_TICKS     32U                     /* 2 us blank before the next slot */
#define SLOTS_PER_PLANE      32U
/* No completed plane for this long while scanning is a scan fault. Flash
 * erases stall the CPU for tens of milliseconds, which only delays planes. */
#define SCAN_STALL_MS        500
/* The plane interrupt checks SAFE only when it ran within this margin of the
 * plane's end: well inside the one-shot's 0.76 ms minimum (calculated). Later,
 * after a CPU stall such as a flash erase, SAFE high is the expected blanking. */
#define SAFE_CHECK_MARGIN_US 300U

struct matrix_config {
	struct gpio_dt_spec lat, blank, arm, safe, enable, sck, mosi;
	NRF_TIMER_Type *slot_timer, *plane_timer;
	NRF_SPIM_Type *spim, *legacy_spim;
	nrfx_gpiote_t *gpiote;
};
struct matrix_data {
	struct k_mutex mutex;
	struct k_sem wake; /* leaves the dark park, or reports a scan fault */
	struct k_thread thread;
	uint8_t fb[2][1024];
	/* Pre-encoded slots, plane-major: [plane * 32 + row][8 bytes]. */
	uint8_t slots[2][256][8];
	uint8_t front, brightness, power_cap, plane;
	uint8_t blank_channel, lat_channel;
	enum display_pixel_format format;
	uint32_t plane_started, plane_us; /* cycle count at start, expected length */
	/* Shared with the plane interrupt: change under irq_lock(). */
	bool blanked, pending, stale, rail_on, running;
	int fault;
	uint32_t epoch;
	atomic_t scans, planes;
	nrfx_gppi_handle_t spi_link, lat_set_link, lat_clear_link, on_link, off_link;
	nrfx_gppi_handle_t count_link, stop_link;
};
static K_THREAD_STACK_DEFINE(matrix_stack, 2048);

static void gpiote_task(const struct matrix_config *cfg, uint8_t channel, bool set)
{
	nrf_gpiote_task_trigger(cfg->gpiote->p_reg, set ? nrf_gpiote_set_task_get(channel)
							: nrf_gpiote_clr_task_get(channel));
}
/* Interrupt-safe: stop the slot timer, BLANK high, LAT low. */
static void stop_hw(const struct matrix_config *cfg, struct matrix_data *data)
{
	nrfx_gppi_conn_disable(data->on_link);
	nrf_timer_task_trigger(cfg->slot_timer, NRF_TIMER_TASK_STOP);
	gpiote_task(cfg, data->blank_channel, true);
	gpiote_task(cfg, data->lat_channel, false);
	data->running = false;
}
static void off(const struct matrix_config *cfg, struct matrix_data *data)
{
	unsigned int key = irq_lock();

	stop_hw(cfg, data);
	irq_unlock(key);
	(void)gpio_pin_set_dt(&cfg->arm, 0);
	(void)gpio_pin_set_dt(&cfg->enable, 0);
	data->rail_on = false;
	/* Let an in-flight slot transfer end before disabling the SPIM. */
	nrf_spim_event_clear(cfg->spim, NRF_SPIM_EVENT_STOPPED);
	nrf_spim_task_trigger(cfg->spim, NRF_SPIM_TASK_STOP);
	for (unsigned i = 0; i < 100 && !nrf_spim_event_check(cfg->spim, NRF_SPIM_EVENT_STOPPED);
	     ++i) {
		k_busy_wait(1);
	}
	nrf_spim_disable(cfg->spim);
}
/* Blanked by the application or held off by the power policy. */
static bool dark(const struct matrix_data *data)
{
	return data->blanked || data->power_cap == 0;
}
/* Program the on-time of data->plane. Called with the slot timer stopped. */
static void arm_plane(const struct matrix_config *cfg, struct matrix_data *data)
{
	const uint32_t on =
		badge_gray_on_ticks(data->plane, MIN(data->brightness, data->power_cap));

	nrf_timer_cc_set(cfg->slot_timer, NRF_TIMER_CC_CHANNEL3, LED_ON_TICKS + on);
	nrf_timer_cc_set(cfg->slot_timer, NRF_TIMER_CC_CHANNEL4,
			 LED_ON_TICKS + on + BLANK_TAIL_TICKS);
	if (on != 0) {
		nrfx_gppi_conn_enable(data->on_link);
	} else {
		nrfx_gppi_conn_disable(data->on_link);
	}
	nrf_timer_task_trigger(cfg->slot_timer, NRF_TIMER_TASK_CLEAR);
	data->plane_us = SLOTS_PER_PLANE * (LED_ON_TICKS + on + BLANK_TAIL_TICKS) / 16U;
	data->plane_started = k_cycle_get_32();
}
static void plane_isr(const void *arg)
{
	const struct device *dev = arg;
	const struct matrix_config *cfg = dev->config;
	struct matrix_data *data = dev->data;

	nrf_timer_event_clear(cfg->plane_timer, NRF_TIMER_EVENT_COMPARE0);
	/* PPI has stopped the slot timer after the plane's last slot; BLANK is high. */
	if (!data->running) {
		return;
	}
	/* On time, LAT pulsed within the last slot, so the one-shot must still
	 * hold SAFE low; a high SAFE then means a broken safety path. */
	const uint32_t elapsed_us = k_cyc_to_us_floor32(k_cycle_get_32() - data->plane_started);
	const bool on_time = elapsed_us <= data->plane_us + SAFE_CHECK_MARGIN_US;

	if (on_time && gpio_pin_get_dt(&cfg->safe) != 0) {
		stop_hw(cfg, data);
		data->fault = -EIO;
		data->blanked = true;
		k_sem_give(&data->wake);
		return;
	}
	atomic_inc(&data->planes);
	if (++data->plane == 8) {
		data->plane = 0;
		atomic_inc(&data->scans);
		if (data->pending) {
			data->front ^= 1;
			data->pending = false;
			data->stale = true;
		}
		nrf_spim_tx_buffer_set(cfg->spim, data->slots[data->front][0], 8);
	}
	arm_plane(cfg, data);
	nrf_timer_task_trigger(cfg->slot_timer, NRF_TIMER_TASK_START);
}
static void configure_spim(const struct matrix_config *cfg)
{
	/* Only one peripheral may drive a pin: release the legacy SPIM first. */
	nrf_spim_disable(cfg->legacy_spim);
	nrf_spim_pins_set(cfg->legacy_spim, NRF_SPIM_PIN_NOT_CONNECTED, NRF_SPIM_PIN_NOT_CONNECTED,
			  NRF_SPIM_PIN_NOT_CONNECTED);
	nrf_spim_pins_set(cfg->spim, NRF_DT_GPIOS_TO_PSEL(DT_DRV_INST(0), sck_gpios),
			  NRF_DT_GPIOS_TO_PSEL(DT_DRV_INST(0), mosi_gpios),
			  NRF_SPIM_PIN_NOT_CONNECTED);
	nrf_spim_frequency_set(cfg->spim, NRF_SPIM_FREQ_8M);
	nrf_spim_configure(cfg->spim, NRF_SPIM_MODE_0, NRF_SPIM_BIT_ORDER_MSB_FIRST);
	nrf_spim_orc_set(cfg->spim, 0xFF);
	nrf_spim_rx_buffer_set(cfg->spim, NULL, 0);
	nrf_spim_tx_list_enable(cfg->spim);
	nrf_spim_enable(cfg->spim);
}
static void start_scan(const struct matrix_config *cfg, struct matrix_data *data)
{
	configure_spim(cfg);

	unsigned int key = irq_lock();

	gpiote_task(cfg, data->lat_channel, false);
	gpiote_task(cfg, data->blank_channel, true);
	nrf_timer_task_trigger(cfg->slot_timer, NRF_TIMER_TASK_STOP);
	nrf_timer_task_trigger(cfg->plane_timer, NRF_TIMER_TASK_CLEAR);
	nrf_timer_event_clear(cfg->plane_timer, NRF_TIMER_EVENT_COMPARE0);
	if (data->pending) {
		data->front ^= 1;
		data->pending = false;
		data->stale = true;
	}
	data->plane = 0;
	nrf_spim_tx_buffer_set(cfg->spim, data->slots[data->front][0], 8);
	(void)gpio_pin_set_dt(&cfg->arm, 1);
	data->running = true;
	arm_plane(cfg, data);
	nrf_timer_task_trigger(cfg->slot_timer, NRF_TIMER_TASK_START);
	irq_unlock(key);
}
static void worker(void *pointer, void *unused1, void *unused2)
{
	ARG_UNUSED(unused1);
	ARG_UNUSED(unused2);
	const struct device *dev = pointer;
	const struct matrix_config *cfg = dev->config;
	struct matrix_data *data = dev->data;
	bool powered = false;
	uint32_t epoch = 0;
	atomic_val_t last_planes = 0;
	int64_t last_progress = 0;
	while (true) {
		k_mutex_lock(&data->mutex, K_FOREVER);
		/* off() from another path (power cap 0) clears rail_on without
		 * waking this thread: re-enable and settle the rail first. */
		if (epoch != data->epoch || !data->rail_on) {
			epoch = data->epoch;
			powered = false;
		}
		if (dark(data)) {
			off(cfg, data);
			powered = false;
			k_mutex_unlock(&data->mutex);
			/* Parked while dark (blanked, shed, charging-only, faulted): woken at
			 * once when that changes, otherwise once a second. */
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
		if (!data->running) {
			start_scan(cfg, data);
			last_planes = atomic_get(&data->planes);
			last_progress = k_uptime_get();
		} else {
			const atomic_val_t planes = atomic_get(&data->planes);
			if (planes != last_planes) {
				last_planes = planes;
				last_progress = k_uptime_get();
			} else if (k_uptime_get() - last_progress > SCAN_STALL_MS) {
				data->fault = -ETIMEDOUT;
				data->blanked = true;
				off(cfg, data);
			}
		}
		k_mutex_unlock(&data->mutex);
		(void)k_sem_take(&data->wake, K_MSEC(100));
	}
}
static int blanking_on(const struct device *dev)
{
	const struct matrix_config *cfg = dev->config;
	struct matrix_data *data = dev->data;
	k_mutex_lock(&data->mutex, K_FOREVER);
	off(cfg, data);
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
	k_sem_give(&data->wake);
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
static void encode_frame(uint8_t slots[256][8], const uint8_t levels[1024])
{
	for (unsigned plane = 0; plane < 8; ++plane) {
		for (unsigned row = 0; row < SLOTS_PER_PLANE; ++row) {
			(void)badge_matrix_encode_row(slots[plane * SLOTS_PER_PLANE + row],
						      levels + row * 32, row, plane);
		}
	}
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
	unsigned int key = irq_lock();
	const bool busy = data->pending;
	const uint8_t front = data->front;

	irq_unlock(key);
	if (busy) {
		k_mutex_unlock(&data->mutex);
		return -EBUSY;
	}
	/* The plane interrupt reads only the front buffers; the back ones are ours. */
	const uint8_t back = front ^ 1;
	if (data->stale) {
		memcpy(data->fb[back], data->fb[front], 1024);
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
		encode_frame(data->slots[back], data->fb[back]);
		key = irq_lock();
		if (data->running) {
			data->pending = true;
		} else {
			data->front = back;
			data->stale = true;
		}
		irq_unlock(key);
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
		unsigned int key = irq_lock();
		bool pending = data->pending, blanked = data->blanked;
		bool held = data->power_cap == 0;
		int fault = data->fault;

		irq_unlock(key);
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
	/* The plane interrupt applies a new ceiling from the next plane. */
	data->power_cap = ceiling;
	if (ceiling == 0) {
		/* As in blanking_on: a swap cannot complete while scanning is stopped. */
		off(dev->config, data);
		if (data->pending) {
			data->front ^= 1;
			data->pending = false;
			data->stale = true;
		}
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
#if defined(CONFIG_REVA_MATRIX_STALL_TEST)
int badge_display_stall_test(const struct device *dev, unsigned limit_us, unsigned *safe_us)
{
	const struct matrix_config *cfg = dev->config;
	struct matrix_data *data = dev->data;
	/* The slot TIMER runs at 16 MHz (prescaler 0). */
	const uint32_t limit = limit_us * 16U;
	k_mutex_lock(&data->mutex, K_FOREVER);
	/* Blanked: the scan is stopped and the worker parked. */
	if (!data->blanked || data->power_cap == 0) {
		k_mutex_unlock(&data->mutex);
		return -EBUSY;
	}
	int rc = gpio_pin_set_dt(&cfg->enable, 1);
	if (rc == 0) {
		data->rail_on = true;
		k_sleep(K_MSEC(5)); /* VLED settles */
		/* One row, column 0 at full on, shifted without the slot sequencer. */
		static uint8_t row[8];
		uint8_t levels[32] = {255};
		(void)badge_matrix_encode_row(row, levels, 0, 7);
		nrfx_gppi_conn_disable(data->lat_set_link);
		configure_spim(cfg);
		nrf_spim_tx_buffer_set(cfg->spim, row, sizeof(row));
		nrf_spim_event_clear(cfg->spim, NRF_SPIM_EVENT_END);
		nrf_spim_task_trigger(cfg->spim, NRF_SPIM_TASK_START);
		for (unsigned i = 0;
		     i < 100 && !nrf_spim_event_check(cfg->spim, NRF_SPIM_EVENT_END); ++i) {
			k_busy_wait(1);
		}
		nrfx_gppi_conn_enable(data->lat_set_link);
	}
	if (rc == 0) {
		unsigned int key = irq_lock();
		/* Only COMPARE3 -> BLANK high stays linked: the hard limit, even if
		 * the CPU stopped here. COMPARE4 then stops the timer. */
		nrfx_gppi_conn_disable(data->spi_link);
		nrfx_gppi_conn_disable(data->lat_clear_link);
		nrfx_gppi_conn_disable(data->count_link);
		nrf_timer_shorts_set(cfg->slot_timer, NRF_TIMER_SHORT_COMPARE4_STOP_MASK);
		nrf_timer_task_trigger(cfg->slot_timer, NRF_TIMER_TASK_STOP);
		nrf_timer_task_trigger(cfg->slot_timer, NRF_TIMER_TASK_CLEAR);
		nrf_timer_cc_set(cfg->slot_timer, NRF_TIMER_CC_CHANNEL3, limit);
		nrf_timer_cc_set(cfg->slot_timer, NRF_TIMER_CC_CHANNEL4, limit + 16U);
		nrf_timer_event_clear(cfg->slot_timer, NRF_TIMER_EVENT_COMPARE3);
		(void)gpio_pin_set_dt(&cfg->arm, 1);
		gpiote_task(cfg, data->lat_channel, true);
		k_busy_wait(1);
		gpiote_task(cfg, data->lat_channel, false);
		/* The pulse triggers the one-shot: SAFE must now read low. */
		if (gpio_pin_get_dt(&cfg->safe) != 0) {
			rc = -EIO;
		} else {
			/* Timed from here, a few microseconds after the pulse. */
			nrf_timer_task_trigger(cfg->slot_timer, NRF_TIMER_TASK_START);
			gpiote_task(cfg, data->blank_channel, false);
			while (gpio_pin_get_dt(&cfg->safe) == 0 &&
			       !nrf_timer_event_check(cfg->slot_timer, NRF_TIMER_EVENT_COMPARE3)) {
			}
			nrf_timer_task_trigger(cfg->slot_timer, NRF_TIMER_TASK_CAPTURE5);
			const uint32_t ticks =
				nrf_timer_cc_get(cfg->slot_timer, NRF_TIMER_CC_CHANNEL5);
			if (gpio_pin_get_dt(&cfg->safe) == 0) {
				rc = -ETIMEDOUT;
			}
			*safe_us = MIN(ticks, limit) / 16U;
		}
		nrf_timer_shorts_set(cfg->slot_timer, NRF_TIMER_SHORT_COMPARE4_CLEAR_MASK);
		nrf_timer_cc_set(cfg->slot_timer, NRF_TIMER_CC_CHANNEL0, SPI_START_TICKS);
		nrfx_gppi_conn_enable(data->spi_link);
		nrfx_gppi_conn_enable(data->lat_clear_link);
		nrfx_gppi_conn_enable(data->count_link);
		irq_unlock(key);
	}
	/* BLANK high, ARM low, LED rail off: as left by blanking_on(). */
	off(cfg, data);
	k_mutex_unlock(&data->mutex);
	return rc;
}
#endif
static DEVICE_API(display, matrix_api) = {.blanking_on = blanking_on,
					  .blanking_off = blanking_off,
					  .write = write_frame,
					  .set_brightness = brightness,
					  .set_pixel_format = pixel_format,
					  .get_capabilities = capabilities};
static int link(uint32_t event, uint32_t task, nrfx_gppi_handle_t *handle, bool enable)
{
	int rc = nrfx_gppi_conn_alloc(event, task, handle);
	if (rc == 0 && enable) {
		nrfx_gppi_conn_enable(*handle);
	}
	return rc;
}
static int gpiote_output(const struct matrix_config *cfg, uint8_t *channel, uint32_t psel,
			 nrf_gpiote_outinit_t initial)
{
	int rc = nrfx_gpiote_channel_alloc(cfg->gpiote, channel);
	if (rc != 0) {
		return rc;
	}
	nrf_gpiote_task_configure(cfg->gpiote->p_reg, *channel, psel, NRF_GPIOTE_POLARITY_NONE,
				  initial);
	nrf_gpiote_task_enable(cfg->gpiote->p_reg, *channel);
	return 0;
}
static int initialize(const struct device *dev)
{
	const struct matrix_config *cfg = dev->config;
	struct matrix_data *data = dev->data;
	k_mutex_init(&data->mutex);
	k_sem_init(&data->wake, 0, 1);
	data->blanked = true;
	data->stale = true;
	data->brightness = 128;
	data->power_cap = IS_ENABLED(CONFIG_REVA_MATRIX_POWER_CAP) ? 0 : 255;
	data->format = PIXEL_FORMAT_MONO01;
	encode_frame(data->slots[0], data->fb[0]);
	encode_frame(data->slots[1], data->fb[1]);
	const struct gpio_dt_spec outputs[] = {cfg->blank, cfg->arm, cfg->enable,
					       cfg->lat,   cfg->sck, cfg->mosi};
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
	if (rc == 0) {
		rc = gpiote_output(cfg, &data->blank_channel,
				   NRF_DT_GPIOS_TO_PSEL(DT_DRV_INST(0), blank_gpios),
				   NRF_GPIOTE_INITIAL_VALUE_HIGH);
	}
	if (rc == 0) {
		rc = gpiote_output(cfg, &data->lat_channel,
				   NRF_DT_GPIOS_TO_PSEL(DT_DRV_INST(0), lat_gpios),
				   NRF_GPIOTE_INITIAL_VALUE_LOW);
	}
	if (rc != 0) {
		return rc;
	}
	/* Slot timer: compares 0-4 sequence one slot, COMPARE4 restarts it. */
	nrf_timer_mode_set(cfg->slot_timer, NRF_TIMER_MODE_TIMER);
	nrf_timer_bit_width_set(cfg->slot_timer, NRF_TIMER_BIT_WIDTH_32);
	nrf_timer_prescaler_set(cfg->slot_timer, 0);
	nrf_timer_shorts_set(cfg->slot_timer, NRF_TIMER_SHORT_COMPARE4_CLEAR_MASK);
	nrf_timer_cc_set(cfg->slot_timer, NRF_TIMER_CC_CHANNEL0, SPI_START_TICKS);
	nrf_timer_cc_set(cfg->slot_timer, NRF_TIMER_CC_CHANNEL1, LAT_CLEAR_TICKS);
	nrf_timer_cc_set(cfg->slot_timer, NRF_TIMER_CC_CHANNEL2, LED_ON_TICKS);
	/* Plane counter: COMPARE0 after 32 slots stops the slot timer. */
	nrf_timer_mode_set(cfg->plane_timer, NRF_TIMER_MODE_COUNTER);
	nrf_timer_bit_width_set(cfg->plane_timer, NRF_TIMER_BIT_WIDTH_16);
	nrf_timer_shorts_set(cfg->plane_timer, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK);
	nrf_timer_cc_set(cfg->plane_timer, NRF_TIMER_CC_CHANNEL0, SLOTS_PER_PLANE);
	nrf_timer_task_trigger(cfg->plane_timer, NRF_TIMER_TASK_START);

	NRF_GPIOTE_Type *gpiote = cfg->gpiote->p_reg;
	rc = link(nrf_timer_event_address_get(cfg->slot_timer, NRF_TIMER_EVENT_COMPARE0),
		  nrf_spim_task_address_get(cfg->spim, NRF_SPIM_TASK_START), &data->spi_link, true);
	if (rc == 0) {
		rc = link(nrf_spim_event_address_get(cfg->spim, NRF_SPIM_EVENT_END),
			  nrf_gpiote_task_address_get(gpiote,
						      nrf_gpiote_set_task_get(data->lat_channel)),
			  &data->lat_set_link, true);
	}
	if (rc == 0) {
		rc = link(nrf_timer_event_address_get(cfg->slot_timer, NRF_TIMER_EVENT_COMPARE1),
			  nrf_gpiote_task_address_get(gpiote,
						      nrf_gpiote_clr_task_get(data->lat_channel)),
			  &data->lat_clear_link, true);
	}
	if (rc == 0) {
		rc = link(nrf_timer_event_address_get(cfg->slot_timer, NRF_TIMER_EVENT_COMPARE2),
			  nrf_gpiote_task_address_get(gpiote,
						      nrf_gpiote_clr_task_get(data->blank_channel)),
			  &data->on_link, false);
	}
	if (rc == 0) {
		rc = link(nrf_timer_event_address_get(cfg->slot_timer, NRF_TIMER_EVENT_COMPARE3),
			  nrf_gpiote_task_address_get(gpiote,
						      nrf_gpiote_set_task_get(data->blank_channel)),
			  &data->off_link, true);
	}
	if (rc == 0) {
		rc = link(nrf_timer_event_address_get(cfg->slot_timer, NRF_TIMER_EVENT_COMPARE4),
			  nrf_timer_task_address_get(cfg->plane_timer, NRF_TIMER_TASK_COUNT),
			  &data->count_link, true);
	}
	if (rc == 0) {
		rc = link(nrf_timer_event_address_get(cfg->plane_timer, NRF_TIMER_EVENT_COMPARE0),
			  nrf_timer_task_address_get(cfg->slot_timer, NRF_TIMER_TASK_STOP),
			  &data->stop_link, true);
	}
	if (rc != 0) {
		return rc;
	}
	IRQ_CONNECT(DT_IRQN(PLANE_TIMER_NODE), DT_IRQ(PLANE_TIMER_NODE, priority), plane_isr,
		    DEVICE_DT_INST_GET(0), 0);
	nrf_timer_int_enable(cfg->plane_timer, NRF_TIMER_INT_COMPARE0_MASK);
	irq_enable(DT_IRQN(PLANE_TIMER_NODE));
	k_thread_create(&data->thread, matrix_stack, K_THREAD_STACK_SIZEOF(matrix_stack), worker,
			(void *)dev, NULL, NULL, CONFIG_REVA_MATRIX_THREAD_PRIORITY, 0, K_NO_WAIT);
	return 0;
}
static struct matrix_data matrix_data;
static const struct matrix_config matrix_config = {
	.lat = GPIO_DT_SPEC_INST_GET(0, lat_gpios),
	.blank = GPIO_DT_SPEC_INST_GET(0, blank_gpios),
	.arm = GPIO_DT_SPEC_INST_GET(0, arm_gpios),
	.safe = GPIO_DT_SPEC_INST_GET(0, safe_gpios),
	.enable = GPIO_DT_SPEC_INST_GET(0, enable_gpios),
	.sck = GPIO_DT_SPEC_INST_GET(0, sck_gpios),
	.mosi = GPIO_DT_SPEC_INST_GET(0, mosi_gpios),
	.slot_timer = (NRF_TIMER_Type *)DT_REG_ADDR(SLOT_TIMER_NODE),
	.plane_timer = (NRF_TIMER_Type *)DT_REG_ADDR(PLANE_TIMER_NODE),
	.spim = (NRF_SPIM_Type *)DT_REG_ADDR(DT_INST_PHANDLE(0, scan_spim)),
	.legacy_spim = (NRF_SPIM_Type *)DT_REG_ADDR(DT_INST_PHANDLE(0, spi)),
	.gpiote = &GPIOTE_NRFX_INST_BY_NODE(NRF_DT_GPIOTE_NODE(DT_DRV_INST(0), blank_gpios)),
};
DEVICE_DT_INST_DEFINE(0, initialize, NULL, &matrix_data, &matrix_config, POST_KERNEL,
		      CONFIG_DISPLAY_INIT_PRIORITY, &matrix_api);
