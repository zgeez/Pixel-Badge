/*
 * SPDX-License-Identifier: MIT
 *
 * Rev A battery load shedding (docs/power.md, Battery-state measurement).
 * Pure state machine fed with VBAT samples; it returns the brightness ceiling
 * the matrix driver enforces (0 = LED rail off) and whether the badge should
 * enter its lowest-power state. Every threshold is PROPOSED until the selected
 * pack is characterized (validation.md section 12).
 */

#pragma once

#include <stdint.h>

namespace badge::power
{

constexpr int kCapMv = 3550;               /* loaded: cap brightness */
constexpr int kShedMv = 3400;              /* loaded, kShedSamples in a row: LED rail off */
constexpr unsigned kShedSamples = 3;       /* at 100 ms */
constexpr int kBatteryOkMv = 3700;         /* battery-qualified recovery ... */
constexpr uint32_t kBatteryOkMs = 10000;   /* ... held this long */
constexpr int kInputOkMv = 3500;           /* input-qualified recovery */
constexpr int kInputOkLimitMa = 500;       /* charger input limit at least this */
constexpr int kLowPowerMv = 3300;          /* unloaded, on battery, kShedSamples in a row: off */
constexpr uint32_t kRampStepMs = 5000;     /* controlled re-enable: one level per step */
constexpr uint32_t kRetryWindowMs = 60000; /* a shed this soon after re-enable fails it */
constexpr uint32_t kBackoffMs = 60000;     /* doubles per failure ... */
constexpr uint32_t kBackoffMaxMs = 900000; /* ... up to 15 min */
constexpr uint8_t kMaxFailures = 3;        /* then battery-qualified recovery only */
/* "<= 50 % on-time": the driver's on-time per bit plane is linear in level. */
constexpr uint8_t kCapLevel = 127;
constexpr uint8_t kRampLevels[] = {13, 64, kCapLevel, 255};

/* Upper bound of the matrix's average on-time, in per mille of scan time, at
 * a brightness level: the Rev A driver's per-plane on-time and slot length,
 * ignoring the SPI shift between slots (which only lowers the real value). */
unsigned matrix_duty_permille(uint8_t level);

/* Worst-case total USB input current (mA) with every pixel lit at level:
 * datasheet maxima and stated assumptions, not measurements (power_policy.cpp). */
int estimated_input_ma(uint8_t level);

/* Highest level whose worst-case total input stays within 80 % of the
 * charger's minimum input limit for limit_ma (IINDPM accuracy -10 %). */
uint8_t input_budget_level(int limit_ma);

struct Sample {
	bool valid = false;  /* fresh, in-range VBAT reading */
	int vbat_mv = 0;     /* protected-pack voltage from the switched divider */
	bool loaded = false; /* LED rail on while sampled */
	bool usb_present = false;
	int input_limit_ma = -1; /* confirmed IINDPM, -1 unknown */
	uint32_t now_ms = 0;
};

enum class ShedState : uint8_t {
	kNoReading, /* no valid sample yet: rail off */
	kNormal,
	kRamping, /* controlled re-enable after a shed */
	kShed,
};

class LoadShedPolicy
{
public:
	void update(const Sample &sample);
	/* Brightness ceiling for the driver; 0 keeps the LED rail off. */
	uint8_t ceiling() const;
	bool low_power() const
	{
		return low_power_;
	}
	ShedState state() const
	{
		return state_;
	}
	uint8_t failures() const
	{
		return failures_;
	}
	bool capped() const
	{
		return capped_;
	}

private:
	void shed(uint32_t now);
	void reenable(uint32_t now, bool battery_ok, uint8_t input_ceiling);
	bool follow_input(const Sample &sample, uint32_t now);

	ShedState state_ = ShedState::kNoReading;
	bool valid_ = false;
	bool low_power_ = false;
	bool capped_ = false;
	bool attempt_ = false; /* a re-enable inside its retry window */
	bool battery_high_ = false;
	bool input_basis_ = false; /* enabled on input, not battery */
	unsigned below_shed_ = 0;
	unsigned below_low_ = 0;
	uint8_t ramp_ = 0;
	uint8_t failures_ = 0;
	uint8_t input_ceiling_ = 255;
	uint32_t battery_high_since_ = 0;
	uint32_t step_at_ = 0;
	uint32_t enabled_at_ = 0;
	uint32_t retry_at_ = 0;
};

} // namespace badge::power
