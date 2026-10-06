/*
 * SPDX-License-Identifier: MIT
 *
 * Register-level ICM-42688-P emulator: five register banks selected through
 * BANK_SEL, soft reset with RESET_DONE, read-to-clear INT_STATUS, and the
 * reset values the driver relies on (TDK DS-000347 Rev 1.2). Sensor data
 * registers power up holding the invalid-sample value -32768 (§14.34).
 * The instance at address 0x69 reports WHO_AM_I 0x00 so the identity check
 * can be tested; the instance at 0x6A powers up with INTF_CONFIG4.I3C_BUS_MODE
 * set so clearing it can be tested.
 */

#define DT_DRV_COMPAT invensense_icm42688p_i2c

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>

#include "icm4268x_reg.h"
#include "icm42688_emul.h"

#define BANKS        5
#define B0(reg)      ((uint8_t)FIELD_GET(REG_ADDRESS_MASK, (reg)))
#define INTF_CONFIG4 0x7A /* bank 1 */
#define INTF_CONFIG6 0x7C /* bank 1 */

struct icm42688_emul_data {
	uint8_t regs[BANKS][256];
	uint8_t bank;
	unsigned int resets;
	bool config_while_on;
};

struct icm42688_emul_cfg {
	uint8_t who_am_i;
	uint8_t intf_config4;
};

static void power_on(const struct emul *target)
{
	const struct icm42688_emul_cfg *cfg = target->cfg;
	struct icm42688_emul_data *d = target->data;

	memset(d->regs, 0, sizeof(d->regs));
	d->bank = 0;
	d->regs[0][B0(REG_WHO_AM_I)] = cfg->who_am_i;
	d->regs[0][B0(REG_INTF_CONFIG0)] = 0x30;
	d->regs[0][B0(REG_INTF_CONFIG1)] = 0x91;
	d->regs[0][B0(REG_INT_CONFIG1)] = 0x10;
	d->regs[0][B0(REG_GYRO_CONFIG0)] = 0x06;
	d->regs[0][B0(REG_ACCEL_CONFIG0)] = 0x06;
	for (uint8_t r = B0(REG_TEMP_DATA1); r <= B0(REG_GYRO_DATA_Z0); r += 2) {
		d->regs[0][r] = 0x80; /* -32768, big-endian */
	}
	d->regs[1][INTF_CONFIG4] = cfg->intf_config4;
	d->regs[1][INTF_CONFIG6] = 0x5F;
}

static void write_reg(const struct emul *target, uint8_t reg, uint8_t val)
{
	struct icm42688_emul_data *d = target->data;

	if (reg == REG_BANK_SEL) {
		d->bank = val;
		return;
	}
	if (d->bank >= BANKS) {
		return;
	}
	if (d->bank == 0 && reg == B0(REG_DEVICE_CONFIG) && (val & BIT_SOFT_RESET_CONFIG)) {
		power_on(target);
		d->regs[0][B0(REG_INT_STATUS)] = BIT_RESET_DONE_INT;
		d->resets++;
		return;
	}
	/* ODR/full-scale writes while a sensor is on are recorded so the
	 * driver's configuration order can be checked.
	 */
	if (d->bank == 0 && (reg == B0(REG_ACCEL_CONFIG0) || reg == B0(REG_GYRO_CONFIG0)) &&
	    d->regs[0][B0(REG_PWR_MGMT0)] != 0) {
		d->config_while_on = true;
	}
	d->regs[d->bank][reg] = val;
}

static uint8_t read_reg(const struct emul *target, uint8_t reg)
{
	struct icm42688_emul_data *d = target->data;
	uint8_t val;

	if (reg == REG_BANK_SEL) {
		return d->bank;
	}
	if (d->bank >= BANKS) {
		return 0;
	}
	val = d->regs[d->bank][reg];
	if (d->bank == 0 && reg == B0(REG_INT_STATUS)) {
		d->regs[0][reg] = 0; /* read to clear */
	}
	return val;
}

static int icm42688_emul_transfer(const struct emul *target, struct i2c_msg *msgs, int num_msgs,
				  int addr)
{
	ARG_UNUSED(addr);

	if (num_msgs < 1 || (msgs[0].flags & I2C_MSG_READ) || msgs[0].len < 1) {
		return -EIO;
	}
	uint8_t reg = msgs[0].buf[0];

	if (num_msgs == 1) {
		for (uint32_t i = 1; i < msgs[0].len; i++) {
			write_reg(target, reg + i - 1, msgs[0].buf[i]);
		}
		return 0;
	}
	if (num_msgs == 2 && (msgs[1].flags & I2C_MSG_READ)) {
		for (uint32_t i = 0; i < msgs[1].len; i++) {
			msgs[1].buf[i] = read_reg(target, reg + i);
		}
		return 0;
	}
	return -EIO;
}

static const struct i2c_emul_api icm42688_emul_api = {
	.transfer = icm42688_emul_transfer,
};

uint8_t icm42688_emul_get(const struct emul *target, uint8_t bank, uint8_t reg)
{
	struct icm42688_emul_data *d = target->data;

	return d->regs[bank][reg];
}

void icm42688_emul_set(const struct emul *target, uint8_t bank, uint8_t reg, uint8_t val)
{
	struct icm42688_emul_data *d = target->data;

	d->regs[bank][reg] = val;
}

unsigned int icm42688_emul_resets(const struct emul *target)
{
	struct icm42688_emul_data *d = target->data;

	return d->resets;
}

bool icm42688_emul_config_while_on(const struct emul *target)
{
	struct icm42688_emul_data *d = target->data;

	return d->config_while_on;
}

static int icm42688_emul_init(const struct emul *target, const struct device *parent)
{
	ARG_UNUSED(parent);
	power_on(target);
	/* Leave a non-zero bank selected, as after an MCU-only reset. */
	((struct icm42688_emul_data *)target->data)->bank = 1;
	return 0;
}

#define ICM42688_EMUL(inst)                                                                        \
	static struct icm42688_emul_data icm42688_emul_data_##inst;                                \
	static const struct icm42688_emul_cfg icm42688_emul_cfg_##inst = {                         \
		.who_am_i = DT_INST_REG_ADDR(inst) == 0x69 ? 0x00 : WHO_AM_I_ICM42688,             \
		.intf_config4 = DT_INST_REG_ADDR(inst) == 0x6A ? 0xC3 : 0x83,                      \
	};                                                                                         \
	EMUL_DT_INST_DEFINE(inst, icm42688_emul_init, &icm42688_emul_data_##inst,                  \
			    &icm42688_emul_cfg_##inst, &icm42688_emul_api, NULL);

DT_INST_FOREACH_STATUS_OKAY(ICM42688_EMUL)
