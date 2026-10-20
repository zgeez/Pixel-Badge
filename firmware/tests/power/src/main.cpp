/* SPDX-License-Identifier: MIT */
#include "badge/bq25620.hpp"
#include "badge/power_sense.hpp"
#include <zephyr/ztest.h>
#include <string.h>

using namespace badge;
namespace chg = badge::bq25620;

/* Expected values are the SLUSEG2D reset words and range end codes. */
ZTEST(power, test_ichg_encoding)
{
	zassert_equal(chg::decode_ichg(chg::kChargeCurrentReset), 1040);
	zassert_equal(chg::encode_ichg(1040), chg::kChargeCurrentReset);
	zassert_equal(chg::encode_ichg(480), 6 << 6);
	zassert_equal(chg::encode_ichg(500), 6 << 6, "rounds down to 80 mA steps");
	zassert_equal(chg::encode_ichg(0), 1 << 6, "clamped to 80 mA");
	zassert_equal(chg::encode_ichg(9999), 0x2C << 6, "clamped to 3520 mA");
	zassert_equal(chg::decode_ichg(chg::encode_ichg(3520)), 3520);
}

ZTEST(power, test_vreg_and_iindpm_encoding)
{
	zassert_equal(chg::decode_vreg(chg::kChargeVoltageReset), 4200);
	zassert_equal(chg::encode_vreg(4200), chg::kChargeVoltageReset);
	zassert_equal(chg::encode_vreg(3000), 0x15E << 3);
	zassert_equal(chg::encode_vreg(5000), 0x1E0 << 3);
	zassert_equal(chg::decode_iindpm(0x0A00), 3200, "IINDPM reset 0A00h = 3200 mA");
	zassert_equal(chg::encode_iindpm(100), 0x5 << 4);
	zassert_equal(chg::encode_iindpm(1500), 75 << 4);
	zassert_equal(chg::encode_iindpm(50), 0x5 << 4, "clamped to 100 mA");
}

ZTEST(power, test_ntc_field)
{
	/* NTC Control 1 resets to 25h: TH1-3 = 001, TH4-6 = 001, VSET_WARM = 01. */
	zassert_equal(chg::get_th456(0x25), 1);
	zassert_equal(chg::set_th456(0x25, 3), 0x2D, "only bits 4:2 change");
	zassert_equal(chg::get_th456(chg::set_th456(0xFF, 0)), 0);
	zassert_equal(chg::set_th456(0xFF, 0), 0xE3);
}

ZTEST(power, test_adc_decoding)
{
	/* IBUS bits 15:1, 2 mA: +4000 mA = 7D0h field, -4000 mA = 7830h field. */
	zassert_equal(chg::ibus_ma(0x7D0 << 1), 4000);
	zassert_equal(chg::ibus_ma(static_cast<uint16_t>(0x7830 << 1)), -4000);
	/* IBAT bits 15:2, 4 mA: +4000 = 3E8h, -7500 = 38ADh; 0x8000 aborted. */
	zassert_equal(chg::ibat_ma(0x3E8 << 2), 4000);
	zassert_equal(chg::ibat_ma(static_cast<uint16_t>(0x38AD << 2)), -7500);
	zassert_false(chg::ibat_valid(0x8000));
	zassert_true(chg::ibat_valid(0x0000));
	/* VBUS bits 14:2, 3.97 mV: 11B6h = 18000 mV (to within one LSB). */
	zassert_within(chg::vbus_mv(0x11B6 << 2), 18000, 4);
	/* VBAT bits 12:1, 1.99 mV: AF0h = 5572 mV. */
	zassert_within(chg::vbat_mv(0xAF0 << 1), 5572, 2);
	zassert_within(chg::vbat_mv(2010 << 1), 4000, 2);
	/* TS 0.0961 %: 3FFh = 98.31 %. TDIE 0.5 C: FB0h = -40 C, 118h = 140 C. */
	zassert_within(chg::ts_permille(0x3FF), 983, 1);
	zassert_equal(chg::tdie_decic(0xFB0), -400);
	zassert_equal(chg::tdie_decic(0x118), 1400);
}

ZTEST(power, test_status_fields)
{
	zassert_true(chg::is_bq25620(0x02), "reset part info 02h: PN 0, rev 2");
	zassert_false(chg::is_bq25620(0x0A), "PN 1 = BQ25622");
	zassert_equal(chg::chg_stat(0x08), 1);
	zassert_equal(chg::vbus_stat(0x03), static_cast<uint8_t>(chg::VbusStat::kDcp));
	zassert_equal(chg::ts_stat(0x02), 2);
	zassert_false(chg::any_fault(0x07), "TS zone alone is not a fault bit");
	zassert_true(chg::any_fault(0x80));
	zassert_true(chg::any_fault(0x08), "thermal shutdown");
	zassert_equal(strcmp(chg::vbus_stat_name(3), "DCP 1.5A"), 0);
	zassert_equal(strcmp(chg::ts_stat_name(0), "normal"), 0);
}

ZTEST(power, test_sense_scaling)
{
	zassert_equal(power::vbat_from_pin_mv(2100), 4200);
	/* VLED 4772 mV -> 4.7/204.7 -> 109.6 mV at the pin. */
	zassert_within(power::vled_from_pin_mv(110), 4790, 5);
	/* CC: 12/112. 0.746 V at CC -> 80 mV at the pin. */
	zassert_within(power::cc_from_pin_mv(80), 746, 1);
}

ZTEST(power, test_cc_policy)
{
	using power::CcAdvert;

	zassert_equal(power::classify_cc_pin_mv(5), CcAdvert::kNone);
	zassert_equal(power::classify_cc_pin_mv(40), CcAdvert::kDefault);
	zassert_equal(power::classify_cc_pin_mv(70), CcAdvert::kDefault, "band -> lower");
	zassert_equal(power::classify_cc_pin_mv(80), CcAdvert::kHigh);
	zassert_equal(power::classify_cc_pin_mv(150), CcAdvert::kHigh);
	zassert_equal(power::active_cc_pin_mv(10, 90), 90);
	/* docs/power.md: 1.5 A for a high CC advert or DCP/CDP/HVDCP, else 100 mA. */
	zassert_equal(power::input_limit_ma(CcAdvert::kHigh, 1, false), 1500);
	zassert_equal(power::input_limit_ma(CcAdvert::kDefault, 3, false), 1500);
	zassert_equal(power::input_limit_ma(CcAdvert::kDefault, 2, false), 100,
		      "CDP reverts to SDP unless enumerated");
	zassert_equal(power::input_limit_ma(CcAdvert::kDefault, 2, true), 1500);
	zassert_equal(power::input_limit_ma(CcAdvert::kDefault, 1, true), 100,
		      "SDP stays at the pre-configuration limit here");
	zassert_equal(power::input_limit_ma(CcAdvert::kDefault, 6, false), 1500);
	zassert_equal(power::input_limit_ma(CcAdvert::kDefault, 1, false), 100,
		      "SDP before enumeration");
	zassert_equal(power::input_limit_ma(CcAdvert::kNone, 4, false), 100, "unknown adapter");
}

ZTEST_SUITE(power, NULL, NULL, NULL, NULL, NULL);
