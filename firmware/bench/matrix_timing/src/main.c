/*
 * SPDX-License-Identifier: MIT
 *
 * Bench test F1: drives the Rev A matrix scan signals on the nRF5340 DK with
 * the planned hardware timing, for a logic analyzer. No matrix is attached.
 *
 * Row slot (TIMER1, 16 MHz, cleared at the period):
 *
 *   1 us    LAT high   latches the row shifted during the previous slot
 *   2 us    LAT low    and SPIM4 starts shifting the next row (8 bytes)
 *   4 us    BLANK low  row on
 *   159 us  BLANK high row off
 *   160 us  period     TIMER2 counts the row
 *
 * After 32 rows TIMER2 stops TIMER1 over DPPI and interrupts. The interrupt
 * rewinds the SPIM4 list pointer and restarts TIMER1, so a late interrupt
 * leaves the matrix blanked rather than a row on. The frame marker is high
 * while the interrupt handler runs.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/onoff.h>
#include <zephyr/sys/util.h>

#include <gpiote_nrfx.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_gpiote.h>
#include <hal/nrf_spim.h>
#include <hal/nrf_timer.h>
#include <helpers/nrfx_gppi.h>
#include <nrfx_gpiote.h>

LOG_MODULE_REGISTER(matrix_timing, LOG_LEVEL_INF);

#define USER_NODE  DT_PATH(zephyr_user)
#define SPIM_NODE  DT_NODELABEL(spi4)
#define SLOT_NODE  DT_NODELABEL(timer1)
#define COUNT_NODE DT_NODELABEL(timer2)

#define LAT_PIN   NRF_DT_GPIOS_TO_PSEL(USER_NODE, mtx_lat_gpios)
#define BLANK_PIN NRF_DT_GPIOS_TO_PSEL(USER_NODE, mtx_blank_gpios)
#define FRAME_PIN NRF_DT_GPIOS_TO_PSEL(USER_NODE, bench_frame_gpios)

#define ROWS      32
#define ROW_BYTES 8 /* 32 row bits, then 32 column bits */

/* TIMER1 runs at 16 MHz (prescaler 0). */
#define TICKS_PER_US 16
#define T_LAT_SET    (1 * TICKS_PER_US)
#define T_LAT_CLR    (2 * TICKS_PER_US)
#define T_ROW_ON     (4 * TICKS_PER_US)
#define T_ROW_OFF    (159 * TICKS_PER_US)
#define T_PERIOD     (160 * TICKS_PER_US)

BUILD_ASSERT(T_LAT_SET < T_LAT_CLR && T_LAT_CLR < T_ROW_ON && T_ROW_ON < T_ROW_OFF &&
	     T_ROW_OFF < T_PERIOD);
/* The shift (64 bits at 8 MHz, 8 us) must end before the next latch. */
BUILD_ASSERT(T_PERIOD + T_LAT_SET - T_LAT_CLR > 9 * TICKS_PER_US);

static NRF_TIMER_Type *const slot_timer = (NRF_TIMER_Type *)DT_REG_ADDR(SLOT_NODE);
static NRF_TIMER_Type *const row_counter = (NRF_TIMER_Type *)DT_REG_ADDR(COUNT_NODE);
static NRF_SPIM_Type *const spim = (NRF_SPIM_Type *)DT_REG_ADDR(SPIM_NODE);
static nrfx_gpiote_t *const gpiote =
	&GPIOTE_NRFX_INST_BY_NODE(NRF_DT_GPIOTE_NODE(USER_NODE, mtx_lat_gpios));

BUILD_ASSERT(NRF_DT_GPIOTE_INST(USER_NODE, mtx_lat_gpios) ==
		     NRF_DT_GPIOTE_INST(USER_NODE, mtx_blank_gpios),
	     "LAT and BLANK must share one GPIOTE instance");

PINCTRL_DT_DEFINE(SPIM_NODE);

/*
 * SPIM4 list: slot s shifts entry s, which holds row (s + 1) % ROWS, so the
 * latch at the start of slot s shows row s. Entry ROWS - 1 (row 0) is shifted
 * once before the first frame.
 */
static uint8_t frame[ROWS][ROW_BYTES];

static volatile uint32_t frames;

static void frame_isr(const void *arg)
{
	ARG_UNUSED(arg);

	nrf_gpio_pin_set(FRAME_PIN);

	nrf_timer_event_clear(row_counter, NRF_TIMER_EVENT_COMPARE0);
	nrf_timer_task_trigger(row_counter, NRF_TIMER_TASK_CLEAR);
	nrf_timer_task_trigger(slot_timer, NRF_TIMER_TASK_CLEAR);
	nrf_spim_tx_buffer_set(spim, frame[0], ROW_BYTES);
	nrf_timer_task_trigger(slot_timer, NRF_TIMER_TASK_START);

	frames++;

	nrf_gpio_pin_clear(FRAME_PIN);
}

static void fill_frame(void)
{
	for (int s = 0; s < ROWS; s++) {
		int row = (s + 1) % ROWS;
		/* A 595 output low turns its row's PFET on: one row bit is 0. */
		uint32_t rows = ~BIT(row);
		uint8_t *b = frame[s];

		sys_put_be32(rows, &b[0]);
		b[4] = 0xA5;
		b[5] = (uint8_t)row;
		b[6] = 0x5A;
		b[7] = (uint8_t)~row;
	}
}

static int hfxo_start(void)
{
	struct onoff_manager *mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
	struct onoff_client cli;
	int res;
	int err;

	if (mgr == NULL) {
		return -ENODEV;
	}
	sys_notify_init_spinwait(&cli.notify);
	err = onoff_request(mgr, &cli);
	if (err < 0) {
		return err;
	}
	while ((err = sys_notify_fetch_result(&cli.notify, &res)) == -EAGAIN) {
		k_msleep(1);
	}
	return err < 0 ? err : res;
}

static int gpiote_task_init(uint32_t pin, nrf_gpiote_outinit_t init, uint8_t *ch)
{
	int err = nrfx_gpiote_channel_alloc(gpiote, ch);

	if (err < 0) {
		return err;
	}
	nrf_gpiote_task_configure(gpiote->p_reg, *ch, pin, NRF_GPIOTE_POLARITY_NONE, init);
	nrf_gpiote_task_enable(gpiote->p_reg, *ch);
	return 0;
}

static uint32_t gpiote_task(nrf_gpiote_task_t task)
{
	return nrf_gpiote_task_address_get(gpiote->p_reg, task);
}

static uint32_t slot_event(nrf_timer_cc_channel_t cc)
{
	return nrf_timer_event_address_get(slot_timer, nrf_timer_compare_event_get(cc));
}

static int connect(uint32_t eep, uint32_t tep, nrfx_gppi_handle_t *handle)
{
	int err = nrfx_gppi_conn_alloc(eep, tep, handle);

	if (err < 0) {
		return err;
	}
	nrfx_gppi_conn_enable(*handle);
	return 0;
}

static int scan_init(void)
{
	uint8_t lat_ch;
	uint8_t blank_ch;
	nrfx_gppi_handle_t h;
	int err;

	/* Frame marker; LAT and BLANK are driven by GPIOTE from here on. */
	nrf_gpio_cfg_output(FRAME_PIN);
	nrf_gpio_pin_clear(FRAME_PIN);

	err = gpiote_task_init(BLANK_PIN, NRF_GPIOTE_INITIAL_VALUE_HIGH, &blank_ch);
	if (err == 0) {
		err = gpiote_task_init(LAT_PIN, NRF_GPIOTE_INITIAL_VALUE_LOW, &lat_ch);
	}
	if (err < 0) {
		LOG_ERR("GPIOTE channel allocation failed: %d", err);
		return err;
	}

	err = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(SPIM_NODE), PINCTRL_STATE_DEFAULT);
	if (err < 0) {
		LOG_ERR("SPIM4 pinctrl failed: %d", err);
		return err;
	}
	nrf_spim_configure(spim, NRF_SPIM_MODE_0, NRF_SPIM_BIT_ORDER_MSB_FIRST);
	nrf_spim_frequency_set(spim, NRF_SPIM_FREQ_8M);
	nrf_spim_orc_set(spim, 0);
	nrf_spim_rx_buffer_set(spim, NULL, 0);
	nrf_spim_tx_list_enable(spim);
#if NRF_ERRATA_STATIC_CHECK(53, 135)
	/* Same SPIM4 workaround nrfx_spim applies while the peripheral is in use. */
	if (NRF_ERRATA_DYNAMIC_CHECK(53, 135)) {
		*(volatile uint32_t *)((uint8_t *)spim + 0xc04) = 1;
	}
#endif
	nrf_spim_enable(spim);

	nrf_timer_mode_set(slot_timer, NRF_TIMER_MODE_TIMER);
	nrf_timer_bit_width_set(slot_timer, NRF_TIMER_BIT_WIDTH_16);
	nrf_timer_prescaler_set(slot_timer, 0);
	nrf_timer_cc_set(slot_timer, NRF_TIMER_CC_CHANNEL0, T_LAT_SET);
	nrf_timer_cc_set(slot_timer, NRF_TIMER_CC_CHANNEL1, T_LAT_CLR);
	nrf_timer_cc_set(slot_timer, NRF_TIMER_CC_CHANNEL2, T_ROW_ON);
	nrf_timer_cc_set(slot_timer, NRF_TIMER_CC_CHANNEL3, T_ROW_OFF);
	nrf_timer_cc_set(slot_timer, NRF_TIMER_CC_CHANNEL5, T_PERIOD);
	nrf_timer_shorts_enable(slot_timer, NRF_TIMER_SHORT_COMPARE5_CLEAR_MASK);

	nrf_timer_mode_set(row_counter, NRF_TIMER_MODE_COUNTER);
	nrf_timer_bit_width_set(row_counter, NRF_TIMER_BIT_WIDTH_16);
	nrf_timer_cc_set(row_counter, NRF_TIMER_CC_CHANNEL0, ROWS);
	nrf_timer_event_clear(row_counter, NRF_TIMER_EVENT_COMPARE0);
	nrf_timer_int_enable(row_counter, NRF_TIMER_INT_COMPARE0_MASK);

	err = connect(slot_event(NRF_TIMER_CC_CHANNEL0),
		      gpiote_task(nrf_gpiote_set_task_get(lat_ch)), &h);
	if (err == 0) {
		err = connect(slot_event(NRF_TIMER_CC_CHANNEL1),
			      gpiote_task(nrf_gpiote_clr_task_get(lat_ch)), &h);
	}
	if (err == 0) {
		/* Same DPPI channel: the shift starts as the latch pulse ends. */
		err = nrfx_gppi_ep_attach(nrf_spim_task_address_get(spim, NRF_SPIM_TASK_START), h);
	}
	if (err == 0) {
		err = connect(slot_event(NRF_TIMER_CC_CHANNEL2),
			      gpiote_task(nrf_gpiote_clr_task_get(blank_ch)), &h);
	}
	if (err == 0) {
		err = connect(slot_event(NRF_TIMER_CC_CHANNEL3),
			      gpiote_task(nrf_gpiote_set_task_get(blank_ch)), &h);
	}
	if (err == 0) {
		err = connect(slot_event(NRF_TIMER_CC_CHANNEL5),
			      nrf_timer_task_address_get(row_counter, NRF_TIMER_TASK_COUNT), &h);
	}
	if (err == 0) {
		err = connect(nrf_timer_event_address_get(row_counter, NRF_TIMER_EVENT_COMPARE0),
			      nrf_timer_task_address_get(slot_timer, NRF_TIMER_TASK_STOP), &h);
	}
	if (err < 0) {
		LOG_ERR("DPPI connection failed: %d", err);
		return err;
	}

	IRQ_CONNECT(DT_IRQN(COUNT_NODE), 1, frame_isr, NULL, 0);
	irq_enable(DT_IRQN(COUNT_NODE));

	return 0;
}

static void scan_start(void)
{
	/* Shift row 0 so the first latch shows it. */
	nrf_spim_event_clear(spim, NRF_SPIM_EVENT_END);
	nrf_spim_tx_buffer_set(spim, frame[ROWS - 1], ROW_BYTES);
	nrf_spim_task_trigger(spim, NRF_SPIM_TASK_START);
	while (!nrf_spim_event_check(spim, NRF_SPIM_EVENT_END)) {
	}

	nrf_spim_tx_buffer_set(spim, frame[0], ROW_BYTES);
	nrf_timer_task_trigger(row_counter, NRF_TIMER_TASK_CLEAR);
	nrf_timer_task_trigger(row_counter, NRF_TIMER_TASK_START);
	nrf_timer_task_trigger(slot_timer, NRF_TIMER_TASK_CLEAR);
	nrf_timer_task_trigger(slot_timer, NRF_TIMER_TASK_START);
}

int main(void)
{
	int err;

	err = hfxo_start();
	if (err < 0) {
		LOG_ERR("HFXO start failed: %d", err);
		return 0;
	}

	fill_frame();
	err = scan_init();
	if (err < 0) {
		return 0;
	}

	LOG_INF("F1 matrix timing: %d rows x %d bytes, SCK 8 MHz, slot %d us", ROWS, ROW_BYTES,
		T_PERIOD / TICKS_PER_US);
	LOG_INF("LAT %d-%d us, row on %d-%d us; SCK P0.08 MOSI P0.09 LAT P0.11 "
		"BLANK P0.12 frame P0.26",
		T_LAT_SET / TICKS_PER_US, T_LAT_CLR / TICKS_PER_US, T_ROW_ON / TICKS_PER_US,
		T_ROW_OFF / TICKS_PER_US);

	scan_start();

	uint32_t last = 0;

	while (true) {
		k_sleep(K_SECONDS(1));
		uint32_t now = frames;

		LOG_INF("%u frames/s", now - last);
		last = now;
	}

	return 0;
}
