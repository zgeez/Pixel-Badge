/*
 * SPDX-License-Identifier: MIT
 *
 * Coarse battery level for the app, from the pack voltage. A 1S LiPo's
 * voltage gives only a rough state of charge, so the badge reports five bands
 * instead of a percentage. The band edges come from one board 1 discharge of
 * the PKCELL LP785060 (2026-11-01, QR at 50 % brightness, about 140 mA,
 * loaded SAADC readings): about 12, 35, 60 and 85 % of the charge drawn
 * before the matrix shed. Brighter content sags the reading lower and shows
 * a lower band; a reading near rest (charging probe) shows a higher one.
 */

#pragma once

#include <stdint.h>

namespace badge::power
{

enum class BatteryLevel : uint8_t {
	kUnknown = 0, /* no usable reading yet */
	kEmpty = 1,   /* below kLevelLowMv, or the matrix was shed */
	kLow = 2,
	kHalf = 3,
	kHigh = 4,
	kFull = 5,
};

constexpr int kLevelLowMv = 3485;
constexpr int kLevelHalfMv = 3560;
constexpr int kLevelHighMv = 3690;
constexpr int kLevelFullMv = 3920;
/* A level changes only once the voltage is this far past the band edge, so
 * noise and small load steps do not make it flicker. */
constexpr int kLevelHysteresisMv = 30;

constexpr BatteryLevel battery_band(int mv)
{
	return mv >= kLevelFullMv   ? BatteryLevel::kFull
	       : mv >= kLevelHighMv ? BatteryLevel::kHigh
	       : mv >= kLevelHalfMv ? BatteryLevel::kHalf
	       : mv >= kLevelLowMv  ? BatteryLevel::kLow
				    : BatteryLevel::kEmpty;
}

/* The level after one reading, given the level shown before. */
constexpr BatteryLevel next_battery_level(int mv, BatteryLevel previous)
{
	const BatteryLevel band = battery_band(mv);

	if (previous == BatteryLevel::kUnknown || band == previous) {
		return band;
	}
	const BatteryLevel past_edge =
		battery_band(band > previous ? mv - kLevelHysteresisMv : mv + kLevelHysteresisMv);

	return past_edge == previous ? previous : past_edge;
}

/* The pack-presence probe pauses charging for about half a second every
 * 30 s. The reported state keeps showing charging through such a pause and
 * shows a stop once it has lasted kChargingStopHoldMs, or at once when USB
 * is gone. */
constexpr uint32_t kChargingStopHoldMs = 2000;

class ChargingReport
{
public:
	bool update(bool charging, bool usb, uint32_t now_ms)
	{
		if (charging && usb) {
			shown_ = true;
			since_ = now_ms;
		} else if (!usb || now_ms - since_ >= kChargingStopHoldMs) {
			shown_ = false;
		}
		return shown_;
	}

private:
	bool shown_ = false;
	uint32_t since_ = 0;
};

} // namespace badge::power
