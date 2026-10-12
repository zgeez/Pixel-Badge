/*
 * SPDX-License-Identifier: MIT
 *
 * TDK ICM-42688-P on I2C.
 *
 * Zephyr's icm4268x driver in NCS v3.4.1 supports SPI only. This driver adds
 * the I2C path with the same devicetree properties, so moving the part to SPI
 * only changes the bus node and compatible. Register addresses and fields come
 * from that driver's icm4268x_reg.h; datasheet references are to TDK
 * DS-000347 Rev 1.2.
 */

#define DT_DRV_COMPAT invensense_icm42688p_i2c

#include <zephyr/device.h>
#include <zephyr/dt-bindings/sensor/icm42688.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include "icm4268x_reg.h"
#include "bus_lock.h"

LOG_MODULE_REGISTER(icm42688_i2c, CONFIG_SENSOR_LOG_LEVEL);

/* Bank 0 register address without the bank encoding of icm4268x_reg.h. */
#define B0(reg) ((uint8_t)FIELD_GET(REG_ADDRESS_MASK, (reg)))

/*
 * icm4268x_reg.h encodes banks 2-4 in a form that does not fit BANK_SEL, so
 * other-bank registers used here carry an explicit bank number. Wake-on-motion
 * thresholds (bank 4, 0x4A-0x4C) will use the same reg_write() path.
 */
#define BANK1_INTF_CONFIG4        0x7A
#define INTF_CONFIG4_I3C_BUS_MODE BIT(6)
#define BANK1_INTF_CONFIG6        0x7C
#define INTF_CONFIG6_I3C_SDR_EN   BIT(0)
#define INTF_CONFIG6_I3C_DDR_EN   BIT(1)

/* §12.3 I2C driver settings: I2C_SLEW_RATE = 1, SPI_SLEW_RATE = 1. */
#define DRIVE_CONFIG_I2C 0x09
/* INTF_CONFIG0: reset value 0x30 (big-endian data) with UI_SIFS_CFG = 10,
 * which disables the SPI interface while the part is used on I2C.
 * FIFO_HOLD_LAST_DATA_EN stays 0, so a sensor's data registers read -32768
 * from reset until its first sample and a valid -32768 is reported as -32766.
 * After the first sample the registers hold the last valid sample until a new
 * one arrives (§12.8, §14.34).
 */
#define INTF_CONFIG0_I2C 0x32
#define INVALID_SAMPLE   INT16_MIN
/* INT1 push-pull, active high, pulsed. */
#define INT_CONFIG_INT1  (BIT_INT1_DRIVE_CIRCUIT | BIT_INT1_POLARITY)

#define DATA_LEN         14  /* TEMP_DATA1 .. GYRO_DATA_Z0, big-endian */
#define POR_DELAY_MS     3   /* registers usable 1 ms after POR; 3 ms supply ramp */
#define MODE_DELAY_US    250 /* §14.36: wait 200 us after a PWR_MGMT0 change */
#define STARTUP_DELAY_MS 50  /* accel 10 ms, gyro 30 ms start-up */

struct icm42688_i2c_config {
	struct i2c_dt_spec i2c;
#ifdef CONFIG_ICM42688_I2C_TRIGGER
	struct gpio_dt_spec int1;
#endif
	uint8_t accel_pwr_mode;
	uint8_t accel_odr;
	uint8_t accel_fs;
	uint8_t gyro_pwr_mode;
	uint8_t gyro_odr;
	uint8_t gyro_fs;
};

struct icm42688_i2c_data {
	uint8_t bank;
	int16_t temp;
	int16_t accel[3];
	int16_t gyro[3];
	/* Whether the last fetch returned a valid sample for each group. */
	bool temp_valid;
	bool accel_valid;
	bool gyro_valid;
#ifdef CONFIG_ICM42688_I2C_TRIGGER
	const struct device *dev;
	struct gpio_callback gpio_cb;
	struct k_work work;
	sensor_trigger_handler_t drdy_handler;
	const struct sensor_trigger *drdy_trigger;
#endif
};

static int select_bank(const struct device *dev, uint8_t bank)
{
	const struct icm42688_i2c_config *cfg = dev->config;
	struct icm42688_i2c_data *data = dev->data;
	int ret;

	if (data->bank == bank) {
		return 0;
	}
	ret = i2c_reg_write_byte_dt(&cfg->i2c, REG_BANK_SEL, bank);
	data->bank = (ret == 0) ? bank : UINT8_MAX;
	return ret;
}

static int reg_write(const struct device *dev, uint8_t bank, uint8_t reg, uint8_t val)
{
	const struct icm42688_i2c_config *cfg = dev->config;
	int ret = badge_bus_lock();
	if (ret != 0) {
		return ret;
	}
	ret = select_bank(dev, bank);
	if (ret == 0) {
		ret = i2c_reg_write_byte_dt(&cfg->i2c, reg, val);
	}
	badge_bus_unlock();
	return ret;
}

static int reg_read(const struct device *dev, uint8_t bank, uint8_t reg, uint8_t *buf, uint32_t len)
{
	const struct icm42688_i2c_config *cfg = dev->config;
	int ret = badge_bus_lock();
	if (ret != 0) {
		return ret;
	}
	ret = select_bank(dev, bank);
	if (ret == 0) {
		ret = i2c_burst_read_dt(&cfg->i2c, reg, buf, len);
	}
	badge_bus_unlock();
	return ret;
}

static int reg_update(const struct device *dev, uint8_t bank, uint8_t reg, uint8_t mask,
		      uint8_t val)
{
	uint8_t old;
	int ret = reg_read(dev, bank, reg, &old, 1);

	return ret ? ret : reg_write(dev, bank, reg, (old & ~mask) | (val & mask));
}

static int icm42688_i2c_reset(const struct device *dev)
{
	struct icm42688_i2c_data *data = dev->data;
	uint8_t val;
	int ret;

	ret = reg_write(dev, 0, B0(REG_DEVICE_CONFIG), BIT_SOFT_RESET_CONFIG);
	if (ret) {
		return ret;
	}
	k_msleep(SOFT_RESET_TIME_MS);
	data->bank = UINT8_MAX; /* re-select explicitly after the reset */

	/* Reading INT_STATUS also clears RESET_DONE. */
	ret = reg_read(dev, 0, B0(REG_INT_STATUS), &val, 1);
	if (ret) {
		return ret;
	}
	if (!(val & BIT_RESET_DONE_INT)) {
		LOG_ERR("reset not reported (INT_STATUS 0x%02x)", val);
		return -EIO;
	}

	ret = reg_read(dev, 0, B0(REG_WHO_AM_I), &val, 1);
	if (ret) {
		return ret;
	}
	if (val != WHO_AM_I_ICM42688) {
		LOG_ERR("WHO_AM_I 0x%02x, expected 0x%02x", val, WHO_AM_I_ICM42688);
		return -ENODEV;
	}
	return 0;
}

/*
 * The binding limits each property to valid codes; this rejects the
 * accelerometer ODRs its power mode cannot run (§14.38, §12.1): 6.25 Hz and
 * below are LP only, 1 kHz and above are LN only.
 */
static int icm42688_i2c_check_config(const struct icm42688_i2c_config *cfg)
{
	bool lp_only = cfg->accel_odr >= ICM42688_DT_ACCEL_ODR_6_25 &&
		       cfg->accel_odr <= ICM42688_DT_ACCEL_ODR_1_5625;
	bool ln_only = cfg->accel_odr <= ICM42688_DT_ACCEL_ODR_1000;

	if ((cfg->accel_pwr_mode == ICM42688_DT_ACCEL_LP && ln_only) ||
	    (cfg->accel_pwr_mode == ICM42688_DT_ACCEL_LN && lp_only)) {
		LOG_ERR("accel ODR code %u not supported in power mode %u", cfg->accel_odr,
			cfg->accel_pwr_mode);
		return -EINVAL;
	}
	return 0;
}

static int icm42688_i2c_configure(const struct device *dev)
{
	const struct icm42688_i2c_config *cfg = dev->config;
	/* INT pulse settings follow the SPI driver: 8 us pulses and no
	 * de-assert time are required at ODR >= 4 kHz. INT_ASYNC_RESET is left
	 * at 0 as §12.6 requires for INT1/INT2 operation.
	 */
	bool fast = (cfg->accel_pwr_mode != ICM42688_DT_ACCEL_OFF &&
		     cfg->accel_odr <= ICM42688_DT_ACCEL_ODR_4000) ||
		    (cfg->gyro_pwr_mode == ICM42688_DT_GYRO_LN &&
		     cfg->gyro_odr <= ICM42688_DT_GYRO_ODR_4000);
	/* ODR and full scale are set before PWR_MGMT0 turns the sensors on. */
	const struct {
		uint8_t bank, reg, mask, val;
	} seq[] = {
		{1, BANK1_INTF_CONFIG6, INTF_CONFIG6_I3C_SDR_EN | INTF_CONFIG6_I3C_DDR_EN, 0},
		{1, BANK1_INTF_CONFIG4, INTF_CONFIG4_I3C_BUS_MODE, 0},
		{0, B0(REG_DRIVE_CONFIG), 0xFF, DRIVE_CONFIG_I2C},
		{0, B0(REG_INTF_CONFIG0), 0xFF, INTF_CONFIG0_I2C},
		{0, B0(REG_INT_SOURCE0), 0xFF, 0},
		{0, B0(REG_INT_CONFIG), 0xFF, INT_CONFIG_INT1},
		{0, B0(REG_INT_CONFIG1), 0xFF,
		 fast ? (BIT_INT_TPULSE_DURATION | BIT_INT_TDEASSERT_DISABLE) : 0},
		{0, B0(REG_ACCEL_CONFIG0), 0xFF,
		 FIELD_PREP(MASK_ACCEL_ODR, cfg->accel_odr) |
			 FIELD_PREP(MASK_ACCEL_UI_FS_SEL, cfg->accel_fs)},
		{0, B0(REG_GYRO_CONFIG0), 0xFF,
		 FIELD_PREP(MASK_GYRO_ODR, cfg->gyro_odr) |
			 FIELD_PREP(MASK_GYRO_UI_FS_SEL, cfg->gyro_fs)},
		{0, B0(REG_PWR_MGMT0), 0xFF,
		 FIELD_PREP(MASK_GYRO_MODE, cfg->gyro_pwr_mode) |
			 FIELD_PREP(MASK_ACCEL_MODE, cfg->accel_pwr_mode)},
	};
	int ret;

	for (size_t i = 0; i < ARRAY_SIZE(seq); i++) {
		ret = reg_update(dev, seq[i].bank, seq[i].reg, seq[i].mask, seq[i].val);
		if (ret) {
			LOG_ERR("write bank %u reg 0x%02x failed (%d)", seq[i].bank, seq[i].reg,
				ret);
			return ret;
		}
		if (seq[i].bank == 1 && seq[i].reg == BANK1_INTF_CONFIG6) {
			/* On the bench module the transaction after this write
			 * (I3C off) stalled until the TWIM timeout and its bus
			 * recovery, then worked (2026-10-11). Run the recovery
			 * now instead of stalling the bus for the timeout.
			 */
			(void)i2c_recover_bus(cfg->i2c.bus);
		}
	}
	k_busy_wait(MODE_DELAY_US);

	if (cfg->accel_pwr_mode || cfg->gyro_pwr_mode) {
		k_msleep(STARTUP_DELAY_MS);
	}
	return 0;
}

static int icm42688_i2c_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	struct icm42688_i2c_data *data = dev->data;
	uint8_t buf[DATA_LEN];
	int ret;

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_ACCEL_XYZ &&
	    chan != SENSOR_CHAN_GYRO_XYZ && chan != SENSOR_CHAN_DIE_TEMP) {
		return -ENOTSUP;
	}
	ret = reg_read(dev, 0, B0(REG_TEMP_DATA1), buf, sizeof(buf));
	if (ret) {
		return ret;
	}
	/* A sensor that is off or has not produced its first sample since reset
	 * reads INVALID_SAMPLE. This is not a freshness check: with unequal ODRs
	 * the slower sensor's registers repeat its last sample (§12.8).
	 */
	data->temp = (int16_t)sys_get_be16(&buf[0]);
	data->temp_valid = data->temp != INVALID_SAMPLE;
	data->accel_valid = true;
	data->gyro_valid = true;
	for (int i = 0; i < 3; i++) {
		data->accel[i] = (int16_t)sys_get_be16(&buf[2 + 2 * i]);
		data->gyro[i] = (int16_t)sys_get_be16(&buf[8 + 2 * i]);
		data->accel_valid &= data->accel[i] != INVALID_SAMPLE;
		data->gyro_valid &= data->gyro[i] != INVALID_SAMPLE;
	}

	switch (chan) {
	case SENSOR_CHAN_ACCEL_XYZ:
		return data->accel_valid ? 0 : -EAGAIN;
	case SENSOR_CHAN_GYRO_XYZ:
		return data->gyro_valid ? 0 : -EAGAIN;
	case SENSOR_CHAN_DIE_TEMP:
		return data->temp_valid ? 0 : -EAGAIN;
	default:
		return (data->accel_valid || data->gyro_valid) ? 0 : -EAGAIN;
	}
}

static void accel_to_ms2(const struct icm42688_i2c_config *cfg, int16_t raw,
			 struct sensor_value *val)
{
	/* ACCEL_UI_FS_SEL 0..3 = +/-16, 8, 4, 2 g over the int16 range. */
	int64_t ug = (int64_t)raw * (16000000 >> cfg->accel_fs) / 32768;

	sensor_ug_to_ms2((int32_t)ug, val);
}

static void gyro_to_rads(const struct icm42688_i2c_config *cfg, int16_t raw,
			 struct sensor_value *val)
{
	/* GYRO_UI_FS_SEL 0..7 = +/-2000 dps halving down to 15.625 dps. */
	int64_t mdps = 2000000 >> cfg->gyro_fs;
	int64_t ten_udeg = (int64_t)raw * mdps * 100 / 32768;

	sensor_10udegrees_to_rad((int32_t)ten_udeg, val);
}

static int icm42688_i2c_channel_get(const struct device *dev, enum sensor_channel chan,
				    struct sensor_value *val)
{
	const struct icm42688_i2c_config *cfg = dev->config;
	struct icm42688_i2c_data *data = dev->data;

	bool valid;

	switch (chan) {
	case SENSOR_CHAN_ACCEL_X:
	case SENSOR_CHAN_ACCEL_Y:
	case SENSOR_CHAN_ACCEL_Z:
	case SENSOR_CHAN_ACCEL_XYZ:
		valid = data->accel_valid;
		break;
	case SENSOR_CHAN_GYRO_X:
	case SENSOR_CHAN_GYRO_Y:
	case SENSOR_CHAN_GYRO_Z:
	case SENSOR_CHAN_GYRO_XYZ:
		valid = data->gyro_valid;
		break;
	case SENSOR_CHAN_DIE_TEMP:
		valid = data->temp_valid;
		break;
	default:
		return -ENOTSUP;
	}
	if (!valid) {
		return -ENODATA;
	}

	switch (chan) {
	case SENSOR_CHAN_ACCEL_X:
	case SENSOR_CHAN_ACCEL_Y:
	case SENSOR_CHAN_ACCEL_Z:
		accel_to_ms2(cfg, data->accel[chan - SENSOR_CHAN_ACCEL_X], val);
		return 0;
	case SENSOR_CHAN_ACCEL_XYZ:
		for (int i = 0; i < 3; i++) {
			accel_to_ms2(cfg, data->accel[i], &val[i]);
		}
		return 0;
	case SENSOR_CHAN_GYRO_X:
	case SENSOR_CHAN_GYRO_Y:
	case SENSOR_CHAN_GYRO_Z:
		gyro_to_rads(cfg, data->gyro[chan - SENSOR_CHAN_GYRO_X], val);
		return 0;
	case SENSOR_CHAN_GYRO_XYZ:
		for (int i = 0; i < 3; i++) {
			gyro_to_rads(cfg, data->gyro[i], &val[i]);
		}
		return 0;
	case SENSOR_CHAN_DIE_TEMP:
		/* §4.1 note: degrees C = TEMP_DATA / 132.48 + 25. */
		return sensor_value_from_micro(val,
					       (int64_t)data->temp * 100000000 / 13248 + 25000000);
	default:
		return -ENOTSUP;
	}
}

#ifdef CONFIG_ICM42688_I2C_TRIGGER
static void icm42688_i2c_work(struct k_work *work)
{
	struct icm42688_i2c_data *data = CONTAINER_OF(work, struct icm42688_i2c_data, work);
	sensor_trigger_handler_t handler = data->drdy_handler;
	uint8_t status;

	/* Reading INT_STATUS clears DATA_RDY_INT. */
	if (reg_read(data->dev, 0, B0(REG_INT_STATUS), &status, 1) == 0 &&
	    (status & BIT_DATA_RDY_INT) && handler != NULL) {
		handler(data->dev, data->drdy_trigger);
	}
}

static void icm42688_i2c_gpio_cb(const struct device *port, struct gpio_callback *cb,
				 gpio_port_pins_t pins)
{
	struct icm42688_i2c_data *data = CONTAINER_OF(cb, struct icm42688_i2c_data, gpio_cb);

	ARG_UNUSED(port);
	ARG_UNUSED(pins);
	k_work_submit(&data->work);
}

static int icm42688_i2c_trigger_set(const struct device *dev, const struct sensor_trigger *trig,
				    sensor_trigger_handler_t handler)
{
	const struct icm42688_i2c_config *cfg = dev->config;
	struct icm42688_i2c_data *data = dev->data;
	int ret;

	if (trig->type != SENSOR_TRIG_DATA_READY || cfg->int1.port == NULL) {
		return -ENOTSUP;
	}
	ret = gpio_pin_interrupt_configure_dt(&cfg->int1, GPIO_INT_DISABLE);
	if (ret) {
		return ret;
	}
	data->drdy_handler = handler;
	/* sensor_trigger_set() stores the caller's trigger as a pointer, not a
	 * copy, so handlers can use CONTAINER_OF on it.
	 */
	data->drdy_trigger = trig;
	ret = reg_write(dev, 0, B0(REG_INT_SOURCE0), handler ? BIT_UI_DRDY_INT1_EN : 0);
	if (ret || handler == NULL) {
		return ret;
	}
	return gpio_pin_interrupt_configure_dt(&cfg->int1, GPIO_INT_EDGE_TO_ACTIVE);
}

static int icm42688_i2c_init_trigger(const struct device *dev)
{
	const struct icm42688_i2c_config *cfg = dev->config;
	struct icm42688_i2c_data *data = dev->data;
	int ret;

	if (cfg->int1.port == NULL) {
		return 0; /* INT1 not wired on this instance */
	}
	if (!gpio_is_ready_dt(&cfg->int1)) {
		LOG_ERR("INT1 GPIO not ready");
		return -ENODEV;
	}
	data->dev = dev;
	k_work_init(&data->work, icm42688_i2c_work);
	ret = gpio_pin_configure_dt(&cfg->int1, GPIO_INPUT);
	if (ret) {
		return ret;
	}
	gpio_init_callback(&data->gpio_cb, icm42688_i2c_gpio_cb, BIT(cfg->int1.pin));
	return gpio_add_callback(cfg->int1.port, &data->gpio_cb);
}
#endif /* CONFIG_ICM42688_I2C_TRIGGER */

static int icm42688_i2c_init(const struct device *dev)
{
	const struct icm42688_i2c_config *cfg = dev->config;
	struct icm42688_i2c_data *data = dev->data;
	int ret;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}
	ret = icm42688_i2c_check_config(cfg);
	if (ret) {
		return ret;
	}
	data->bank = UINT8_MAX; /* unknown after an MCU-only reset */
	k_msleep(POR_DELAY_MS);

	ret = icm42688_i2c_reset(dev);
	if (ret == 0) {
		ret = icm42688_i2c_configure(dev);
	}
#ifdef CONFIG_ICM42688_I2C_TRIGGER
	if (ret == 0) {
		ret = icm42688_i2c_init_trigger(dev);
	}
#endif
	if (ret) {
		LOG_ERR("init failed (%d)", ret);
	}
	return ret;
}

static DEVICE_API(sensor, icm42688_i2c_api) = {
	.sample_fetch = icm42688_i2c_sample_fetch,
	.channel_get = icm42688_i2c_channel_get,
#ifdef CONFIG_ICM42688_I2C_TRIGGER
	.trigger_set = icm42688_i2c_trigger_set,
#endif
};

#ifdef CONFIG_ICM42688_I2C_TRIGGER
#define ICM42688_I2C_INT(inst) .int1 = GPIO_DT_SPEC_INST_GET_OR(inst, int_gpios, {0}),
#else
#define ICM42688_I2C_INT(inst)
#endif

#define ICM42688_I2C_DEFINE(inst)                                                                  \
	static struct icm42688_i2c_data icm42688_i2c_data_##inst;                                  \
	static const struct icm42688_i2c_config icm42688_i2c_config_##inst = {                     \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
		ICM42688_I2C_INT(inst).accel_pwr_mode = DT_INST_PROP(inst, accel_pwr_mode),        \
		.accel_odr = DT_INST_PROP(inst, accel_odr),                                        \
		.accel_fs = DT_INST_PROP(inst, accel_fs),                                          \
		.gyro_pwr_mode = DT_INST_PROP(inst, gyro_pwr_mode),                                \
		.gyro_odr = DT_INST_PROP(inst, gyro_odr),                                          \
		.gyro_fs = DT_INST_PROP(inst, gyro_fs),                                            \
	};                                                                                         \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, icm42688_i2c_init, NULL, &icm42688_i2c_data_##inst,     \
				     &icm42688_i2c_config_##inst, POST_KERNEL,                     \
				     CONFIG_SENSOR_INIT_PRIORITY, &icm42688_i2c_api);

DT_INST_FOREACH_STATUS_OKAY(ICM42688_I2C_DEFINE)
