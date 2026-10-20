/*
 * SPDX-License-Identifier: MIT
 *
 * Human-readable names for BQ25620 status fields and CC advertisements.
 */

#include "badge/bq25620.hpp"
#include "badge/power_sense.hpp"

namespace badge
{

const char *bq25620::chg_stat_name(uint8_t stat)
{
	static const char *const names[] = {"not charging/done", "CC charge", "CV taper",
					    "top-off"};
	return names[stat & 0x03U];
}

const char *bq25620::vbus_stat_name(uint8_t stat)
{
	static const char *const names[] = {
		"none",          "SDP 500mA",    "CDP 1.5A",   "DCP 1.5A",
		"unknown 500mA", "non-standard", "HVDCP 1.5A", "OTG"};
	return names[stat & 0x07U];
}

const char *bq25620::ts_stat_name(uint8_t stat)
{
	static const char *const names[] = {"normal", "cold/no bias", "hot",     "cool",
					    "warm",   "precool",      "prewarm", "bias fault"};
	return names[stat & 0x07U];
}

const char *power::cc_advert_name(CcAdvert cc)
{
	switch (cc) {
	case CcAdvert::kNone:
		return "none";
	case CcAdvert::kDefault:
		return "default";
	case CcAdvert::kHigh:
		return "1.5A/3.0A";
	}
	return "?";
}

} // namespace badge
