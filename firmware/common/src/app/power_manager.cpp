/*
 * SPDX-License-Identifier: MIT
 *
 * Rev A charger and battery manager (badge/power_manager.hpp), one thread:
 * - every 20 ms: CC sense and the charger's D+/D- result set the input limit,
 *   which is read back from IINDPM and rewritten when it differs (docs/power.md,
 *   tSinkAdj 60 ms); the power switch is debounced;
 * - VBAT through the switched divider every 100 ms while the LED rail is on
 *   and every 1 s otherwise, fed to the load-shed policy (power_policy.hpp),
 *   whose brightness ceiling the matrix driver enforces;
 * - every 240 ms: charger state and the charge gates. Charging needs
 *   CONFIG_APP_CHARGING and every gate, including a pack-presence probe
 *   (30 mA forced discharge) before charging and every 30 s while charging;
 * - the charger I2C watchdog kept alive and TS thresholds set before any
 *   charging.
 * Bring-up images (verbose) add state logs, charger event flags, button and
 * switch edges and a status-LED heartbeat; the product shows charging on it.
 */

#include "badge/power_manager.hpp"

#if defined(CONFIG_APP_POWER_MANAGER)

#include "badge/bq25620.hpp"
#include "badge/nfc_platform.hpp"
#include "badge/power_policy.hpp"
#include "badge/power_sense.hpp"

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/reboot.h>
#include <hal/nrf_power.h>
#if defined(CONFIG_POWEROFF)
#include <zephyr/sys/poweroff.h>
#endif
#if defined(CONFIG_APP_LOAD_SHED)
#include "badge/display_sync.h"
#endif
#if defined(CONFIG_ICM42688_I2C)
#include "badge/imu_power.h"
#endif
#if defined(CONFIG_APP_USB_DEVICE)
#include "badge/usb_device.h"
#endif

LOG_MODULE_REGISTER(power, CONFIG_APP_LOG_LEVEL);

#define USER_NODE DT_PATH(zephyr_user)

BUILD_ASSERT(DT_NODE_HAS_PROP(USER_NODE, charger_i2c_bus) &&
		     DT_NODE_HAS_PROP(USER_NODE, io_channels) &&
		     DT_NODE_HAS_PROP(USER_NODE, chg_en_gpios),
	     "the power manager needs the Rev A charger bus and sense pins");

namespace badge
{
namespace
{

namespace chg = bq25620;

/* CC is sampled and the input limit enforced every 20 ms, inside the 60 ms
 * tSinkAdj of USB Type-C R2.5 (docs/power.md); slower work runs every 240 ms. */
constexpr int kFastTickMs = 20;
constexpr unsigned kSlowEvery = 12;          /* 240 ms: charger read, gates */
constexpr unsigned kLogEvery = 250;          /* 5 s */
constexpr unsigned kWatchdogKickEvery = 500; /* 10 s, against the 50 s charger watchdog */
constexpr unsigned kVbatSettleTicks = 2;     /* 40 ms, above the 25 ms start-up settle */
constexpr unsigned kVbatLoadedEvery = 5;     /* 100 ms while the LED rail is on */
constexpr unsigned kVbatIdleEvery = 50;      /* 1 s otherwise */
constexpr unsigned kProbeDischargeTicks = 3; /* 60 ms of FORCE_IBATDIS before sampling */
constexpr unsigned kProbeEvery = 1500;       /* 30 s between pack-presence probes */
constexpr unsigned kDiscoverEvery = 50;      /* 1 s between charger discovery retries */
constexpr unsigned kSwitchStableTicks = 5;   /* 100 ms power-switch debounce */
constexpr uint32_t kHealthyMs = 1000;        /* thread heartbeat age */
constexpr uint32_t kSenseStaleMs = 5000;     /* no valid VBAT/CC this long: unhealthy */
constexpr uint32_t kAdcResetupMs = 2000;     /* no valid VBAT this long: set the SAADC up again */
constexpr int kVbatSettleMs = 25;            /* blocking read at start-up only */
constexpr int kHizMinMv = 3400;     /* pack needed to enter HIZ while USB is suspended ... */
constexpr int kHizExitMv = 3300;    /* ... and to stay in it (hysteresis) */
constexpr int kVbatMaxMv = 4500;    /* above the pack's 4.28 V limit: sensing fault */
constexpr uint8_t kTh456Code = 0x3; /* docs/power.md: TS_TH4_TH5_TH6 = 011 */

#if defined(CONFIG_APP_CHARGING)
constexpr bool kChargeAllowed = true;
constexpr int kIchgMa = CONFIG_APP_CHARGE_ICHG_MA;
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
const struct gpio_dt_spec nfc_pwr = GPIO_DT_SPEC_GET(USER_NODE, nfc_pwr_gpios);
const struct gpio_dt_spec btn_mode = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
const struct gpio_dt_spec btn_action = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#if defined(CONFIG_APP_LOAD_SHED)
const struct device *const display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
#endif

bool adc_calibrated;
bool adc_ready; /* all four SAADC channels set up; retried every 1 s */
/* Uptime of the last valid VBAT sample and CC read: sensing health. */
atomic_t last_vbat_ok;
atomic_t last_cc_ok;
atomic_t last_charger_ok; /* last successful charger status read */
bool usb_start_failed;    /* USB stack not started: a host suspend would go unseen */
bool charger_present;
bool vled_checked;      /* VLED read with the rail off at least once this boot */
bool hiz_confirmed;     /* EN_HIZ known set: a set_hiz(true) succeeded since */
bool hiz_pending;       /* HIZ wanted (host suspended) but not confirmed */
bool vled_fault;        /* VLED present with LED_EN low at start: matrix held for this boot */
bool configure_pending; /* configure_charger() failed: retried every 240 ms */
bool charging;
int input_limit_ma = -1;
unsigned iindpm_restores; /* times the charger's own IINDPM value was overwritten */
/* FORCE_IBATDIS may be set: true until a clear is confirmed, including after an
 * MCU reset that interrupted a probe. */
bool discharge_on = true;

bool verbose;
bool started;
power::LoadShedPolicy policy;
/* Shared with the application threads. */
atomic_t heartbeat;
atomic_t switch_state = ATOMIC_INIT(-1);
atomic_t low_battery;
#if defined(CONFIG_APP_LOAD_SHED)
atomic_t held = ATOMIC_INIT(1); /* LED rail held off: no reading yet */
#endif
atomic_t stop_state; /* 0 running, 1 stop requested, 2 thread parked */
K_SEM_DEFINE(stopped, 0, 1);
K_SEM_DEFINE(resume, 0, 1);
K_SEM_DEFINE(usb_event, 0, 1); /* USB state changed: re-check HIZ at once */
/* EN_HIZ may be set: true until a clear is confirmed, because the charger
 * keeps it across an MCU reset (as discharge_on for FORCE_IBATDIS). */
bool hiz_on = true;

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
	if (!adc_ready) {
		return -ENODEV;
	}
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
	bool fresh = false; /* every field came from the last read */
};

int read_charger_fields(ChargerState &s)
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
	if ((err = reg_read16(chg::kVbusAdc, w)) != 0) {
		return err;
	}
	s.vbus_mv = chg::vbus_mv(w);
	if ((err = reg_read16(chg::kVbatAdc, w)) != 0) {
		return err;
	}
	s.vbat_mv = chg::vbat_mv(w);
	if ((err = reg_read16(chg::kVsysAdc, w)) != 0) {
		return err;
	}
	s.vsys_mv = chg::vbat_mv(w);
	if ((err = reg_read16(chg::kIbusAdc, w)) != 0) {
		return err;
	}
	s.ibus_ma = chg::ibus_ma(w);
	if ((err = reg_read16(chg::kIbatAdc, w)) != 0) {
		return err;
	}
	s.ibat_valid = chg::ibat_valid(w);
	s.ibat_ma = chg::ibat_ma(w);
	if ((err = reg_read16(chg::kTsAdc, w)) != 0) {
		return err;
	}
	s.ts_permille = chg::ts_permille(w);
	if ((err = reg_read16(chg::kTdieAdc, w)) != 0) {
		return err;
	}
	s.tdie_decic = chg::tdie_decic(w);
	return 0;
}

/* Updates s only when every register read succeeds; otherwise s is marked
 * stale, so no gate or log uses a value left over from an earlier read. */
int read_charger(ChargerState &s)
{
	ChargerState next;
	int err = read_charger_fields(next);

	if (err != 0) {
		s.fresh = false;
		return err;
	}
	next.fresh = true;
	s = next;
	return 0;
}

/* REG0x16 read-modify-write shared by the watchdog kick, the forced
 * discharge, HIZ and the watchdog period. WD_RST reads back 0 and is written
 * only when a caller sets it. */
int update_control1(uint8_t set, uint8_t clear)
{
	uint8_t ctrl1 = 0;
	int err = reg_read8(chg::kControl1, ctrl1);

	if (err != 0) {
		return err;
	}
	ctrl1 = static_cast<uint8_t>((ctrl1 & ~(clear | chg::kWdRst)) | set);
	return reg_write8(chg::kControl1, ctrl1);
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
	/* Undo power_enter_off(): the charger keeps its registers across an MCU
	 * reset, so the watchdog and interrupt sources are restored here. */
	if ((err = update_control1(chg::kWatchdog50s, chg::kWatchdogMask)) != 0 ||
	    (err = reg_write8(chg::kMask0, 0)) != 0 || (err = reg_write8(chg::kMask1, 0)) != 0 ||
	    (err = reg_write8(chg::kFaultMask0, 0)) != 0) {
		return err;
	}
	return reg_write8(chg::kAdcControl, chg::kAdcEnableContinuous12Bit);
}

/* Also clears FORCE_IBATDIS unless a pack probe is running, so the
 * read-modify-write cannot preserve a discharge left on by a failed clear. */
int kick_watchdog(bool probe_running)
{
	const int err = update_control1(chg::kWdRst, probe_running ? 0 : chg::kForceIbatdis);

	if (err == 0 && !probe_running) {
		discharge_on = false;
	}
	return err;
}

/* Reads IINDPM on every fast tick and rewrites it when it differs from the
 * policy. No cached value is trusted: the charger resets IINDPM to 3.2 A on
 * adapter removal and its D+/D- detection rewrites it on every plug-in
 * (SLUSEG2D REG0x06). input_limit_ma is the limit confirmed this tick, or -1
 * after a failed access, which fails the charge gate. */
int enforce_input_limit(int limit)
{
	uint16_t reg = 0;
	int err = reg_read16(chg::kInputCurrent, reg);

	if (err == 0 && chg::decode_iindpm(reg) != limit) {
		err = reg_write16(chg::kInputCurrent, chg::encode_iindpm(limit));
		if (err == 0 && limit != input_limit_ma) {
			LOG_INF("Charger: input limit %d mA", limit);
		} else if (err == 0) {
			iindpm_restores++;
		}
	}
	input_limit_ma = err == 0 ? limit : -1;
	return err;
}

bool board_too_warm; /* TS gate state, with hysteresis */

/* All gates for the bring-up charge (docs/bringup.md, first charge). */
bool charge_gates_pass(const ChargerState &s, const Rails &r, int policy_limit, const char *&reason)
{
	/* Updated on every read, whichever gate decides. */
	board_too_warm = power::ts_too_warm(s.ts_permille, board_too_warm);
	if (!kChargeAllowed) {
		reason = "disabled in this image (CONFIG_APP_CHARGING=n)";
	} else if (chg::vbus_stat(s.status1) == 0 && s.vbus_mv < 4300) {
		reason = "no VBUS";
	} else if (chg::any_fault(s.fault0)) {
		reason = "charger fault";
	} else if (chg::ts_stat(s.fault0) != 0) {
		reason = "TS not normal";
	} else if (chg::get_th456(s.ntc1) != kTh456Code) {
		reason = "TS thresholds not set";
	} else if (board_too_warm) {
		reason = "board above 38 C (cell charge limit 40 C)";
	} else if (chg::decode_ichg(s.ichg) != chg::decode_ichg(chg::encode_ichg(kIchgMa))) {
		reason = "ICHG readback mismatch";
	} else if (input_limit_ma != policy_limit ||
		   chg::decode_iindpm(s.iindpm) != input_limit_ma) {
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
		if (on && verbose) {
			LOG_WRN("Charger: CHARGING ENABLED at ICHG %d mA; watch the cell and "
				"charger temperature",
				kIchgMa);
		} else if (on) {
			LOG_INF("Charger: charging at ICHG %d mA", kIchgMa);
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
	if (!s.fresh) {
		LOG_WRN("Charger: last register read failed; no current telemetry");
		return;
	}
	LOG_INF("Charger: VBUS %d mV IBUS %d mA | VBAT %d mV IBAT %d%s mA | VSYS %d mV | "
		"TS %d.%d %% | TDIE %s%d.%d C",
		s.vbus_mv, s.ibus_ma, s.vbat_mv, s.ibat_ma, s.ibat_valid ? "" : "?", s.vsys_mv,
		s.ts_permille / 10, s.ts_permille % 10, s.tdie_decic < 0 ? "-" : "",
		(s.tdie_decic < 0 ? -s.tdie_decic : s.tdie_decic) / 10,
		(s.tdie_decic < 0 ? -s.tdie_decic : s.tdie_decic) % 10);
	LOG_INF("Charger: %s, D+/D- %s, TS %s, fault 0x%02x, status0 0x%02x, ICHG %d mA, "
		"IINDPM %d mA (%u restores), VREG %d mV, CHG_EN %d%s%s",
		chg::chg_stat_name(chg::chg_stat(s.status1)),
		chg::vbus_stat_name(chg::vbus_stat(s.status1)),
		chg::ts_stat_name(chg::ts_stat(s.fault0)), s.fault0, s.status0,
		chg::decode_ichg(s.ichg), chg::decode_iindpm(s.iindpm), iindpm_restores,
		chg::decode_vreg(s.vreg), charging ? 1 : 0, charging ? "" : " - ",
		charging ? "" : off_reason);
}

struct Inputs {
	int mode = -1, action = -1, sw = -1;
};

/* /INT is a 256 us pulse (SLUSEG2D pin table), too short to poll; the flag
 * registers latch each event until read, so read them every slow tick. */
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

/* Pack-presence probe. VBAT in range does not prove a pack: with no pack and
 * charging enabled the charger regulates BAT itself. With charging disabled
 * the BATFET turns off (SLUSEG2D 8.3.3.6), so the ~30 mA forced discharge
 * (FORCE_IBATDIS, REG0x16 bit 6) pulls an empty BAT node below 3 V within
 * milliseconds while a pack holds its voltage. */
int set_force_discharge(bool on)
{
	if (on) {
		discharge_on = true; /* assume set even if the write fails part-way */
	}
	const int err = on ? update_control1(chg::kForceIbatdis, 0)
			   : update_control1(0, chg::kForceIbatdis);

	if (err == 0 && !on) {
		discharge_on = false;
	}
	return err;
}

int read_cc(Rails &r)
{
	int pin = 0;
	int err = adc_pin_mv(adc_cc1, pin);

	r.cc1_pin_mv = err == 0 ? pin : -1;
	if (err == 0) {
		err = adc_pin_mv(adc_cc2, pin);
	}
	r.cc2_pin_mv = err == 0 ? pin : -1;
	return err;
}

/* ---- Load shedding and power switch ---- */

bool rail_on()
{
#if defined(CONFIG_APP_LOAD_SHED)
	return badge_display_rail_on(display);
#else
	return false;
#endif
}

const char *shed_state_name(power::ShedState state)
{
	switch (state) {
	case power::ShedState::kNoReading:
		return "no reading";
	case power::ShedState::kNormal:
		return "normal";
	case power::ShedState::kRamping:
		return "re-enabling";
	case power::ShedState::kShed:
		return "shed";
	}
	return "?";
}

/* The matrix ceiling the driver enforces: the battery policy's, or 0 while
 * the rail-off check has not passed (VLED unread or present with LED_EN low)
 * or a suspended host still waits for a confirmed HIZ. */
uint8_t apply_ceiling();

/* VLED must read near 0 V with LED_EN low before the matrix may run: a rail
 * that cannot be switched off cannot be shed. Retried until a reading succeeds. */
void check_vled_off()
{
	int pin = 0;

	if (adc_pin_mv(adc_vled, pin) != 0) {
		return;
	}
	const int vled_mv = power::vled_from_pin_mv(pin);

	vled_checked = true;
	if (vled_mv > 500) {
		LOG_ERR("Power: FAIL, VLED %d mV while LED_EN is low; matrix held off", vled_mv);
		vled_fault = true;
	}
	(void)apply_ceiling();
}

uint8_t apply_ceiling()
{
	const uint8_t ceiling = (!vled_checked || vled_fault || hiz_pending) ? 0 : policy.ceiling();

#if defined(CONFIG_APP_LOAD_SHED)
	badge_display_set_power_cap(display, ceiling);
	atomic_set(&held, ceiling == 0 ? 1 : 0);
#endif
	return ceiling;
}

/* One VBAT reading into the load-shed policy; the driver enforces its ceiling. */
void apply_policy(const power::Sample &sample)
{
	const power::ShedState before = policy.state();
	const uint8_t old_ceiling = policy.ceiling();
	const bool was_low = policy.low_power();

	policy.update(sample);
	const uint8_t ceiling = apply_ceiling();

	atomic_set(&low_battery, policy.low_power() ? 1 : 0);
	if (policy.state() != before || ceiling != old_ceiling || policy.low_power() != was_low) {
		LOG_INF("Power: VBAT %d mV%s%s: matrix ceiling %u (%s, %u failed re-enables)%s",
			sample.vbat_mv, sample.loaded ? " loaded" : "",
			sample.valid ? "" : " (invalid)", ceiling, shed_state_name(policy.state()),
			policy.failures(), policy.low_power() ? ", LOW BATTERY" : "");
	}
}

void debounce_switch(int &candidate, unsigned &stable)
{
	const int raw = gpio_pin_get_dt(&pwr_sw);

	if (raw < 0) {
		return;
	}
	if (raw != candidate) {
		candidate = raw;
		stable = 1;
		return;
	}
	if (stable < kSwitchStableTicks && ++stable == kSwitchStableTicks) {
		if (atomic_get(&switch_state) != raw) {
			LOG_INF("Power switch %s", raw ? "ON" : "OFF");
		}
		atomic_set(&switch_state, raw);
	}
}

/* The nRF52840's VBUS detector: a level, independent of the charger's I2C. */
bool nrf_vbus_present()
{
#if NRF_POWER_HAS_USBREG
	return nrf_power_usbregstatus_vbusdet_get(NRF_POWER);
#else
	return false;
#endif
}

/* An IMU whose driver did not initialize is left in its reset state, with
 * both sensors off (PWR_MGMT0 reset 0). */
int sleep_imu()
{
#if defined(CONFIG_ICM42688_I2C) && DT_NODE_HAS_STATUS(DT_NODELABEL(imu), okay)
	const struct device *const imu = DEVICE_DT_GET(DT_NODELABEL(imu));

	if (device_is_ready(imu)) {
		const int err = badge_imu_sleep(imu);

		if (err != 0) {
			LOG_WRN("Power: IMU sleep failed (%d)", err);
		}
		return err;
	}
#endif
	return 0;
}

bool usb_enable_failing()
{
#if defined(CONFIG_APP_USB_DEVICE)
	return badge_usb_enable_failing();
#else
	return false;
#endif
}

/* High-impedance input (SLUSEG2D 8.3.4.3): VBUS draws at most 20 uA and the
 * battery runs the system. Reset by the I2C watchdog and adapter plug-in. */
int set_hiz(bool on)
{
	return on ? update_control1(chg::kEnHiz, 0) : update_control1(0, chg::kEnHiz);
}

/* BQ25620 identity and configuration. Retried by the manager every second
 * until the charger answers, so a transient bus error at boot does not leave
 * the input limit, charging and System OFF quiescing disabled all boot. */
int discover_charger()
{
	static bool logged;
	uint8_t part = 0;
	int err = device_is_ready(charger_bus) ? reg_read8(chg::kPartInfo, part) : -ENODEV;

	if (err == 0 && !chg::is_bq25620(part)) {
		err = -ENODEV;
	}
	if (err != 0) {
		if (!logged) {
			LOG_ERR("Charger: FAIL, no BQ25620 at 0x%02x (part 0x%02x, %d); retrying",
				chg::kAddress, part, err);
			logged = true;
		}
		return err;
	}
	LOG_INF("Charger: BQ25620 rev %u found", part & 0x07);
	hiz_on = true; /* unknown until a clear is confirmed */
	configure_pending = configure_charger() != 0;
	if (configure_pending) {
		LOG_ERR("Charger: FAIL, configuration write; retrying");
	}
	if (set_hiz(false) == 0) {
		hiz_on = false;
	}
	charger_present = true;
	return configure_pending ? -EIO : 0;
}

/* EN_HIZ may be set; meaningless without a charger. */
bool hiz_maybe()
{
	return charger_present && hiz_on;
}

power::UsbHost usb_host()
{
#if defined(CONFIG_APP_USB_DEVICE)
	switch (badge_usb_host_state()) {
	case BADGE_USB_CONFIGURED:
		return power::UsbHost::kConfigured;
	case BADGE_USB_SUSPENDED:
		return power::UsbHost::kSuspended;
	default:
		break;
	}
#endif
	return power::UsbHost::kNotConfigured;
}

void power_thread(void *, void *, void *)
{
	Inputs inputs;
	Rails r;
	ChargerState s;
	unsigned tick = 0;
	/* VBAT divider and pack probe sequencing, in fast ticks. */
	unsigned vbat_started = 0U - kVbatIdleEvery, probe_started = 0, last_probe = 0;
	bool vbat_busy = false, vbat_loaded = false, probing = false, pack_ok = false,
	     probed = false;
	int policy_limit = 100;
	uint8_t last_vbus_stat = 0xFF;
	int sw_candidate = -1;
	unsigned sw_stable = 0;
	bool hiz_warned = false;
	bool hiz_pack_latched = false; /* left HIZ on a low pack: stay out until resume */
	int hiz_vbat_mv = -1;          /* last valid VBAT sample, -1 after an invalid one */
	bool hiz_want = false;
	bool cc_fresh = false; /* last fast-tick CC read succeeded */
	const char *reason = charger_present ? "starting" : "charger not present";

	/* A suspended USB host needs suspend current within 10 ms of bus idle
	 * (USB 2.0 suspend rules): run from the pack in HIZ if it can carry the load. */
	auto update_hiz = [&]() {
		const bool suspended = usb_host() == power::UsbHost::kSuspended;

		if (!suspended) {
			hiz_pack_latched = false;
		}
		/* Leaving HIZ on a low pack is latched until the host resumes: on
		 * VBUS the pack rebounds above the entry level, and re-entering would
		 * toggle HIZ and keep loading the suspended host. */
		const bool pack = !hiz_pack_latched &&
				  hiz_vbat_mv >= (hiz_on ? kHizExitMv : kHizMinMv) &&
				  !(probed && !pack_ok);
		const bool want = charger_present && suspended && pack;

		if (hiz_on && suspended && charger_present && !pack) {
			hiz_pack_latched = true;
		}

		hiz_want = want;

		if (suspended && !pack && !hiz_warned) {
			LOG_WRN("USB host suspended without a pack above %d mV: input stays on",
				kHizMinMv);
		}
		hiz_warned = suspended && !pack;
		/* hiz_on means "on or unknown" (it starts unknown after a reset);
		 * only a successful write confirms entry, so a wanted HIZ is
		 * written until confirmed. */
		if (want != hiz_on || (want && !hiz_confirmed)) {
			if (want) {
				set_charging(false, "USB host suspended (HIZ)");
			}
			if (set_hiz(want) == 0) {
				if (want != hiz_on || want != hiz_confirmed) {
					LOG_INF("Charger: %s HIZ (USB host %s)",
						want ? "entering" : "leaving",
						suspended ? "suspended" : "active");
				}
				hiz_on = want;
				hiz_confirmed = want;
			}
		}
		/* Until HIZ is confirmed the host carries the load: hold the
		 * matrix, the largest one. */
		if ((want && !hiz_confirmed) != hiz_pending) {
			hiz_pending = want && !hiz_confirmed;
			if (hiz_pending) {
				LOG_WRN("Charger: HIZ not confirmed; matrix held");
			}
			(void)apply_ceiling();
		}
	};

	/* A USB state change is handled between the I2C-heavy steps too, not
	 * only in the end-of-tick wait. A contended bus lock can still delay
	 * the HIZ write itself. */
	auto service_usb = [&]() {
		if (k_sem_take(&usb_event, K_NO_WAIT) == 0 && charger_present) {
			update_hiz();
		}
	};

	while (true) {
		if (atomic_cas(&stop_state, 1, 2)) {
			/* power_enter_off() owns the hardware until it resumes this
			 * thread (a semaphore, so an early resume is not lost). */
			k_sem_give(&stopped);
			k_sem_take(&resume, K_FOREVER);
			vbat_busy = probing = false;
			continue;
		}
		const int64_t tick_start = k_uptime_get();
		atomic_set(&heartbeat, static_cast<atomic_val_t>(k_uptime_get_32()));
		debounce_switch(sw_candidate, sw_stable);

		service_usb();
		/* ---- fast: CC policy within tSinkAdj ---- */
		if (charger_present) {
			uint8_t status1 = 0;

			const bool cc_ok = read_cc(r) == 0;

			cc_fresh = cc_ok;
			if (cc_ok) {
				atomic_set(&last_cc_ok,
					   static_cast<atomic_val_t>(k_uptime_get_32()));
			}

			if (reg_read8(chg::kStatus1, status1) != 0) {
				set_charging(false, "status read failed");
				input_limit_ma = -1;
				last_vbus_stat = 0xFF; /* unknown, not the last value */
			} else {
				atomic_set(&last_charger_ok,
					   static_cast<atomic_val_t>(k_uptime_get_32()));
				if (!cc_ok) {
					set_charging(false, "CC read failed");
				}
				/* Plug, unplug or a new D+/D- result: stop and let the slow
				 * tick re-qualify the source. */
				if (chg::vbus_stat(status1) != last_vbus_stat) {
					set_charging(false, "input source changed");
					last_vbus_stat = chg::vbus_stat(status1);
				}
				/* Unknown CC counts as no advertisement: the limit is still
				 * enforced, from BC1.2 and USB configuration only. */
				const power::CcAdvert cc =
					cc_ok ? power::classify_cc_pin_mv(power::active_cc_pin_mv(
							r.cc1_pin_mv, r.cc2_pin_mv))
					      : power::CcAdvert::kNone;

				/* 500 mA only once a host has configured the badge; a
				 * CDP counts as an SDP. */
				policy_limit = power::input_limit_ma(cc, chg::vbus_stat(status1),
								     usb_host());
				if (enforce_input_limit(policy_limit) != 0) {
					set_charging(false, "input limit access failed");
				}
			}
			/* Keep retrying a failed FORCE_IBATDIS clear. */
			if (discharge_on && !probing && set_force_discharge(false) == 0) {
				LOG_INF("Charger: forced discharge off");
			}
#if defined(CONFIG_APP_USB_DEVICE)
			badge_usb_poll(last_vbus_stat != 0 && last_vbus_stat != 0xFF);
#endif
			update_hiz();
		}
#if defined(CONFIG_APP_USB_DEVICE)
		else {
			badge_usb_poll(false);
		}
#endif

		/* ---- VBAT: divider on, sample kVbatSettleTicks later, shed policy ---- */
		if (!vbat_busy && !probing &&
		    tick - vbat_started >= (rail_on() ? kVbatLoadedEvery : kVbatIdleEvery)) {
			vbat_loaded = rail_on();
			vbat_started = tick;
			vbat_busy = gpio_pin_set_dt(&vbat_meas_en, 1) == 0;
			if (!vbat_busy) {
				power::Sample failed;

				failed.now_ms = k_uptime_get_32();
				apply_policy(failed);
			}
		}
		if (vbat_busy && tick - vbat_started >= kVbatSettleTicks && !probing) {
			int pin = 0;
			const bool read_ok = adc_pin_mv(adc_vbat, pin) == 0;
			power::Sample sample;

			(void)gpio_pin_set_dt(&vbat_meas_en, 0);
			vbat_busy = false;
			r.vbat_mv = read_ok ? power::vbat_from_pin_mv(pin) : -1;
			sample.valid = read_ok && r.vbat_mv >= 0 && r.vbat_mv <= kVbatMaxMv;
			hiz_vbat_mv = sample.valid ? r.vbat_mv : -1;
			if (sample.valid) {
				atomic_set(&last_vbat_ok,
					   static_cast<atomic_val_t>(k_uptime_get_32()));
			}
			sample.vbat_mv = r.vbat_mv;
			/* Loaded only if the LED rail stayed on across the reading. */
			sample.loaded = vbat_loaded && rail_on();
			/* USB presence from VBUS itself. An uncertain HIZ state only
			 * withholds input capacity (input_limit_ma below). */
			sample.usb_present = nrf_vbus_present() ||
					     (last_vbus_stat != 0 && last_vbus_stat != 0xFF) ||
					     (s.fresh && s.vbus_mv >= 4300);
			sample.input_limit_ma = hiz_maybe() ? -1 : input_limit_ma;
			sample.now_ms = k_uptime_get_32();
			apply_policy(sample);
		}

		/* ---- pack-presence probe ---- */
		if (probing && tick - probe_started >= kProbeDischargeTicks) {
			int pin = 0;
			const int mv =
				adc_pin_mv(adc_vbat, pin) == 0 ? power::vbat_from_pin_mv(pin) : -1;

			(void)gpio_pin_set_dt(&vbat_meas_en, 0);
			probing = false;
			last_probe = tick;
			if (set_force_discharge(false) != 0) {
				/* Not a valid probe until the discharge is off; the fast
				 * tick retries the clear and the probe runs again. */
				pack_ok = false;
				probed = false;
				LOG_WRN("Charger: pack probe cleanup failed; retrying");
			} else {
				pack_ok = mv >= 3000;
				probed = true;
				LOG_INF("Charger: pack probe %d mV under 30 mA discharge: %s", mv,
					pack_ok ? "pack present" : "NO PACK");
			}
		}

		if (!charger_present && tick % kDiscoverEvery == 0) {
			(void)discover_charger();
		}
		if (adc_ready && tick % kDiscoverEvery == 0 &&
		    k_uptime_get_32() - static_cast<uint32_t>(atomic_get(&last_vbat_ok)) >=
			    kAdcResetupMs) {
			/* Reads keep failing after a good setup: set it up again. */
			LOG_WRN("Power: no valid VBAT for %u ms; SAADC setup again",
				static_cast<unsigned>(kAdcResetupMs));
			adc_ready = false;
		}
		if (!adc_ready && tick % kDiscoverEvery == 0 && adc_setup() == 0) {
			adc_ready = true;
			LOG_INF("Power: SAADC ready");
		}
		/* ---- slow: charger state, gates, watchdog, log ---- */
		if (tick % kSlowEvery == 0 && !vled_checked && !rail_on()) {
			check_vled_off();
		}
		if (tick % kSlowEvery == 0 && !probing) {
			bool ok = false;

			service_usb();
			if (charger_present && configure_pending) {
				configure_pending = configure_charger() != 0;
			}
			service_usb();
			if (charger_present && read_charger(s) == 0) {
				service_usb();
				if ((s.status0 & 0x01) != 0) {
					LOG_WRN("Charger: I2C watchdog expired; re-applying "
						"configuration");
					configure_pending = configure_charger() != 0;
					(void)kick_watchdog(false);
					input_limit_ma = -1;
					reason = "watchdog expired; reconfigured";
				} else {
					ok = charge_gates_pass(s, r, policy_limit, reason);
				}
				if (hiz_want) {
					/* The I2C watchdog and a re-plug clear EN_HIZ. */
					/* A failed re-assert leaves HIZ unconfirmed: the
					 * matrix is held until the next write succeeds. */
					hiz_confirmed = set_hiz(true) == 0;
				}
				if (ok && !cc_fresh) {
					ok = false;
					reason = "CC read failed";
				}
				if (hiz_on || hiz_want) {
					/* Also while a wanted HIZ entry is not confirmed. */
					ok = false;
					reason = hiz_want ? "USB host suspended (HIZ)"
							  : "HIZ clear not confirmed";
				}
				if (verbose) {
					log_charger_flags();
				}
			} else if (charger_present) {
				reason = "charger read failed";
			}
			/* Charging needs a pack probe no older than 30 s: before the first
			 * enable, every 30 s while charging, and every 30 s while held
			 * off by a missing pack, so re-inserting one is seen. */
			if (ok && (!probed || tick - last_probe >= kProbeEvery)) {
				set_charging(false, "pack presence probe");
				if (vbat_busy || discharge_on) {
					/* Divider sample in flight or a clear pending: probe on a
					 * later slow tick. */
				} else if (set_force_discharge(true) != 0 ||
					   gpio_pin_set_dt(&vbat_meas_en, 1) != 0) {
					(void)set_force_discharge(false);
					(void)gpio_pin_set_dt(&vbat_meas_en, 0);
				} else {
					probing = true;
					probe_started = tick;
				}
			} else {
				set_charging(ok && pack_ok && !discharge_on,
					     !ok ? reason
					     : discharge_on
						     ? "forced discharge still on"
						     : "no pack: BAT fell below 3 V in probe");
				int pin = 0;

				if (adc_pin_mv(adc_vled, pin) == 0) {
					r.vled_mv = power::vled_from_pin_mv(pin);
				}
			}
			if (!verbose) {
				/* Product: the status LED shows charging in progress. */
				(void)gpio_pin_set_dt(&status_led,
						      charging && chg::chg_stat(s.status1) != 0);
			}
		}
		if (tick % kWatchdogKickEvery == 0 && charger_present) {
			(void)kick_watchdog(probing);
		}
		if (verbose) {
			if (tick % kLogEvery == 0) {
				log_state(s, r);
			}
			log_input_edges(inputs);
			if (tick % 25 == 0) {
				(void)gpio_pin_toggle_dt(&status_led);
			}
		}
		tick++;
		/* Sleep to the next tick, handling USB suspend and resume at once.
		 * At least 1 ms, so slow I2C never starves lower-priority threads. */
		const int64_t deadline = tick_start + kFastTickMs;
		bool slept = false;

		for (int64_t left = deadline - k_uptime_get(); left > 0;
		     left = deadline - k_uptime_get()) {
			slept = true;
			if (k_sem_take(&usb_event, K_MSEC(left)) != 0) {
				break;
			}
			if (charger_present) {
				update_hiz();
			}
		}
		if (!slept) {
			k_msleep(1);
		}
	}
}

K_THREAD_DEFINE(power_tid, 2048, power_thread, NULL, NULL, NULL, 6, 0, SYS_FOREVER_MS);

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

int power_manager_start(bool verbose_logs)
{
	int failed = 0;

	verbose = verbose_logs;
	if (configure_gpios() != 0) {
		LOG_ERR("Power: FAIL, GPIO setup; the LED rail stays off");
		return 1;
	}
	adc_ready = adc_setup() == 0;
	atomic_set(&last_vbat_ok, static_cast<atomic_val_t>(k_uptime_get_32()));
	atomic_set(&last_cc_ok, static_cast<atomic_val_t>(k_uptime_get_32()));
	atomic_set(&last_charger_ok, static_cast<atomic_val_t>(k_uptime_get_32()));
	if (!adc_ready) {
		/* Retried every 1 s; unhealthy after kSenseStaleMs without VBAT. */
		LOG_ERR("Power: FAIL, SAADC setup; retrying");
		failed++;
	}

	/* A failed discovery or HIZ clear is retried by the manager. */
	if (discover_charger() != 0) {
		failed++;
	}
#if defined(CONFIG_APP_USB_DEVICE)
	if (const int err = badge_usb_start(&usb_event); err != 0) {
		/* A host suspend would go unseen: unhealthy while VBUS is present. */
		LOG_ERR("USB: FAIL, device start (%d)", err);
		usb_start_failed = true;
		failed++;
	}
#endif

	const Rails r = read_rails();

	if (verbose) {
		LOG_INF("Power: VBAT %d mV via divider (expect pack voltage, or ~0 with no pack)",
			r.vbat_mv);
		LOG_INF("Power: VLED %d mV (LED rail is off at start: expect ~0)", r.vled_mv);
	}
	/* The rail is off at start; an unreadable VLED is retried by the thread. */
	check_vled_off();
	if (vled_fault) {
		failed++;
	}
	LOG_INF("Power manager started: %d failed checks; charging %s", failed,
		kChargeAllowed ? "allowed when every gate passes" : "disabled in this image");
	atomic_set(&heartbeat, static_cast<atomic_val_t>(k_uptime_get_32()));
	started = true;
	k_thread_start(power_tid);
	return failed;
}

bool power_manager_healthy()
{
	/* Never started (GPIO setup failed): unhealthy, so the application holds
	 * the display and stops the watchdog feed instead of running unmanaged. */
	/* Sensing that stays down (SAADC setup or reads) leaves no VBAT, so no
	 * low-battery off, and no CC for the input limit: unhealthy, so the
	 * watchdog resets the badge and setup runs again. */
	const uint32_t now = k_uptime_get_32();

	if (started && atomic_get(&stop_state) == 0 &&
	    (now - static_cast<uint32_t>(atomic_get(&last_vbat_ok)) >= kSenseStaleMs ||
	     (charger_present &&
	      now - static_cast<uint32_t>(atomic_get(&last_cc_ok)) >= kSenseStaleMs))) {
		return false;
	}
	/* With USB attached the charger must be under control (input limit, HIZ)
	 * and the USB stack running (host suspend). A charger that never answered
	 * or went silent, or a USB stack that did not start, fails closed. On
	 * battery alone neither matters, and System OFF waits for the charger. */
	if (started && atomic_get(&stop_state) == 0 && nrf_vbus_present() &&
	    (usb_start_failed || usb_enable_failing() ||
	     now - static_cast<uint32_t>(atomic_get(&last_charger_ok)) >= kSenseStaleMs)) {
		return false;
	}
	return started &&
	       (atomic_get(&stop_state) != 0 ||
		k_uptime_get_32() - static_cast<uint32_t>(atomic_get(&heartbeat)) < kHealthyMs);
}

int power_switch_state(uint32_t timeout_ms)
{
	const int64_t deadline = k_uptime_get() + timeout_ms;
	int state = static_cast<int>(atomic_get(&switch_state));

	while (state < 0 && k_uptime_get() < deadline) {
		k_msleep(10);
		state = static_cast<int>(atomic_get(&switch_state));
	}
	return state;
}

bool power_display_held()
{
#if defined(CONFIG_APP_LOAD_SHED)
	return atomic_get(&held) != 0;
#else
	return false;
#endif
}

bool power_low_battery()
{
	return atomic_get(&low_battery) != 0;
}

int power_enter_charging_only()
{
	return sleep_imu();
}

void resume_manager()
{
	atomic_clear(&stop_state);
	k_sem_give(&resume);
}

/* Checked after arming the wake sense, so an attach from here on pulses /INT.
 * VBUS_STAT reads 0 while BC1.2 detection runs, so the nRF52840's own VBUS
 * detector is checked too. */
bool usb_attached()
{
	uint8_t status1 = 0;

	return hiz_maybe() || nrf_vbus_present() ||
	       (charger_present && reg_read8(chg::kStatus1, status1) == 0 &&
		chg::vbus_stat(status1) != 0);
}

/* Watchdog off, ADC off and every /INT source but VBUS masked, each read back.
 * configure_charger() restores all of this after the wake. */
int quiet_charger()
{
	const struct {
		uint8_t reg, value;
	} writes[] = {
		{chg::kAdcControl, 0},
		{chg::kMask0, chg::kMask0All},
		{chg::kMask1, chg::kMask1AllButVbus},
		{chg::kFaultMask0, chg::kFaultMask0All},
	};
	uint8_t value = 0;
	/* Watchdog off and the ~30 mA forced discharge off, both read back. */
	int err = update_control1(0, chg::kWatchdogMask | chg::kForceIbatdis);

	if (err == 0) {
		err = reg_read8(chg::kControl1, value);
	}
	if (err == 0 && (value & (chg::kWatchdogMask | chg::kForceIbatdis)) != 0) {
		err = -EIO;
	}
	if (err == 0) {
		discharge_on = false;
	}
	for (const auto &w : writes) {
		if (err == 0) {
			err = reg_write8(w.reg, w.value);
		}
		if (err == 0) {
			err = reg_read8(w.reg, value);
		}
		if (err == 0 && value != w.value) {
			err = -EIO;
		}
	}
	const uint8_t flags[] = {chg::kFlag0, chg::kFlag1, chg::kFaultFlag0};

	for (uint8_t reg : flags) {
		(void)reg_read8(reg, value); /* clear latched events */
	}
	return err;
}

/* Back to normal operation before anything was switched off. */
void abort_off()
{
	(void)gpio_pin_interrupt_configure_dt(&chg_int, GPIO_INT_DISABLE);
	if (charger_present) {
		/* A failure stays pending: the manager retries every 240 ms. */
		configure_pending = configure_charger() != 0;
	}
	resume_manager();
}

void power_enter_off()
{
	static bool unavailable;

	if (unavailable || !started) {
		return;
	}
	if (!charger_present) {
		/* It may still run its watchdog, ADC and interrupts from before an
		 * MCU reset: wait for discovery (the application retries). */
		LOG_WRN("Power: charger not found yet; System OFF deferred");
		return;
	}
	k_sem_reset(&stopped);
	k_sem_reset(&resume);
	atomic_set(&stop_state, 1);
	if (k_sem_take(&stopped, K_MSEC(500)) != 0) {
		if (atomic_cas(&stop_state, 1, 0)) {
			LOG_ERR("Power: manager did not stop; staying on");
			return;
		}
		/* The thread parked as the wait timed out: its give is imminent. */
		k_sem_take(&stopped, K_FOREVER);
	}
	/* The manager thread is parked: this context owns the pins and charger. */
	set_charging(false, "entering System OFF");
	(void)gpio_pin_set_dt(&vbat_meas_en, 0);
	(void)set_force_discharge(false);
#if defined(CONFIG_APP_LOAD_SHED)
	badge_display_set_power_cap(display, 0);
	atomic_set(&held, 1);
#endif
	/* VBUS changes pulse /INT by default (REG0x24 VBUS_MASK reset 0, SLUSEG2D
	 * 8.6.2.26); GPIO sense on CHG_INT wakes the nRF52840 from System OFF. */
	int rc = gpio_pin_interrupt_configure_dt(&chg_int, GPIO_INT_LEVEL_ACTIVE);
#if !defined(CONFIG_POWEROFF)
	rc = -ENOTSUP;
#endif
	if (rc != 0) {
		LOG_ERR("Power: no USB wake source (%d); staying on", rc);
		unavailable = true;
		resume_manager();
		return;
	}
	/* Quiet the charger, or do not power off: the I2C watchdog would expire
	 * and pulse /INT about every 50 s, and its continuous ADC draws about
	 * 260 uA from the pack (SLUSEG2D IQ_BAT_ADC, against 1.5 uA). */
	if (quiet_charger() != 0) {
		LOG_ERR("Power: charger quiescing failed; staying on and retrying later");
		abort_off();
		return;
	}
	if (usb_attached()) {
		LOG_INF("Power: USB attached; staying on");
		abort_off();
		return;
	}
	if (sleep_imu() != 0) {
		LOG_ERR("Power: IMU sleep failed; staying on and retrying later");
		abort_off();
		return;
	}
	(void)gpio_pin_set_dt(&nfc_pwr, 0);
	(void)gpio_pin_set_dt(&status_led, 0);
	LOG_WRN("Power: battery low; System OFF until USB is attached or the switch is cycled");
	LOG_PANIC();
#if defined(CONFIG_POWEROFF)
	/* Last check with interrupts locked, so nothing can service and clear
	 * an attach between it and System OFF. The nRF52840's VBUS detector is a
	 * level, not the charger's 256 us pulse. The IMU and NFC are already off,
	 * so an attach seen here restarts the badge. */
	const unsigned int key = irq_lock();

	if (nrf_vbus_present()) {
		irq_unlock(key);
		sys_reboot(SYS_REBOOT_COLD);
	}
	sys_poweroff();
#endif
}

} // namespace badge

#endif /* CONFIG_APP_POWER_MANAGER */
