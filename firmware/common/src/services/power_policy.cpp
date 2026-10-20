/* SPDX-License-Identifier: MIT */
#include "badge/power_policy.hpp"
#include "badge/hub75_timing.h"

namespace badge::power
{
namespace
{
/* Input-current estimate (docs/power.md, Battery discharge limit). PROVISIONAL
 * until power measurements (validation section 4) calibrate it. */
constexpr int kColumns = 32;
constexpr int kColumnMaxUa = 19400;      /* 1.08 x 18.0 mA R_IREF setting */
constexpr int kVledMaxMv = 4890;         /* 4.77 V setting, VREF high */
constexpr int kBoostEffPermille = 800;   /* TPS61023, as in the discharge limit */
constexpr int kLogicMw = 255;            /* I_logic ~60 mA at a 4.25 V SYS */
constexpr int kVbusMinMv = 4400;         /* USB 2.0 minimum at the device */
constexpr int kChargerEffPermille = 850; /* BQ25620 buck: assumed, below TI's curves */
constexpr int kLimitMinPermille = 900;   /* SLUSEG2D IINDPM_ACC: 450/810/1350 mA min */
constexpr int kMarginPermille = 800;     /* PROPOSED 20 % margin */

bool after(uint32_t now, uint32_t at)
{
	return static_cast<int32_t>(now - at) >= 0;
}
} // namespace

unsigned matrix_duty_permille(uint8_t level)
{
	uint32_t on = 0, slots = 0;

	for (unsigned plane = 0; plane < 8; ++plane) {
		const uint32_t ticks = badge_gray_on_ticks(plane, level);

		on += ticks;
		slots += badge_reva_slot_ticks(ticks);
	}
	return level == 0 ? 0 : static_cast<unsigned>((on * 1000U + slots - 1) / slots);
}

int estimated_input_ma(uint8_t level)
{
	/* LED power at the boost input, then the whole system through the charger. */
	const int64_t led_uw = static_cast<int64_t>(kColumns) * kColumnMaxUa * kVledMaxMv / 1000 *
			       matrix_duty_permille(level) / kBoostEffPermille;
	const int64_t system_uw = led_uw + static_cast<int64_t>(kLogicMw) * 1000;

	return static_cast<int>((system_uw * 1000 / kChargerEffPermille + kVbusMinMv - 1) /
				kVbusMinMv);
}

uint8_t input_budget_level(int limit_ma)
{
	const int budget = limit_ma * kLimitMinPermille / 1000 * kMarginPermille / 1000;
	uint8_t best = 0;

	for (int level = 1; level <= 255; ++level) {
		if (estimated_input_ma(static_cast<uint8_t>(level)) <= budget) {
			best = static_cast<uint8_t>(level);
		}
	}
	return best;
}

void LoadShedPolicy::update(const Sample &s)
{
	const uint32_t now = s.now_ms;

	if (!s.valid) {
		/* Missing, stale or invalid: keep the matrix off until a good reading. */
		valid_ = false;
		low_power_ = false;
		below_shed_ = 0;
		below_low_ = 0;
		battery_high_ = false;
		return;
	}
	valid_ = true;
	if (!s.usb_present && !s.loaded && s.vbat_mv < kLowPowerMv) {
		below_low_ += below_low_ < kShedSamples ? 1 : 0;
	} else {
		below_low_ = 0;
	}
	low_power_ = below_low_ >= kShedSamples;

	if (s.vbat_mv >= kBatteryOkMv) {
		if (!battery_high_) {
			battery_high_ = true;
			battery_high_since_ = now;
		}
	} else {
		battery_high_ = false;
	}
	const bool battery_ok = battery_high_ && now - battery_high_since_ >= kBatteryOkMs;

	if (state_ == ShedState::kNoReading) {
		/* Below the loaded shed level even unloaded: start shed. */
		capped_ = s.vbat_mv < kCapMv;
		if (s.vbat_mv < kShedMv) {
			state_ = ShedState::kShed;
			retry_at_ = now;
		} else {
			state_ = ShedState::kNormal;
		}
		return;
	}

	if (state_ == ShedState::kShed) {
		const uint8_t budget = s.usb_present && s.input_limit_ma >= kInputOkLimitMa
					       ? input_budget_level(s.input_limit_ma)
					       : 0;
		const bool input_ok =
			failures_ < kMaxFailures && budget > 0 && s.vbat_mv >= kInputOkMv;

		if (after(now, retry_at_) && (battery_ok || input_ok)) {
			reenable(now, battery_ok, battery_ok ? 255 : budget);
		}
		return;
	}

	if (s.loaded && s.vbat_mv < kShedMv) {
		if (++below_shed_ >= kShedSamples) {
			shed(now);
			return;
		}
	} else {
		below_shed_ = 0;
	}
	if (s.loaded && s.vbat_mv < kCapMv) {
		capped_ = true;
	}
	if (battery_ok) {
		capped_ = false;
		input_ceiling_ = 255;
	}
	if (attempt_ && now - enabled_at_ >= kRetryWindowMs) {
		attempt_ = false;
		failures_ = 0;
	}
	if (state_ == ShedState::kRamping && now - step_at_ >= kRampStepMs) {
		step_at_ = now;
		if (++ramp_ >= sizeof(kRampLevels) - 1) {
			state_ = ShedState::kNormal;
		}
	}
}

uint8_t LoadShedPolicy::ceiling() const
{
	if (!valid_ || state_ == ShedState::kNoReading || state_ == ShedState::kShed) {
		return 0;
	}
	uint8_t level = state_ == ShedState::kRamping ? kRampLevels[ramp_] : 255;

	if (capped_ && level > kCapLevel) {
		level = kCapLevel;
	}
	return level < input_ceiling_ ? level : input_ceiling_;
}

void LoadShedPolicy::shed(uint32_t now)
{
	state_ = ShedState::kShed;
	below_shed_ = 0;
	capped_ = true;
	if (attempt_ && now - enabled_at_ < kRetryWindowMs && failures_ < 255) {
		failures_++;
	}
	attempt_ = false;
	uint32_t backoff = 0;

	if (failures_ > 0) {
		backoff = kBackoffMs;
		for (uint8_t i = 1; i < failures_ && backoff < kBackoffMaxMs; ++i) {
			backoff *= 2;
		}
		backoff = backoff < kBackoffMaxMs ? backoff : kBackoffMaxMs;
	}
	retry_at_ = now + backoff;
}

void LoadShedPolicy::reenable(uint32_t now, bool battery_ok, uint8_t input_ceiling)
{
	state_ = ShedState::kRamping;
	ramp_ = 0;
	step_at_ = now;
	enabled_at_ = now;
	attempt_ = true;
	input_ceiling_ = input_ceiling;
	if (battery_ok) {
		capped_ = false;
	}
}

} // namespace badge::power
