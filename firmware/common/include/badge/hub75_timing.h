/* SPDX-License-Identifier: MIT */
#ifndef BADGE_HUB75_TIMING_H
#define BADGE_HUB75_TIMING_H
#include <stdint.h>
/* 16MHz timer. Hardware compare edges bracket each pulse. */
static inline uint32_t badge_gray_on_ticks(unsigned plane, uint8_t brightness)
{
	const uint32_t ticks = ((4U << plane) * brightness) / 255U;
	return ticks == 0 && brightness != 0 ? 1 : ticks;
}
static inline uint32_t badge_gray_slot_ticks(uint32_t on)
{
	const uint32_t end = 16U + on + 32U;
	return end < 256U ? 256U : end;
}
/* Rev A (reva_matrix.c): 2 us setup + on-time + 2 us blank, at least 16 us. */
static inline uint32_t badge_reva_slot_ticks(uint32_t on)
{
	return on + 64U > 256U ? on + 64U : 256U;
}
#endif
