/*
 * SPDX-License-Identifier: MIT
 *
 * TI BQ25620 register map, field encoding and ADC decoding (SLUSEG2D, §8.6).
 * Pure functions only; the I2C transport lives in the application.
 *
 * Multi-byte registers are 16-bit little-endian words at the even address
 * (e.g. ICHG spans REG0x02-0x03). Field positions below were checked against
 * each register's reset value and code ranges in the datasheet.
 */

#pragma once

#include <stdint.h>

namespace badge::bq25620
{

constexpr uint8_t kAddress = 0x6B;

enum Reg : uint8_t {
	kChargeCurrent = 0x02, /* ICHG, 16-bit, bits 11:6, 80 mA/LSB */
	kChargeVoltage = 0x04, /* VREG, 16-bit, bits 11:3, 10 mV/LSB */
	kInputCurrent = 0x06,  /* IINDPM, 16-bit, bits 11:4, 20 mA/LSB */
	kChargeControl0 = 0x14,
	kChargeTimer = 0x15,
	kControl1 = 0x16, /* EN_CHG, WD_RST, WATCHDOG */
	kControl2 = 0x17,
	kNtc0 = 0x1A,
	kNtc1 = 0x1B, /* TS_TH1_TH2_TH3, TS_TH4_TH5_TH6, TS_VSET_WARM */
	kStatus0 = 0x1D,
	kStatus1 = 0x1E, /* CHG_STAT, VBUS_STAT */
	kFault0 = 0x1F,  /* VBUS/BAT/SYS/OTG fault, TSHUT, TS_STAT */
	kFlag0 = 0x20,
	kFlag1 = 0x21,
	kFaultFlag0 = 0x22,
	kAdcControl = 0x26,
	kAdcDisable0 = 0x27,
	kIbusAdc = 0x28,
	kIbatAdc = 0x2A,
	kVbusAdc = 0x2C,
	kVpmidAdc = 0x2E,
	kVbatAdc = 0x30,
	kVsysAdc = 0x32,
	kTsAdc = 0x34,
	kTdieAdc = 0x36,
	kPartInfo = 0x38,
};

/* Reset values used to recognise a freshly reset device (SLUSEG2D §8.6.2). */
constexpr uint16_t kChargeCurrentReset = 0x0340; /* 1040 mA */
constexpr uint16_t kChargeVoltageReset = 0x0D20; /* 4200 mV */

constexpr int clamp(int v, int lo, int hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

/* Charge current, 80-3520 mA in 80 mA steps (rounded down). */
constexpr uint16_t encode_ichg(int ma)
{
	return static_cast<uint16_t>((clamp(ma, 80, 3520) / 80) << 6);
}
constexpr int decode_ichg(uint16_t word)
{
	return ((word >> 6) & 0x3F) * 80;
}

/* Input current limit, 100-3200 mA in 20 mA steps (rounded down). */
constexpr uint16_t encode_iindpm(int ma)
{
	return static_cast<uint16_t>((clamp(ma, 100, 3200) / 20) << 4);
}
constexpr int decode_iindpm(uint16_t word)
{
	return ((word >> 4) & 0xFF) * 20;
}

/* Battery regulation voltage, 3500-4800 mV in 10 mV steps. */
constexpr uint16_t encode_vreg(int mv)
{
	return static_cast<uint16_t>((clamp(mv, 3500, 4800) / 10) << 3);
}
constexpr int decode_vreg(uint16_t word)
{
	return ((word >> 3) & 0x1FF) * 10;
}

/* REG0x16 Charger Control 1. */
constexpr uint8_t kForceIbatdis = 1U << 6; /* ~30 mA forced BAT discharge */
constexpr uint8_t kEnChg = 1U << 5;
constexpr uint8_t kEnHiz = 1U << 4;
constexpr uint8_t kWdRst = 1U << 2;
constexpr uint8_t kWatchdogMask = 0x03; /* 00 off, 01 50 s (reset), 10 100 s, 11 200 s */

/* REG0x1B NTC Control 1: TS_TH4_TH5_TH6 occupies bits 4:2. */
constexpr uint8_t kTh456Mask = 0x07U << 2;
constexpr uint8_t set_th456(uint8_t reg, uint8_t code)
{
	return static_cast<uint8_t>((reg & ~kTh456Mask) | ((code & 0x07U) << 2));
}
constexpr uint8_t get_th456(uint8_t reg)
{
	return (reg >> 2) & 0x07U;
}

/* REG0x26 ADC Control: enable, continuous, 12-bit effective resolution. */
constexpr uint8_t kAdcEnableContinuous12Bit = 0x80;

/* ADC decoding (SLUSEG2D §8.6.2.30-36). */
constexpr int16_t sign_extend(uint16_t value, unsigned bits)
{
	const uint16_t sign = static_cast<uint16_t>(1U << (bits - 1));
	const uint16_t mask = static_cast<uint16_t>((1U << bits) - 1);

	value &= mask;
	return static_cast<int16_t>((value ^ sign) - sign);
}

/* IBUS: bits 15:1, two's complement, 2 mA/LSB, positive into the charger. */
constexpr int ibus_ma(uint16_t word)
{
	return sign_extend(static_cast<uint16_t>(word >> 1), 15) * 2;
}

/* IBAT: bits 15:2, two's complement, 4 mA/LSB, positive while charging.
 * Code 0x2000 (word 0x8000) marks an aborted conversion. */
constexpr bool ibat_valid(uint16_t word)
{
	return word != 0x8000;
}
constexpr int ibat_ma(uint16_t word)
{
	return sign_extend(static_cast<uint16_t>(word >> 2), 14) * 4;
}

/* VBUS and VPMID: bits 14:2, 3.97 mV/LSB. */
constexpr int vbus_mv(uint16_t word)
{
	return static_cast<int>(((word >> 2) & 0x1FFFU) * 397U / 100U);
}

/* VBAT and VSYS: bits 12:1, 1.99 mV/LSB. */
constexpr int vbat_mv(uint16_t word)
{
	return static_cast<int>(((word >> 1) & 0x0FFFU) * 199U / 100U);
}

/* TS: bits 11:0, percentage of the bias reference, 0.0961 %/LSB.
 * Returned in tenths of a percent. */
constexpr int ts_permille(uint16_t word)
{
	return static_cast<int>((word & 0x0FFFU) * 961U / 1000U);
}

/* TDIE: bits 11:0, two's complement, 0.5 degC/LSB. Returned in tenths. */
constexpr int tdie_decic(uint16_t word)
{
	return sign_extend(word, 12) * 5;
}

/* REG0x38 Part Information: PN bits 5:3 (0 = BQ25620), DEV_REV bits 2:0. */
constexpr bool is_bq25620(uint8_t part)
{
	return ((part >> 3) & 0x07U) == 0;
}

/* REG0x1E Charger Status 1. */
constexpr uint8_t chg_stat(uint8_t status1)
{
	return (status1 >> 3) & 0x03U;
}
constexpr uint8_t vbus_stat(uint8_t status1)
{
	return status1 & 0x07U;
}

/* REG0x1F FAULT Status 0. */
constexpr uint8_t ts_stat(uint8_t fault0)
{
	return fault0 & 0x07U;
}
constexpr bool any_fault(uint8_t fault0)
{
	return (fault0 & 0xF8U) != 0; /* VBUS, BAT, SYS, OTG fault or thermal shutdown */
}

enum class VbusStat : uint8_t {
	kNone = 0,
	kSdp = 1,
	kCdp = 2,
	kDcp = 3,
	kUnknown = 4,
	kNonStandard = 5,
	kHvdcp = 6,
	kOtg = 7,
};

const char *chg_stat_name(uint8_t stat);
const char *vbus_stat_name(uint8_t stat);
const char *ts_stat_name(uint8_t stat);

} // namespace badge::bq25620
