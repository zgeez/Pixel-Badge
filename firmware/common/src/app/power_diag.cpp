/*
 * SPDX-License-Identifier: MIT
 *
 * Rev A power bring-up diagnostics (built when the board describes the
 * charger bus and the sense ADC channels):
 * - reset cause and 32.768 kHz clock source;
 * - SAADC rails: switched VBAT divider, VLED sense, CC1/CC2 sense;
 * - BQ25620 identity, faults, TS zone, D+/D- result and its ADC;
 * - input-current policy from CC/BC1.2 (docs/power.md) with the I2C watchdog
 *   kept alive, and TS thresholds set before any charging;
 * - charging only with CONFIG_APP_BRINGUP_CHARGE and every gate passing,
 *   at CONFIG_APP_BRINGUP_ICHG_MA;
 * - buttons, power switch and charger interrupt edges, status LED heartbeat.
 *
 * The matrix stays disarmed: this module never touches LED_EN/ARM/BLANK.
 */

#include "badge/power_diag.hpp"
#include "badge/bq25620.hpp"
#include "badge/nfc_platform.hpp"
#include "badge/power_sense.hpp"

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(power_diag, CONFIG_APP_LOG_LEVEL);

#define USER_NODE DT_PATH(zephyr_user)

/* Bring-up images only: the product image must not carry this thread. */
#if defined(CONFIG_APP_BRINGUP_CHECKS) && defined(CONFIG_I2C) && defined(CONFIG_ADC) &&            \
	DT_NODE_HAS_PROP(USER_NODE, charger_i2c_bus) &&                                            \
	DT_NODE_HAS_PROP(USER_NODE, io_channels) && DT_NODE_HAS_PROP(USER_NODE, chg_en_gpios)
#define HAVE_POWER_DIAG 1
#else
#define HAVE_POWER_DIAG 0
#endif

#if HAVE_POWER_DIAG

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#if defined(CONFIG_HWINFO)
#include <zephyr/drivers/hwinfo.h>
#endif
#include <soc.h>

namespace badge
{
namespace
{

namespace chg = bq25620;

constexpr int kTickMs = 250;
constexpr int kLogEveryTicks = 20;     /* 5 s */
constexpr int kWatchdogKickTicks = 40; /* 10 s, against the 50 s charger watchdog */
constexpr int kVbatSettleMs = 25;      /* > 10 tau of the 23.5 k / 100 nF node */
constexpr uint8_t kTh456Code = 0x3;    /* docs/power.md: TS_TH4_TH5_TH6 = 011 */

#if defined(CONFIG_APP_BRINGUP_CHARGE)
constexpr bool kChargeAllowed = true;
constexpr int kIchgMa = CONFIG_APP_BRINGUP_ICHG_MA;
#else
constexpr bool kChargeAllowed = false;
constexpr int kIchgMa = 480;
#endif

const struct device *const charger_bus = DEVICE_DT_GET(DT_PHANDLE(USER_NODE, charger_i2c_bus));
const struct adc_dt_spec adc_vbat = ADC_DT_SPEC_GET_BY_NAME(USER_NODE, vbat);
const struct adc_dt_spec adc_cc1 = ADC_DT_SPEC_GET_BY_NAME(USER_NODE, cc1);
const struct adc_dt_spec adc_cc2 = ADC_DT_SPEC_GET_BY_NAME(USER_NODE, cc2);
const struct adc_dt_spec adc_vled = ADC_DT_SPEC_GET_BY_NAME(USER_NODE, vled);
const struct gpio_dt_spec vbat_meas_en = GPIO_DT_SPEC_GET(USER_NODE, vbat_meas_en_gpios);
const struct gpio_dt_spec chg_en = GPIO_DT_SPEC_GET(USER_NODE, chg_en_gpios);
const struct gpio_dt_spec chg_int = GPIO_DT_SPEC_GET(USER_NODE, chg_int_gpios);
const struct gpio_dt_spec pwr_sw = GPIO_DT_SPEC_GET(USER_NODE, pwr_sw_gpios);
const struct gpio_dt_spec btn_mode = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
const struct gpio_dt_spec btn_action = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

bool adc_calibrated;
bool charger_present;
bool charging;
int input_limit_ma = -1;

/* ---- Charger I2C (shared TWIM0 with the IMU; serialised by the bus lock) ---- */

int reg_read(uint8_t reg, uint8_t *buf, size_t len)
{
	int err = bench_bus_lock();

	if (err == 0) {
		err = i2c_write_read(charger_bus, chg::kAddress, &reg, 1, buf, len);
		bench_bus_unlock();
	}
	return err;
}

int reg_read8(uint8_t reg, uint8_t &value)
{
	return reg_read(reg, &value, 1);
}

int reg_read16(uint8_t reg, uint16_t &value)
{
	uint8_t buf[2];
	int err = reg_read(reg, buf, sizeof(buf));

	value = static_cast<uint16_t>(buf[0] | (buf[1] << 8));
	return err;
}

int reg_write8(uint8_t reg, uint8_t value)
{
	const uint8_t buf[2] = {reg, value};
	int err = bench_bus_lock();

	if (err == 0) {
		err = i2c_write(charger_bus, buf, sizeof(buf), chg::kAddress);
		bench_bus_unlock();
	}
	return err;
}

int reg_write16(uint8_t reg, uint16_t value)
{
	const uint8_t buf[3] = {reg, static_cast<uint8_t>(value & 0xFF),
				static_cast<uint8_t>(value >> 8)};
	int err = bench_bus_lock();

	if (err == 0) {
		err = i2c_write(charger_bus, buf, sizeof(buf), chg::kAddress);
		bench_bus_unlock();
	}
	return err;
}

/* ---- SAADC ---- */

int adc_setup()
{
	const struct adc_dt_spec *const specs[] = {&adc_vbat, &adc_cc1, &adc_cc2, &adc_vled};

	for (const struct adc_dt_spec *spec : specs) {
		if (!adc_is_ready_dt(spec)) {
			return -ENODEV;
		}
		int err = adc_channel_setup_dt(spec);

		if (err != 0) {
			return err;
		}
	}
	return 0;
}

int adc_pin_mv(const struct adc_dt_spec &spec, int &mv)
{
	int16_t sample = 0;
	struct adc_sequence seq = {};

	seq.buffer = &sample;
	seq.buffer_size = sizeof(sample);
	int err = adc_sequence_init_dt(&spec, &seq);

	if (err != 0) {
		return err;
	}
	/* Offset calibration once at start-up (docs/power.md). */
	seq.calibrate = !adc_calibrated;
	err = adc_read_dt(&spec, &seq);
	if (err != 0) {
		return err;
	}
	adc_calibrated = true;
	int32_t value = sample < 0 ? 0 : sample;

	err = adc_raw_to_millivolts_dt(&spec, &value);
	mv = static_cast<int>(value);
	return err;
}

/* VBAT through the switched divider: enable, settle, sample, disable. */
int read_vbat_mv(int &mv)
{
	int pin = 0;
	int err = gpio_pin_set_dt(&vbat_meas_en, 1);

	if (err != 0) {
		return err;
	}
	k_msleep(kVbatSettleMs);
	err = adc_pin_mv(adc_vbat, pin);
	(void)gpio_pin_set_dt(&vbat_meas_en, 0);
	mv = power::vbat_from_pin_mv(pin);
	return err;
}

struct Rails {
	int vbat_mv = -1;
	int vled_mv = -1;
	int cc1_pin_mv = -1;
	int cc2_pin_mv = -1;
};

Rails read_rails()
{
	Rails r;
	int pin;

	if (read_vbat_mv(r.vbat_mv) != 0) {
		r.vbat_mv = -1;
	}
	if (adc_pin_mv(adc_vled, pin) == 0) {
		r.vled_mv = power::vled_from_pin_mv(pin);
	}
	if (adc_pin_mv(adc_cc1, pin) == 0) {
		r.cc1_pin_mv = pin;
	}
	if (adc_pin_mv(adc_cc2, pin) == 0) {
		r.cc2_pin_mv = pin;
	}
	return r;
}

/* ---- Charger state ---- */

struct ChargerState {
	uint8_t status0 = 0, status1 = 0, fault0 = 0, ntc1 = 0;
	uint16_t ichg = 0, iindpm = 0, vreg = 0;
	int vbus_mv = 0, vbat_mv = 0, vsys_mv = 0, ibus_ma = 0, ibat_ma = 0;
	int ts_permille = 0, tdie_decic = 0;
	bool ibat_valid = false;
};

int read_charger(ChargerState &s)
{
	uint16_t w = 0;
	int err;

	if ((err = reg_read8(chg::kStatus0, s.status0)) != 0 ||
	    (err = reg_read8(chg::kStatus1, s.status1)) != 0 ||
	    (err = reg_read8(chg::kFault0, s.fault0)) != 0 ||
	    (err = reg_read8(chg::kNtc1, s.ntc1)) != 0 ||
	    (err = reg_read16(chg::kChargeCurrent, s.ichg)) != 0 ||
	    (err = reg_read16(chg::kInputCurrent, s.iindpm)) != 0 ||
	    (err = reg_read16(chg::kChargeVoltage, s.vreg)) != 0) {
		return err;
	}
	if (reg_read16(chg::kVbusAdc, w) == 0) {
		s.vbus_mv = chg::vbus_mv(w);
	}
	if (reg_read16(chg::kVbatAdc, w) == 0) {
		s.vbat_mv = chg::vbat_mv(w);
	}
	if (reg_read16(chg::kVsysAdc, w) == 0) {
		s.vsys_mv = chg::vbat_mv(w);
	}
	if (reg_read16(chg::kIbusAdc, w) == 0) {
		s.ibus_ma = chg::ibus_ma(w);
	}
	if (reg_read16(chg::kIbatAdc, w) == 0) {
		s.ibat_valid = chg::ibat_valid(w);
		s.ibat_ma = chg::ibat_ma(w);
	}
	if (reg_read16(chg::kTsAdc, w) == 0) {
		s.ts_permille = chg::ts_permille(w);
	}
	if (reg_read16(chg::kTdieAdc, w) == 0) {
		s.tdie_decic = chg::tdie_decic(w);
	}
	return 0;
}

/* Settings that must be in place before CHG_EN can go high. ICHG is written
 * first; the I2C watchdog halves ICHG and clears ADC_EN on expiry, so this is
 * re-applied whenever WD_STAT is seen. */
int configure_charger()
{
	uint8_t ntc1 = 0;
	int err;

	if ((err = reg_write16(chg::kChargeCurrent, chg::encode_ichg(kIchgMa))) != 0 ||
	    (err = reg_read8(chg::kNtc1, ntc1)) != 0 ||
	    (err = reg_write8(chg::kNtc1, chg::set_th456(ntc1, kTh456Code))) != 0) {
		return err;
	}
	return reg_write8(chg::kAdcControl, chg::kAdcEnableContinuous12Bit);
}

int kick_watchdog()
{
	uint8_t ctrl1 = 0;
	int err = reg_read8(chg::kControl1, ctrl1);

	return err != 0 ? err : reg_write8(chg::kControl1, ctrl1 | chg::kWdRst);
}

/* Enforces the policy limit against the register every tick: the charger's
 * own D+/D- detection rewrites IINDPM (SDP/unknown 500 mA, DCP 1.5 A) and an
 * adapter removal resets it to 3.2 A (SLUSEG2D Table 8-11). */
int apply_input_limit(power::CcAdvert cc, uint8_t vbus_stat, uint16_t iindpm_reg)
{
	const int limit = power::input_limit_ma(cc, vbus_stat);

	if (chg::decode_iindpm(iindpm_reg) == limit) {
		input_limit_ma = limit;
		return 0;
	}
	int err = reg_write16(chg::kInputCurrent, chg::encode_iindpm(limit));

	if (err == 0) {
		LOG_INF("Charger: input limit %d -> %d mA (CC %s, D+/D- %s)",
			chg::decode_iindpm(iindpm_reg), limit, power::cc_advert_name(cc),
			chg::vbus_stat_name(vbus_stat));
		input_limit_ma = limit;
	}
	return err;
}

/* All gates for the bring-up charge (docs/bringup.md, first charge). */
bool charge_gates_pass(const ChargerState &s, const Rails &r, const char *&reason)
{
	if (!kChargeAllowed) {
		reason = "disabled in this image (CONFIG_APP_BRINGUP_CHARGE=n)";
	} else if (chg::vbus_stat(s.status1) == 0 && s.vbus_mv < 4300) {
		reason = "no VBUS";
	} else if (chg::any_fault(s.fault0)) {
		reason = "charger fault";
	} else if (chg::ts_stat(s.fault0) != 0) {
		reason = "TS not normal";
	} else if (chg::get_th456(s.ntc1) != kTh456Code) {
		reason = "TS thresholds not set";
	} else if (chg::decode_ichg(s.ichg) != chg::decode_ichg(chg::encode_ichg(kIchgMa))) {
		reason = "ICHG readback mismatch";
	} else if (chg::decode_iindpm(s.iindpm) != input_limit_ma) {
		reason = "input limit not at policy";
	} else if (chg::decode_vreg(s.vreg) != 4200) {
		reason = "VREG not 4200 mV";
	} else if (s.vbat_mv < 3000 || s.vbat_mv > 4250) {
		reason = "VBAT out of range / no pack";
	} else if (r.vbat_mv < s.vbat_mv - 200 || r.vbat_mv > s.vbat_mv + 200) {
		/* The board divider and the charger ADC must agree on the pack. */
		reason = "VBAT readings disagree (SAADC vs charger)";
	} else {
		reason = "";
		return true;
	}
	return false;
}

const char *off_reason = "";

void set_charging(bool on, const char *reason)
{
	if (on != charging) {
		(void)gpio_pin_set_dt(&chg_en, on ? 1 : 0);
		charging = on;
		if (on) {
			LOG_WRN("Charger: CHARGING ENABLED at ICHG %d mA; watch the cell and "
				"charger "
				"temperature",
				kIchgMa);
		}
	}
	/* Log each new reason the charge is held off, once. */
	if (!on && strcmp(reason, off_reason) != 0) {
		LOG_INF("Charger: charging off (%s)", reason);
	}
	off_reason = on ? "" : reason;
}

void log_state(const ChargerState &s, const Rails &r)
{
	const power::CcAdvert cc =
		power::classify_cc_pin_mv(power::active_cc_pin_mv(r.cc1_pin_mv, r.cc2_pin_mv));

	LOG_INF("Rails: VBAT(SAADC) %d mV, VLED %d mV, CC1 %d mV, CC2 %d mV (pin), CC %s",
		r.vbat_mv, r.vled_mv, power::cc_from_pin_mv(r.cc1_pin_mv),
		power::cc_from_pin_mv(r.cc2_pin_mv), power::cc_advert_name(cc));
	if (!charger_present) {
		return;
	}
	LOG_INF("Charger: VBUS %d mV IBUS %d mA | VBAT %d mV IBAT %d%s mA | VSYS %d mV | "
		"TS %d.%d %% | TDIE %s%d.%d C",
		s.vbus_mv, s.ibus_ma, s.vbat_mv, s.ibat_ma, s.ibat_valid ? "" : "?", s.vsys_mv,
		s.ts_permille / 10, s.ts_permille % 10, s.tdie_decic < 0 ? "-" : "",
		(s.tdie_decic < 0 ? -s.tdie_decic : s.tdie_decic) / 10,
		(s.tdie_decic < 0 ? -s.tdie_decic : s.tdie_decic) % 10);
	LOG_INF("Charger: %s, D+/D- %s, TS %s, fault 0x%02x, status0 0x%02x, ICHG %d mA, "
		"IINDPM %d mA, VREG %d mV, CHG_EN %d%s%s",
		chg::chg_stat_name(chg::chg_stat(s.status1)),
		chg::vbus_stat_name(chg::vbus_stat(s.status1)),
		chg::ts_stat_name(chg::ts_stat(s.fault0)), s.fault0, s.status0,
		chg::decode_ichg(s.ichg), chg::decode_iindpm(s.iindpm), chg::decode_vreg(s.vreg),
		charging ? 1 : 0, charging ? "" : " - ", charging ? "" : off_reason);
}

struct Inputs {
	int mode = -1, action = -1, sw = -1;
};

/* /INT is a 256 us pulse (SLUSEG2D pin table), too short to poll; the flag
 * registers latch each event until read, so read them every tick. */
void log_charger_flags()
{
	uint8_t flag0 = 0, flag1 = 0, fault_flag0 = 0;

	if (reg_read8(chg::kFlag0, flag0) != 0 || reg_read8(chg::kFlag1, flag1) != 0 ||
	    reg_read8(chg::kFaultFlag0, fault_flag0) != 0) {
		return;
	}
	if (flag0 != 0 || flag1 != 0 || fault_flag0 != 0) {
		LOG_INF("Charger event: flag0 0x%02x flag1 0x%02x fault_flag0 0x%02x", flag0, flag1,
			fault_flag0);
	}
}

void log_input_edges(Inputs &last)
{
	const Inputs now = {gpio_pin_get_dt(&btn_mode), gpio_pin_get_dt(&btn_action),
			    gpio_pin_get_dt(&pwr_sw)};

	if (now.mode != last.mode) {
		LOG_INF("Button MODE %s", now.mode ? "pressed" : "released");
	}
	if (now.action != last.action) {
		LOG_INF("Button ACTION %s", now.action ? "pressed" : "released");
	}
	if (now.sw != last.sw) {
		LOG_INF("Power switch %s", now.sw ? "ON" : "OFF");
	}
	last = now;
}

void power_thread(void *, void *, void *)
{
	Inputs inputs;
	unsigned tick = 0;

	while (true) {
		ChargerState s;
		const Rails r = read_rails();
		const char *reason =
			charger_present ? "charger read failed" : "charger not present";
		bool ok = false;

		if (charger_present && read_charger(s) == 0) {
			bool fresh = true;

			if ((s.status0 & 0x01) != 0) {
				LOG_WRN("Charger: I2C watchdog expired; re-applying configuration");
				(void)configure_charger();
				(void)kick_watchdog();
				input_limit_ma = -1;
				/* Re-read so the gates see the re-applied settings. */
				fresh = read_charger(s) == 0;
			}
			if (fresh) {
				const power::CcAdvert cc = power::classify_cc_pin_mv(
					power::active_cc_pin_mv(r.cc1_pin_mv, r.cc2_pin_mv));

				(void)apply_input_limit(cc, chg::vbus_stat(s.status1), s.iindpm);
				ok = charge_gates_pass(s, r, reason);
			}
			log_charger_flags();
		}
		set_charging(ok, reason);

		if (tick % kWatchdogKickTicks == 0 && charger_present) {
			(void)kick_watchdog();
		}
		if (tick % kLogEveryTicks == 0) {
			log_state(s, r);
		}
		log_input_edges(inputs);
		if (tick % 2 == 0) {
			(void)gpio_pin_toggle_dt(&status_led);
		}
		tick++;
		k_msleep(kTickMs);
	}
}

K_THREAD_DEFINE(power_diag_tid, 2048, power_thread, NULL, NULL, NULL, 9, 0, SYS_FOREVER_MS);

void log_reset_and_clock()
{
#if defined(CONFIG_HWINFO)
	uint32_t cause = 0;

	if (hwinfo_get_reset_cause(&cause) == 0) {
		LOG_INF("Reset cause 0x%08x (pin %d, por %d, watchdog %d, software %d, debug %d)",
			cause, !!(cause & RESET_PIN), !!(cause & RESET_POR),
			!!(cause & RESET_WATCHDOG), !!(cause & RESET_SOFTWARE),
			!!(cause & RESET_DEBUG));
		(void)hwinfo_clear_reset_cause();
	}
#endif
	const uint32_t lf = NRF_CLOCK->LFCLKSTAT;
	const uint32_t src = (lf & CLOCK_LFCLKSTAT_SRC_Msk) >> CLOCK_LFCLKSTAT_SRC_Pos;

	LOG_INF("LFCLK %s, source %s", (lf & CLOCK_LFCLKSTAT_STATE_Msk) ? "running" : "stopped",
		src == CLOCK_LFCLKSTAT_SRC_Xtal ? "32.768 kHz crystal"
						: (src == CLOCK_LFCLKSTAT_SRC_RC ? "RC" : "synth"));
}

int configure_gpios()
{
	const struct gpio_dt_spec *const inputs[] = {&chg_int, &pwr_sw, &btn_mode, &btn_action};

	for (const struct gpio_dt_spec *g : inputs) {
		if (!gpio_is_ready_dt(g) || gpio_pin_configure_dt(g, GPIO_INPUT) != 0) {
			return -ENODEV;
		}
	}
	/* board_safety.cpp already drives CHG_EN and VBAT_MEAS_EN inactive at boot. */
	if (gpio_pin_configure_dt(&chg_en, GPIO_OUTPUT_INACTIVE) != 0 ||
	    gpio_pin_configure_dt(&vbat_meas_en, GPIO_OUTPUT_INACTIVE) != 0 ||
	    gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_INACTIVE) != 0) {
		return -ENODEV;
	}
	return 0;
}

} // namespace

int power_diag_check()
{
	int failed = 0;

	log_reset_and_clock();
	if (configure_gpios() != 0) {
		LOG_ERR("Power: FAIL, GPIO setup");
		return 1;
	}
	if (adc_setup() != 0) {
		LOG_ERR("Power: FAIL, SAADC setup");
		failed++;
	}

	uint8_t part = 0;

	if (!device_is_ready(charger_bus)) {
		LOG_ERR("Charger: FAIL, I2C bus not ready");
		failed++;
	} else if (reg_read8(chg::kPartInfo, part) != 0) {
		LOG_ERR("Charger: FAIL, no response at 0x%02x", chg::kAddress);
		failed++;
	} else if (!chg::is_bq25620(part)) {
		LOG_ERR("Charger: FAIL, part info 0x%02x is not a BQ25620", part);
		failed++;
	} else {
		LOG_INF("Charger: BQ25620 rev %u found", part & 0x07);
		charger_present = true;
		if (configure_charger() != 0) {
			LOG_ERR("Charger: FAIL, configuration write");
			failed++;
		}
	}

	const Rails r = read_rails();

	LOG_INF("Power: VBAT %d mV via divider (expect pack voltage, or ~0 with no pack)",
		r.vbat_mv);
	LOG_INF("Power: VLED %d mV (LED rail is off in this image: expect ~0)", r.vled_mv);
	if (r.vled_mv > 500) {
		LOG_ERR("Power: FAIL, VLED present while LED_EN is low");
		failed++;
	}
	LOG_INF("Power checks done: %d failed; charging %s", failed,
		kChargeAllowed ? "allowed when every gate passes" : "disabled in this image");
	k_thread_start(power_diag_tid);
	return failed;
}

} // namespace badge

#else

namespace badge
{
int power_diag_check()
{
	return 0;
}
} // namespace badge

#endif
