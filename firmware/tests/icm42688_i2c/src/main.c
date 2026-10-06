/* SPDX-License-Identifier: MIT */

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/dt-bindings/sensor/icm42688.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "icm4268x_reg.h"
#include "icm42688_emul.h"

#define B0(reg) ((uint8_t)FIELD_GET(REG_ADDRESS_MASK, (reg)))

#define IMU_NODE DT_NODELABEL(imu)

static const struct device *const imu = DEVICE_DT_GET(IMU_NODE);
static const struct device *const bad_imu = DEVICE_DT_GET(DT_NODELABEL(bad_imu));
static const struct device *const lp_imu = DEVICE_DT_GET(DT_NODELABEL(lp_imu));
static const struct emul *const emul = EMUL_DT_GET(IMU_NODE);
static const struct emul *const lp_emul = EMUL_DT_GET(DT_NODELABEL(lp_imu));
static const struct gpio_dt_spec int1 = GPIO_DT_SPEC_GET(IMU_NODE, int_gpios);

static K_SEM_DEFINE(drdy_sem, 0, 1);

static void set_be16(uint8_t reg, int16_t v)
{
	icm42688_emul_set(emul, 0, reg, (uint8_t)((uint16_t)v >> 8));
	icm42688_emul_set(emul, 0, reg + 1, (uint8_t)v);
}

static double value(const struct sensor_value *v)
{
	return sensor_value_to_double(v);
}

ZTEST(icm42688_i2c, test_init_and_i2c_mode)
{
	zassert_true(device_is_ready(imu));
	zassert_equal(icm42688_emul_resets(emul), 1, "one soft reset");

	/* DS-000347 §12.3 I2C settings and §12.6 INT_ASYNC_RESET = 0. */
	zassert_equal(icm42688_emul_get(emul, 1, 0x7C), 0x5C, "I3C SDR/DDR disabled");
	zassert_equal(icm42688_emul_get(emul, 1, 0x7A), 0x83, "I3C_BUS_MODE clear, rest kept");
	zassert_equal(icm42688_emul_get(emul, 0, B0(REG_DRIVE_CONFIG)), 0x09);
	zassert_equal(icm42688_emul_get(emul, 0, B0(REG_INTF_CONFIG0)), 0x32, "SPI disabled");
	zassert_equal(icm42688_emul_get(emul, 0, B0(REG_INT_CONFIG)), 0x03);
	zassert_equal(icm42688_emul_get(emul, 0, B0(REG_INT_CONFIG1)), 0x00);

	zassert_equal(icm42688_emul_get(emul, 0, B0(REG_PWR_MGMT0)), 0x0F, "accel+gyro LN");
	zassert_equal(icm42688_emul_get(emul, 0, B0(REG_ACCEL_CONFIG0)),
		      (ICM42688_DT_ACCEL_FS_2 << 5) | ICM42688_DT_ACCEL_ODR_100);
	zassert_equal(icm42688_emul_get(emul, 0, B0(REG_GYRO_CONFIG0)),
		      (ICM42688_DT_GYRO_FS_2000 << 5) | ICM42688_DT_GYRO_ODR_200);
	zassert_false(icm42688_emul_config_while_on(emul), "ODR set before power-on");
}

ZTEST(icm42688_i2c, test_wrong_identity_rejected)
{
	zassert_false(device_is_ready(bad_imu));
}

ZTEST(icm42688_i2c, test_low_power_startup)
{
	zassert_true(device_is_ready(lp_imu));
	zassert_equal(icm42688_emul_get(lp_emul, 1, 0x7A), 0x83, "I3C_BUS_MODE cleared");
	zassert_equal(icm42688_emul_get(lp_emul, 0, B0(REG_PWR_MGMT0)), ICM42688_DT_ACCEL_LP,
		      "accel LP, gyro off");
	zassert_equal(icm42688_emul_get(lp_emul, 0, B0(REG_ACCEL_CONFIG0)),
		      (ICM42688_DT_ACCEL_FS_2 << 5) | ICM42688_DT_ACCEL_ODR_50);
	zassert_false(icm42688_emul_config_while_on(lp_emul), "ODR set before power-on");
}

ZTEST(icm42688_i2c, test_unsupported_odr_rejected)
{
	/* Rejected before any bus access, so the parts are never reset. */
	zassert_false(device_is_ready(DEVICE_DT_GET(DT_NODELABEL(lp_fast_imu))));
	zassert_equal(icm42688_emul_resets(EMUL_DT_GET(DT_NODELABEL(lp_fast_imu))), 0);
	zassert_false(device_is_ready(DEVICE_DT_GET(DT_NODELABEL(ln_slow_imu))));
	zassert_equal(icm42688_emul_resets(EMUL_DT_GET(DT_NODELABEL(ln_slow_imu))), 0);
}

ZTEST(icm42688_i2c, test_invalid_samples)
{
	struct sensor_value v[3];

	/* Everything invalid, as from reset until the first sample or with both
	 * sensors off.
	 */
	for (uint8_t r = B0(REG_TEMP_DATA1); r <= B0(REG_GYRO_DATA_Z0); r += 2) {
		set_be16(r, INT16_MIN);
	}
	zassert_equal(sensor_sample_fetch(imu), -EAGAIN);
	zassert_equal(sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, v), -ENODATA);
	zassert_equal(sensor_channel_get(imu, SENSOR_CHAN_DIE_TEMP, v), -ENODATA);

	/* Accel and temperature sampled; gyro off or still starting up. */
	set_be16(B0(REG_TEMP_DATA1), 0);
	set_be16(B0(REG_TEMP_DATA1) + 2, 0);
	set_be16(B0(REG_TEMP_DATA1) + 4, 0);
	set_be16(B0(REG_TEMP_DATA1) + 6, 16384);
	zassert_ok(sensor_sample_fetch(imu));
	zassert_ok(sensor_channel_get(imu, SENSOR_CHAN_ACCEL_Z, v));
	zassert_within(value(&v[0]), 9.80665, 0.001);
	zassert_equal(sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, v), -ENODATA);
	zassert_equal(sensor_channel_get(imu, SENSOR_CHAN_GYRO_X, v), -ENODATA);
	zassert_equal(sensor_sample_fetch_chan(imu, SENSOR_CHAN_GYRO_XYZ), -EAGAIN);
	zassert_ok(sensor_sample_fetch_chan(imu, SENSOR_CHAN_ACCEL_XYZ));
}

ZTEST(icm42688_i2c, test_conversion)
{
	struct sensor_value v[3];

	set_be16(B0(REG_TEMP_DATA1), 1325);       /* ~ +10 C over 25 C */
	set_be16(B0(REG_TEMP_DATA1) + 2, 16384);  /* +1 g at +/-2 g */
	set_be16(B0(REG_TEMP_DATA1) + 4, -16384); /* -1 g */
	set_be16(B0(REG_TEMP_DATA1) + 6, 0);
	set_be16(B0(REG_TEMP_DATA1) + 8, 16384); /* +1000 dps at +/-2000 dps */
	set_be16(B0(REG_TEMP_DATA1) + 10, 0);
	set_be16(B0(REG_TEMP_DATA1) + 12, -32766); /* most negative valid sample */

	zassert_ok(sensor_sample_fetch(imu));

	zassert_ok(sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, v));
	zassert_within(value(&v[0]), 9.80665, 0.001);
	zassert_within(value(&v[1]), -9.80665, 0.001);
	zassert_within(value(&v[2]), 0.0, 0.001);

	zassert_ok(sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, v));
	zassert_within(value(&v[0]), 17.4533, 0.001);
	zassert_within(value(&v[2]), -34.9045, 0.001);

	zassert_ok(sensor_channel_get(imu, SENSOR_CHAN_DIE_TEMP, v));
	zassert_within(value(&v[0]), 35.0, 0.01);
}

static void on_drdy(const struct device *dev, const struct sensor_trigger *trig)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(trig);
	k_sem_give(&drdy_sem);
}

ZTEST(icm42688_i2c, test_data_ready_trigger)
{
	static const struct sensor_trigger drdy = {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ALL,
	};

	zassert_ok(sensor_trigger_set(imu, &drdy, on_drdy));
	zassert_equal(icm42688_emul_get(emul, 0, B0(REG_INT_SOURCE0)), BIT_UI_DRDY_INT1_EN);

	icm42688_emul_set(emul, 0, B0(REG_INT_STATUS), BIT_DATA_RDY_INT);
	zassert_ok(gpio_emul_input_set(int1.port, int1.pin, 1));
	zassert_ok(k_sem_take(&drdy_sem, K_MSEC(100)), "handler not called");
	zassert_equal(icm42688_emul_get(emul, 0, B0(REG_INT_STATUS)), 0, "status cleared");
	zassert_ok(gpio_emul_input_set(int1.port, int1.pin, 0));

	zassert_ok(sensor_trigger_set(imu, &drdy, NULL));
	zassert_equal(icm42688_emul_get(emul, 0, B0(REG_INT_SOURCE0)), 0);
}

ZTEST(icm42688_i2c, test_unsupported_trigger)
{
	static const struct sensor_trigger motion = {
		.type = SENSOR_TRIG_MOTION,
		.chan = SENSOR_CHAN_ALL,
	};

	zassert_equal(sensor_trigger_set(imu, &motion, on_drdy), -ENOTSUP);
}

ZTEST_SUITE(icm42688_i2c, NULL, NULL, NULL, NULL, NULL);
