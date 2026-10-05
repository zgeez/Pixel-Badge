/* SPDX-License-Identifier: MIT */

#ifndef ICM42688_EMUL_H_
#define ICM42688_EMUL_H_

#include <stdint.h>
#include <zephyr/drivers/emul.h>

uint8_t icm42688_emul_get(const struct emul *target, uint8_t bank, uint8_t reg);
void icm42688_emul_set(const struct emul *target, uint8_t bank, uint8_t reg, uint8_t val);
unsigned int icm42688_emul_resets(const struct emul *target);

#endif /* ICM42688_EMUL_H_ */
