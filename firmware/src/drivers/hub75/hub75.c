/*
 * SPDX-License-Identifier: MIT
 *
 * HUB75 panel as a Zephyr display (bench display on the nRF5340 DK).
 *
 * Monochrome window (PIXEL_FORMAT_MONO01, rows packed MSB first) in the top
 * half of the panel. A TIMER compare at the end of each row slot interrupts;
 * the handler switches the row off, sets the address, latches the row shifted
 * during the previous slot, switches it on, then shifts the next row while
 * this one is lit. A second compare switches OE off through DPPI and GPIOTE,
 * so the on-time (brightness) is set by hardware.
 *
 * Writes go to a back buffer that the interrupt swaps in at the frame start.
 */

#define DT_DRV_COMPAT smartledbadge_hub75

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <gpiote_nrfx.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_gpiote.h>
#include <hal/nrf_timer.h>
#include <helpers/nrfx_gppi.h>
#include <nrfx_gpiote.h>

LOG_MODULE_REGISTER(hub75, CONFIG_DISPLAY_LOG_LEVEL);

#define MAX_ADDR_LINES 5
#define TICKS_PER_US   16
/* OE stays off for at least this long at the end of each slot. */
#define MIN_OFF_US     2
/* Time allowed between scheduling the off compare and switching OE on. */
#define ON_MARGIN      (1 * TICKS_PER_US)

struct hub75_port_pin {
	NRF_GPIO_Type *port;
	uint32_t mask;
};

struct hub75_config {
	struct gpio_dt_spec data[4]; /* R1, G1, B1, CLK */
	struct gpio_dt_spec lat;
	struct gpio_dt_spec oe;
	struct gpio_dt_spec addr[MAX_ADDR_LINES];
	struct hub75_port_pin addr_pin[MAX_ADDR_LINES];
	struct hub75_port_pin lat_pin;
	NRF_GPIO_Type *data_port;
	uint32_t data_mask;
	uint32_t lit_mask;
	uint32_t clk_mask;
	uint32_t oe_psel;
	nrfx_gpiote_t *gpiote;
	NRF_TIMER_Type *timer;
	uint8_t *fb; /* two buffers of height x stride bytes */
	void (*irq_config)(void);
	unsigned int irqn;
	uint16_t width;
	uint16_t height;
	uint16_t stride;
	uint16_t panel_columns;
	uint16_t row_period_us;
	uint8_t addr_lines;
	bool mirror_x;
};

struct hub75_data {
	uint32_t on_ticks;
	volatile uint32_t frames;
	uint8_t oe_ch;
	uint8_t front;
	uint8_t next_row;
	uint8_t brightness;
	volatile bool swap_pending;
	volatile bool back_stale;
	/* Read by the interrupt: a blanked display is never switched on. */
	volatile bool blanked;
};

static inline uint8_t *buffer(const struct hub75_config *cfg, uint8_t idx)
{
	return cfg->fb + (size_t)idx * cfg->height * cfg->stride;
}

static inline void oe_task(const struct hub75_config *cfg, uint8_t ch, bool on)
{
	/* OE is active low at the panel: clear = row on, set = row off. */
	nrf_gpiote_task_trigger(cfg->gpiote->p_reg,
				on ? nrf_gpiote_clr_task_get(ch) : nrf_gpiote_set_task_get(ch));
}

static void set_address(const struct hub75_config *cfg, uint8_t row)
{
	for (uint8_t i = 0; i < cfg->addr_lines; i++) {
		if (row & BIT(i)) {
			nrf_gpio_port_out_set(cfg->addr_pin[i].port, cfg->addr_pin[i].mask);
		} else {
			nrf_gpio_port_out_clear(cfg->addr_pin[i].port, cfg->addr_pin[i].mask);
		}
	}
}

static void shift_row(const struct hub75_config *cfg, const uint8_t *line)
{
	NRF_GPIO_Type *port = cfg->data_port;

	for (uint16_t i = 0; i < cfg->panel_columns; i++) {
		/* The first pixel shifted ends up at the far end of the row. */
		uint16_t x = cfg->mirror_x ? i : cfg->panel_columns - 1 - i;
		bool lit = x < cfg->width && (line[x >> 3] & BIT(7 - (x & 7)));

		nrf_gpio_port_out_clear(port, cfg->data_mask);
		if (lit) {
			nrf_gpio_port_out_set(port, cfg->lit_mask);
		}
		arch_nop();
		nrf_gpio_port_out_set(port, cfg->clk_mask);
		arch_nop();
	}
	nrf_gpio_port_out_clear(port, cfg->data_mask);
}

static void hub75_isr(const void *arg)
{
	const struct device *dev = arg;
	const struct hub75_config *cfg = dev->config;
	struct hub75_data *data = dev->data;
	uint8_t row = data->next_row;
	uint8_t next = (row + 1 == cfg->height) ? 0 : row + 1;

	nrf_timer_event_clear(cfg->timer, NRF_TIMER_EVENT_COMPARE0);

	oe_task(cfg, data->oe_ch, false);
	if (data->blanked) {
		return;
	}
	set_address(cfg, row);
	nrf_gpio_port_out_set(cfg->lat_pin.port, cfg->lat_pin.mask);
	nrf_gpio_port_out_clear(cfg->lat_pin.port, cfg->lat_pin.mask);

	if (data->on_ticks > 0) {
		/* The off compare is set from now, so every nonzero brightness
		 * gets its full on-time, and the row is always off by the end of
		 * the slot. A late interrupt skips the row rather than leave it
		 * on. */
		uint32_t last = (cfg->row_period_us - MIN_OFF_US) * TICKS_PER_US;
		uint32_t now;
		uint32_t off_at;

		nrf_timer_task_trigger(cfg->timer, NRF_TIMER_TASK_CAPTURE2);
		now = nrf_timer_cc_get(cfg->timer, NRF_TIMER_CC_CHANNEL2);
		off_at = MIN(now + ON_MARGIN + data->on_ticks, last);
		if (now + ON_MARGIN < off_at) {
			nrf_timer_cc_set(cfg->timer, NRF_TIMER_CC_CHANNEL1, off_at);
			oe_task(cfg, data->oe_ch, true);
		}
	}

	if (next == 0) {
		data->frames++;
		if (data->swap_pending) {
			data->front ^= 1;
			data->swap_pending = false;
			data->back_stale = true;
		}
	}
	shift_row(cfg, buffer(cfg, data->front) + (size_t)next * cfg->stride);
	data->next_row = next;
}

static uint32_t on_ticks_for(const struct hub75_config *cfg, uint8_t brightness)
{
	uint32_t max_on = (cfg->row_period_us - MIN_OFF_US) * TICKS_PER_US;

	return max_on * brightness / 255U;
}

static int hub75_blanking_on(const struct device *dev)
{
	const struct hub75_config *cfg = dev->config;
	struct hub75_data *data = dev->data;

	/* Set first, so an interrupt that still runs leaves OE off. */
	data->blanked = true;
	nrf_timer_task_trigger(cfg->timer, NRF_TIMER_TASK_STOP);
	irq_disable(cfg->irqn);
	nrf_timer_event_clear(cfg->timer, NRF_TIMER_EVENT_COMPARE0);
	NVIC_ClearPendingIRQ((IRQn_Type)cfg->irqn);
	oe_task(cfg, data->oe_ch, false);

	/* Complete a swap queued before blanking, so writes made while blanked go
	 * to the buffer that is shown on resume, not one an old swap replaces. */
	if (data->swap_pending) {
		data->front ^= 1;
		data->swap_pending = false;
		data->back_stale = true;
	}
	return 0;
}

static int hub75_blanking_off(const struct device *dev)
{
	const struct hub75_config *cfg = dev->config;
	struct hub75_data *data = dev->data;

	if (!data->blanked) {
		return 0;
	}
	data->next_row = 0;
	shift_row(cfg, buffer(cfg, data->front));
	nrf_timer_task_trigger(cfg->timer, NRF_TIMER_TASK_CLEAR);
	nrf_timer_event_clear(cfg->timer, NRF_TIMER_EVENT_COMPARE0);
	NVIC_ClearPendingIRQ((IRQn_Type)cfg->irqn);
	data->blanked = false;
	irq_enable(cfg->irqn);
	nrf_timer_task_trigger(cfg->timer, NRF_TIMER_TASK_START);
	return 0;
}

static void copy_region(const struct hub75_config *cfg, uint8_t *dst, uint16_t x, uint16_t y,
			const struct display_buffer_descriptor *desc, const uint8_t *src)
{
	for (uint16_t yy = 0; yy < desc->height; yy++) {
		uint8_t *line = dst + (size_t)(y + yy) * cfg->stride;

		for (uint16_t xx = 0; xx < desc->width; xx++) {
			size_t bit = (size_t)yy * desc->pitch + xx;
			uint16_t px = x + xx;
			uint8_t mask = BIT(7 - (px & 7));

			if (src[bit >> 3] & BIT(7 - (bit & 7))) {
				line[px >> 3] |= mask;
			} else {
				line[px >> 3] &= ~mask;
			}
		}
	}
}

static int hub75_write(const struct device *dev, const uint16_t x, const uint16_t y,
		       const struct display_buffer_descriptor *desc, const void *buf)
{
	const struct hub75_config *cfg = dev->config;
	struct hub75_data *data = dev->data;
	uint8_t *dst;

	if (desc->width == 0 || desc->height == 0 || x + desc->width > cfg->width ||
	    y + desc->height > cfg->height || desc->pitch < desc->width ||
	    desc->buf_size <
		    DIV_ROUND_UP((size_t)desc->pitch * (desc->height - 1) + desc->width, 8)) {
		return -EINVAL;
	}

	if (data->blanked) {
		/* No swaps while blanked: write the shown buffer directly. */
		copy_region(cfg, buffer(cfg, data->front), x, y, desc, buf);
		data->back_stale = true;
		return 0;
	}

	/* Wait for the previous frame's swap (at most one frame). */
	for (int i = 0; data->swap_pending; i++) {
		if (i > 50) {
			return -EBUSY;
		}
		k_msleep(1);
	}

	dst = buffer(cfg, data->front ^ 1);
	if (data->back_stale) {
		memcpy(dst, buffer(cfg, data->front), (size_t)cfg->height * cfg->stride);
		data->back_stale = false;
	}
	copy_region(cfg, dst, x, y, desc, buf);
	if (!desc->frame_incomplete) {
		data->swap_pending = true;
	}
	return 0;
}

int badge_hub75_wait_visible(const struct device *dev, unsigned timeout_ms)
{
	struct hub75_data *data = dev->data;
	int64_t deadline = k_uptime_get() + timeout_ms;
	while (data->swap_pending) {
		if (data->blanked || k_uptime_get() >= deadline) {
			return -ETIMEDOUT;
		}
		k_msleep(1);
	}
	/* A front-buffer swap precedes the row-zero latch. Require one complete
	 * scan as well, so all REC rows have actually reached the panel. */
	uint32_t frame = data->frames;
	while (data->frames == frame) {
		if (data->blanked || k_uptime_get() >= deadline) {
			return -ETIMEDOUT;
		}
		k_msleep(1);
	}
	return data->blanked || data->brightness == 0 ? -EIO : 0;
}

static int hub75_set_brightness(const struct device *dev, const uint8_t brightness)
{
	const struct hub75_config *cfg = dev->config;
	struct hub75_data *data = dev->data;

	data->brightness = brightness;
	/* Used from the next row: the interrupt sets the off compare per row. */
	data->on_ticks = on_ticks_for(cfg, brightness);
	return 0;
}

static void hub75_get_capabilities(const struct device *dev, struct display_capabilities *caps)
{
	const struct hub75_config *cfg = dev->config;

	memset(caps, 0, sizeof(*caps));
	caps->x_resolution = cfg->width;
	caps->y_resolution = cfg->height;
	caps->supported_pixel_formats = PIXEL_FORMAT_MONO01;
	caps->current_pixel_format = PIXEL_FORMAT_MONO01;
	caps->screen_info = SCREEN_INFO_MONO_MSB_FIRST;
	caps->current_orientation = DISPLAY_ORIENTATION_NORMAL;
}

static int hub75_set_pixel_format(const struct device *dev, const enum display_pixel_format pf)
{
	ARG_UNUSED(dev);
	return pf == PIXEL_FORMAT_MONO01 ? 0 : -ENOTSUP;
}

static DEVICE_API(display, hub75_api) = {
	.blanking_on = hub75_blanking_on,
	.blanking_off = hub75_blanking_off,
	.write = hub75_write,
	.set_brightness = hub75_set_brightness,
	.get_capabilities = hub75_get_capabilities,
	.set_pixel_format = hub75_set_pixel_format,
};

static int hub75_init(const struct device *dev)
{
	const struct hub75_config *cfg = dev->config;
	struct hub75_data *data = dev->data;
	nrfx_gppi_handle_t h;
	int err;

	/* OE first, so the panel stays off while the other pins start. */
	err = gpio_pin_configure_dt(&cfg->oe, GPIO_OUTPUT_HIGH);
	for (size_t i = 0; err == 0 && i < ARRAY_SIZE(cfg->data); i++) {
		err = gpio_pin_configure_dt(&cfg->data[i], GPIO_OUTPUT_LOW);
	}
	if (err == 0) {
		err = gpio_pin_configure_dt(&cfg->lat, GPIO_OUTPUT_LOW);
	}
	for (uint8_t i = 0; err == 0 && i < cfg->addr_lines; i++) {
		err = gpio_pin_configure_dt(&cfg->addr[i], GPIO_OUTPUT_LOW);
	}
	if (err < 0) {
		LOG_ERR("GPIO configuration failed: %d", err);
		return err;
	}

	err = nrfx_gpiote_channel_alloc(cfg->gpiote, &data->oe_ch);
	if (err < 0) {
		LOG_ERR("GPIOTE channel allocation failed: %d", err);
		return err;
	}
	nrf_gpiote_task_configure(cfg->gpiote->p_reg, data->oe_ch, cfg->oe_psel,
				  NRF_GPIOTE_POLARITY_NONE, NRF_GPIOTE_INITIAL_VALUE_HIGH);
	nrf_gpiote_task_enable(cfg->gpiote->p_reg, data->oe_ch);

	nrf_timer_mode_set(cfg->timer, NRF_TIMER_MODE_TIMER);
	nrf_timer_bit_width_set(cfg->timer, NRF_TIMER_BIT_WIDTH_32);
	nrf_timer_prescaler_set(cfg->timer, 0);
	nrf_timer_cc_set(cfg->timer, NRF_TIMER_CC_CHANNEL0, cfg->row_period_us * TICKS_PER_US);
	nrf_timer_shorts_enable(cfg->timer, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK);
	nrf_timer_event_clear(cfg->timer, NRF_TIMER_EVENT_COMPARE0);
	nrf_timer_int_enable(cfg->timer, NRF_TIMER_INT_COMPARE0_MASK);
	hub75_set_brightness(dev, CONFIG_HUB75_DISPLAY_INIT_BRIGHTNESS);

	err = nrfx_gppi_conn_alloc(
		nrf_timer_event_address_get(cfg->timer, NRF_TIMER_EVENT_COMPARE1),
		nrf_gpiote_task_address_get(cfg->gpiote->p_reg,
					    nrf_gpiote_set_task_get(data->oe_ch)),
		&h);
	if (err < 0) {
		LOG_ERR("DPPI connection failed: %d", err);
		return err;
	}
	nrfx_gppi_conn_enable(h);

	data->blanked = true;
	cfg->irq_config();
	return 0;
}

#define HUB75_SPEC(n, prop) GPIO_DT_SPEC_INST_GET(n, prop)
#define HUB75_PORT(node, prop, idx)                                                                \
	((NRF_GPIO_Type *)DT_REG_ADDR(DT_GPIO_CTLR_BY_IDX(node, prop, idx)))
#define HUB75_MASK(node, prop, idx)      BIT(DT_GPIO_PIN_BY_IDX(node, prop, idx))
#define HUB75_ADDR_SPEC(node, prop, idx) GPIO_DT_SPEC_GET_BY_IDX(node, prop, idx)
#define HUB75_ADDR_PIN(node, prop, idx)                                                            \
	{.port = HUB75_PORT(node, prop, idx), .mask = HUB75_MASK(node, prop, idx)}
#define HUB75_SAME_PORT(n, prop)                                                                   \
	(DT_DEP_ORD(DT_GPIO_CTLR(DT_DRV_INST(n), prop)) ==                                         \
	 DT_DEP_ORD(DT_GPIO_CTLR(DT_DRV_INST(n), r1_gpios)))
#define HUB75_TIMER(n) DT_INST_PHANDLE(n, timer)

#define HUB75_DEFINE(n)                                                                            \
	BUILD_ASSERT(HUB75_SAME_PORT(n, g1_gpios) && HUB75_SAME_PORT(n, b1_gpios) &&               \
			     HUB75_SAME_PORT(n, clk_gpios),                                        \
		     "R1, G1, B1 and CLK must be on one GPIO port");                               \
	BUILD_ASSERT(DT_INST_PROP_LEN(n, addr_gpios) <= MAX_ADDR_LINES, "too many address lines"); \
	BUILD_ASSERT(DT_INST_PROP(n, height) <= BIT(DT_INST_PROP_LEN(n, addr_gpios)),              \
		     "height exceeds the rows the address lines select");                          \
	BUILD_ASSERT(DT_INST_PROP(n, width) <= DT_INST_PROP(n, panel_columns),                     \
		     "width exceeds panel-columns");                                               \
	BUILD_ASSERT(DT_INST_PROP(n, row_period_us) > 4 * MIN_OFF_US, "row period too short");     \
                                                                                                   \
	static uint8_t hub75_fb_##n[2 * DT_INST_PROP(n, height) *                                  \
				    DIV_ROUND_UP(DT_INST_PROP(n, width), 8)];                      \
	static struct hub75_data hub75_data_##n;                                                   \
                                                                                                   \
	static void hub75_irq_config_##n(void)                                                     \
	{                                                                                          \
		IRQ_CONNECT(DT_IRQN(HUB75_TIMER(n)), DT_IRQ(HUB75_TIMER(n), priority), hub75_isr,  \
			    DEVICE_DT_INST_GET(n), 0);                                             \
	}                                                                                          \
                                                                                                   \
	static const struct hub75_config hub75_config_##n = {                                      \
		.data = {HUB75_SPEC(n, r1_gpios), HUB75_SPEC(n, g1_gpios),                         \
			 HUB75_SPEC(n, b1_gpios), HUB75_SPEC(n, clk_gpios)},                       \
		.lat = HUB75_SPEC(n, lat_gpios),                                                   \
		.oe = HUB75_SPEC(n, oe_gpios),                                                     \
		.addr = {DT_INST_FOREACH_PROP_ELEM_SEP(n, addr_gpios, HUB75_ADDR_SPEC, (, ))},     \
		.addr_pin = {DT_INST_FOREACH_PROP_ELEM_SEP(n, addr_gpios, HUB75_ADDR_PIN, (, ))},  \
		.lat_pin = HUB75_ADDR_PIN(DT_DRV_INST(n), lat_gpios, 0),                           \
		.data_port = HUB75_PORT(DT_DRV_INST(n), r1_gpios, 0),                              \
		.data_mask = HUB75_MASK(DT_DRV_INST(n), r1_gpios, 0) |                             \
			     HUB75_MASK(DT_DRV_INST(n), g1_gpios, 0) |                             \
			     HUB75_MASK(DT_DRV_INST(n), b1_gpios, 0) |                             \
			     HUB75_MASK(DT_DRV_INST(n), clk_gpios, 0),                             \
		.lit_mask =                                                                        \
			((DT_INST_PROP(n, on_color) & 1) ? HUB75_MASK(DT_DRV_INST(n), r1_gpios, 0) \
							 : 0) |                                    \
			((DT_INST_PROP(n, on_color) & 2) ? HUB75_MASK(DT_DRV_INST(n), g1_gpios, 0) \
							 : 0) |                                    \
			((DT_INST_PROP(n, on_color) & 4) ? HUB75_MASK(DT_DRV_INST(n), b1_gpios, 0) \
							 : 0),                                     \
		.clk_mask = HUB75_MASK(DT_DRV_INST(n), clk_gpios, 0),                              \
		.oe_psel = NRF_DT_GPIOS_TO_PSEL(DT_DRV_INST(n), oe_gpios),                         \
		.gpiote = &GPIOTE_NRFX_INST_BY_NODE(NRF_DT_GPIOTE_NODE(DT_DRV_INST(n), oe_gpios)), \
		.timer = (NRF_TIMER_Type *)DT_REG_ADDR(HUB75_TIMER(n)),                            \
		.fb = hub75_fb_##n,                                                                \
		.irq_config = hub75_irq_config_##n,                                                \
		.irqn = DT_IRQN(HUB75_TIMER(n)),                                                   \
		.width = DT_INST_PROP(n, width),                                                   \
		.height = DT_INST_PROP(n, height),                                                 \
		.stride = DIV_ROUND_UP(DT_INST_PROP(n, width), 8),                                 \
		.panel_columns = DT_INST_PROP(n, panel_columns),                                   \
		.row_period_us = DT_INST_PROP(n, row_period_us),                                   \
		.addr_lines = DT_INST_PROP_LEN(n, addr_gpios),                                     \
		.mirror_x = DT_INST_PROP(n, mirror_x),                                             \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, hub75_init, NULL, &hub75_data_##n, &hub75_config_##n,             \
			      POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY, &hub75_api);

DT_INST_FOREACH_STATUS_OKAY(HUB75_DEFINE)
