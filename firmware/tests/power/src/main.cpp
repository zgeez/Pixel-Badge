/* SPDX-License-Identifier: MIT */
#include "badge/battery_level.hpp"
#include "badge/battery_log.hpp"
#include "badge/bq25620.hpp"
#include "badge/power_sense.hpp"
#include <zephyr/ztest.h>
#include <math.h>
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

ZTEST(power, test_ts_zones)
{
	/* Board 1 at about 26 degC read TS 48.6 % and TS_STAT prewarm (0x06). */
	zassert_true(chg::ts_allows_charge(chg::ts_stat(0x06)), "prewarm");
	zassert_true(chg::ts_allows_charge(0), "normal");
	zassert_true(chg::ts_allows_charge(5), "precool");
	const uint8_t blocked[] = {1, 2, 3, 4, 7}; /* cold, hot, cool, warm, bias fault */
	for (uint8_t zone : blocked) {
		zassert_false(chg::ts_allows_charge(zone), "zone %u", zone);
	}
}

ZTEST(power, test_sense_scaling)
{
	zassert_equal(power::vbat_from_pin_mv(2100), 4200);
	/* VLED 4772 mV -> 4.7/204.7 -> 109.6 mV at the pin. */
	zassert_within(power::vled_from_pin_mv(110), 4790, 5);
	/* CC: 12/112. 0.746 V at CC -> 80 mV at the pin. */
	zassert_within(power::cc_from_pin_mv(80), 746, 1);
}

ZTEST(power, test_vled_off_verdict)
{
	using power::VledOff;
	zassert_equal(power::vled_off_verdict(0, 100), VledOff::kOff);
	zassert_equal(power::vled_off_verdict(power::kVledOffMv, 100), VledOff::kOff);
	/* Still charged just after a quick reset (board 1: 958-2264 mV). */
	zassert_equal(power::vled_off_verdict(2264, 110), VledOff::kDraining);
	zassert_equal(power::vled_off_verdict(501, power::kVledDrainMs - 1), VledOff::kDraining);
	/* Drained late is still a pass; present after the drain time is not. */
	zassert_equal(power::vled_off_verdict(300, power::kVledDrainMs + 5000), VledOff::kOff);
	zassert_equal(power::vled_off_verdict(501, power::kVledDrainMs), VledOff::kStuck);
	zassert_equal(power::vled_off_verdict(4790, 120000), VledOff::kStuck);
}

ZTEST(power, test_battery_level)
{
	using power::BatteryLevel;
	using power::next_battery_level;
	zassert_equal(power::battery_band(4200), BatteryLevel::kFull);
	zassert_equal(power::battery_band(power::kLevelFullMv - 1), BatteryLevel::kHigh);
	zassert_equal(power::battery_band(3800), BatteryLevel::kHigh);
	zassert_equal(power::battery_band(3600), BatteryLevel::kHalf);
	zassert_equal(power::battery_band(3500), BatteryLevel::kLow);
	zassert_equal(power::battery_band(3450), BatteryLevel::kEmpty);
	/* The first reading sets the level directly. */
	zassert_equal(next_battery_level(3800, BatteryLevel::kUnknown), BatteryLevel::kHigh);
	/* Just past an edge, either way, keeps the shown level. */
	zassert_equal(next_battery_level(3700, BatteryLevel::kHalf), BatteryLevel::kHalf);
	zassert_equal(next_battery_level(3680, BatteryLevel::kHigh), BatteryLevel::kHigh);
	/* Past the edge by the hysteresis, it moves. */
	zassert_equal(next_battery_level(3720, BatteryLevel::kHalf), BatteryLevel::kHigh);
	zassert_equal(next_battery_level(3659, BatteryLevel::kHigh), BatteryLevel::kHalf);
	/* A large step (charger attached, pack replaced) moves several bands. */
	zassert_equal(next_battery_level(4180, BatteryLevel::kLow), BatteryLevel::kFull);
	zassert_equal(next_battery_level(3400, BatteryLevel::kFull), BatteryLevel::kEmpty);
}

ZTEST(power, test_battery_log_record)
{
	namespace blog = badge::battery_log;
	const blog::Record in{86399, 3987, 3992, -142, 255, blog::kLoaded | blog::kStart, -51, 512};
	uint8_t bytes[blog::kRecordSize];

	blog::encode(in, bytes);
	/* Little-endian words: seconds 0x0001517F, IBAT -142 = 0xFF72. */
	zassert_equal(bytes[0], 0x7F);
	zassert_equal(bytes[2], 0x01);
	zassert_equal(bytes[8], 0x72);
	zassert_equal(bytes[9], 0xFF);
	zassert_false(blog::erased(bytes));
	const blog::Record out = blog::decode(bytes);
	zassert_equal(out.seconds, in.seconds);
	zassert_equal(out.saadc_mv, in.saadc_mv);
	zassert_equal(out.charger_mv, in.charger_mv);
	zassert_equal(out.ibat_ma, in.ibat_ma);
	zassert_equal(out.ceiling, in.ceiling);
	zassert_equal(out.flags, in.flags);
	zassert_equal(out.tdie_decic, in.tdie_decic);
	zassert_equal(out.ts_permille, in.ts_permille);
	memset(bytes, 0xFF, sizeof(bytes));
	zassert_true(blog::erased(bytes));
	zassert_equal(blog::mean_ma(-1000, 7), -142);
	zassert_equal(blog::mean_ma(500, 0), 0);
}

ZTEST(power, test_battery_log_boot)
{
	namespace blog = badge::battery_log;
	using blog::Boot;
	/* Empty log, closed session or full log: wait for an unplug. */
	zassert_equal(blog::boot_action(false, 0, false, false), Boot::kIdle);
	zassert_equal(blog::boot_action(true, blog::kEnd, false, false), Boot::kIdle);
	zassert_equal(blog::boot_action(true, 0, true, false), Boot::kIdle);
	/* Reset mid-session: resume on battery, close if USB is back. */
	zassert_equal(blog::boot_action(true, blog::kLoaded, false, false), Boot::kResume);
	zassert_equal(blog::boot_action(true, blog::kStart, false, true), Boot::kClose);
}

ZTEST(power, test_charging_report_holds_through_probe)
{
	power::ChargingReport report;
	zassert_false(report.update(false, true, 0));
	zassert_true(report.update(true, true, 1000));
	/* A half-second probe pause keeps showing charging. */
	zassert_true(report.update(false, true, 1500));
	zassert_true(report.update(true, true, 1600));
	/* A real stop shows once it has lasted the hold time. */
	zassert_true(report.update(false, true, 3599));
	zassert_false(report.update(false, true, 3600));
	/* Unplugging shows at once. */
	zassert_true(report.update(true, true, 5000));
	zassert_false(report.update(true, false, 5100));
}

ZTEST(power, test_input_limit_filter)
{
	power::InputLimitFilter filter;
	/* Attach transient seen on board 1: 1500/500 alternating every 20 ms. */
	uint32_t t = 0;
	zassert_equal(filter.update(500, t), 100);
	for (int i = 0; i < 6; ++i) {
		zassert_true(filter.update(i % 2 ? 500 : 1500, t += 20) <= 500);
	}
	/* 500 mA (host configured) held 200 ms applies. */
	zassert_equal(filter.update(500, t += 20), 100);
	zassert_equal(filter.update(500, t += 100), 100);
	zassert_equal(filter.update(500, t += 100), 500);
	/* A DCP must hold too; a brief 1500 never applies. */
	zassert_equal(filter.update(1500, t += 20), 500);
	zassert_equal(filter.update(500, t += 20), 500);
	zassert_equal(filter.update(1500, t += 20), 500);
	zassert_equal(filter.update(1500, t += power::kInputRaiseHoldMs), 1500);
	/* Lowering is immediate (unplug, or the source lowers its CC level). */
	zassert_equal(filter.update(100, t += 20), 100);
}

/* TS as a fraction of REGN for the board NTC network at t degC. */
static int ts_permille_at(double t)
{
	const double r = 10000.0 * exp(3380.0 * (1.0 / (t + 273.15) - 1.0 / 298.15));
	return static_cast<int>(1000.0 * r / (10000.0 + r) + 0.5);
}

ZTEST(power, test_ts_charge_gate)
{
	/* The thresholds sit at about 38 and 36 degC, below the 40 degC limit
	 * and above the charger's own WARM step. */
	zassert_within(power::kTsChargeOffPermille, ts_permille_at(38.0), 1);
	zassert_within(power::kTsChargeOnPermille, ts_permille_at(36.0), 1);
	zassert_true(power::kTsChargeOffPermille > ts_permille_at(40.0));
	/* Hysteresis: off below 38.4 %, back on only at 40.0 %. */
	zassert_false(power::ts_too_warm(ts_permille_at(30.0), false));
	zassert_false(power::ts_too_warm(390, false));
	zassert_true(power::ts_too_warm(383, false));
	zassert_true(power::ts_too_warm(395, true));
	zassert_false(power::ts_too_warm(400, true));
	/* An unread TS (0) blocks charging. */
	zassert_true(power::ts_too_warm(0, false));
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
	/* docs/power.md: 1.5 A for a high CC advert or DCP/HVDCP, 500 mA once a
	 * host configures the badge, else 100 mA. */
	using power::UsbHost;
	zassert_equal(power::input_limit_ma(CcAdvert::kHigh, 1, UsbHost::kNotConfigured), 1500);
	zassert_equal(power::input_limit_ma(CcAdvert::kDefault, 3, UsbHost::kNotConfigured), 1500);
	zassert_equal(power::input_limit_ma(CcAdvert::kDefault, 6, UsbHost::kNotConfigured), 1500);
	zassert_equal(power::input_limit_ma(CcAdvert::kDefault, 1, UsbHost::kNotConfigured), 100,
		      "SDP before configuration");
	zassert_equal(power::input_limit_ma(CcAdvert::kDefault, 1, UsbHost::kConfigured), 500,
		      "configured SDP");
	zassert_equal(power::input_limit_ma(CcAdvert::kDefault, 2, UsbHost::kNotConfigured), 100,
		      "CDP reverts to SDP unless enumerated");
	zassert_equal(power::input_limit_ma(CcAdvert::kDefault, 2, UsbHost::kConfigured), 500,
		      "CDP is not trusted beyond a configured SDP");
	zassert_equal(power::input_limit_ma(CcAdvert::kDefault, 1, UsbHost::kSuspended), 100,
		      "suspended: HIZ handles suspend current");
	zassert_equal(power::input_limit_ma(CcAdvert::kNone, 4, UsbHost::kConfigured), 500);
	zassert_equal(power::input_limit_ma(CcAdvert::kNone, 4, UsbHost::kNotConfigured), 100,
		      "unknown adapter");
}

ZTEST_SUITE(power, NULL, NULL, NULL, NULL, NULL);
