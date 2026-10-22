/*
 * SPDX-License-Identifier: MIT
 *
 * Rev A power sensing: divider scaling for the SAADC inputs, USB-C CC
 * advertisement classification and the bring-up input-current policy.
 * Pure functions; values come from the Rev A schematic and docs/power.md.
 */

#pragma once

#include <stdint.h>

namespace badge::power
{

/* VBAT_SENSE: R220 47k / R221 47k behind the Q201 switch (ratio 1/2). */
constexpr int vbat_from_pin_mv(int pin_mv)
{
	return pin_mv * 2;
}

/* VLED_SENSE: R216 200k / R217 4.7k (ratio 4.7/204.7). */
constexpr int vled_from_pin_mv(int pin_mv)
{
	return pin_mv * 2047 / 47;
}

/* Board NTC on the charger's TS pin: RT1 10 kOhm from REGN, 10 kOhm
 * B25/50 3380 K NTC to GND, no RT2 (docs/power.md), so TS falls as the board
 * warms. One pack specification limits charging to 0-40 degC, and the
 * charger's own HOT threshold sits at 41.7-44.5 degC, so firmware stops
 * charging below TS 38.4 % of REGN (38.0 degC nominal, 37.3-38.7 degC with
 * 1 % RT1, R25 and B) and allows it again at 40.0 % (36.1 degC). */
constexpr int kTsChargeOffPermille = 384;
constexpr int kTsChargeOnPermille = 400;

/* True while the board is too warm to charge. An unread TS (0) counts as too
 * warm. */
constexpr bool ts_too_warm(int ts_permille, bool was_too_warm)
{
	return ts_permille < (was_too_warm ? kTsChargeOnPermille : kTsChargeOffPermille);
}

/* CC1/CC2_SENSE: CC -> 100k (R303/R306) -> pass FET -> pin, 12k to GND
 * (R304/R307), ratio 12/112. */
constexpr int cc_from_pin_mv(int pin_mv)
{
	return pin_mv * 112 / 12;
}

/* Source advertisement seen on the active CC pin (USB Type-C R2.5
 * Table 4-36 sink thresholds, scaled to the SAADC pin in docs/power.md:
 * Default <= 66 mV, 1.5 A >= 80 mV at the pin; the band between is treated
 * as the lower advertisement). */
enum class CcAdvert : uint8_t {
	kNone,    /* below vRd-Connect: nothing attached on CC */
	kDefault, /* USB default power */
	kHigh,    /* 1.5 A or 3.0 A */
};

constexpr int kCcConnectPinMv = 19; /* vRd-Connect 0.2 V x 12/112 = 21 mV, minus tolerance */
constexpr int kCcHighPinMv = 80;

constexpr CcAdvert classify_cc_pin_mv(int pin_mv)
{
	if (pin_mv < kCcConnectPinMv) {
		return CcAdvert::kNone;
	}
	return pin_mv >= kCcHighPinMv ? CcAdvert::kHigh : CcAdvert::kDefault;
}

/* The higher of the two CC readings is the active CC (docs/power.md). */
constexpr int active_cc_pin_mv(int cc1_pin_mv, int cc2_pin_mv)
{
	return cc1_pin_mv > cc2_pin_mv ? cc1_pin_mv : cc2_pin_mv;
}

/* What the USB host has done with the badge (usb_device.h). */
enum class UsbHost : uint8_t {
	kNotConfigured,
	kConfigured, /* host selected the 500 mA configuration */
	kSuspended,  /* host suspended the bus: the charger goes to HIZ instead */
};

/* Input current limit policy (docs/power.md): 1.5 A for a 1.5/3.0 A CC
 * advertisement or a BC1.2 DCP/HVDCP port; 500 mA once a host has configured
 * the badge; otherwise 100 mA. A CDP is treated as an SDP: it allows 1.5 A
 * only if the device enumerated within 2.5 s of attach (SLUSEG2D 8.3.3.3),
 * which the badge cannot guarantee when the attach also boots it. vbus_stat
 * is the BQ25620 VBUS_STAT field. */
constexpr int input_limit_ma(CcAdvert cc, uint8_t vbus_stat, UsbHost host)
{
	if (cc == CcAdvert::kHigh || vbus_stat == 3 || vbus_stat == 6) {
		return 1500;
	}
	return host == UsbHost::kConfigured ? 500 : 100;
}

const char *cc_advert_name(CcAdvert cc);

} // namespace badge::power
