/*
 * SPDX-License-Identifier: MIT
 *
 * Bench battery logger (CONFIG_APP_BATTERY_LOG): discharge records in the
 * flash reserve, read back over SWD with scripts/bench/battery_log.py.
 * Pure record encoding and session rules; flash access is in
 * src/app/battery_log.cpp.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace badge::battery_log
{

/* Record flags. kStart marks a time base reset: a new session or a resume
 * after an MCU reset (seconds restart at 0). */
constexpr uint8_t kLoaded = 1;      /* LED rail on during the SAADC sample */
constexpr uint8_t kUsb = 2;         /* USB power present */
constexpr uint8_t kCharging = 4;    /* charger reports a charge phase */
constexpr uint8_t kShed = 8;        /* matrix shed by the load-shed policy */
constexpr uint8_t kLowPower = 16;   /* lowest-power threshold reached */
constexpr uint8_t kStart = 32;      /* session start or resume */
constexpr uint8_t kEnd = 64;        /* session closed: USB back or System OFF */
constexpr uint8_t kNoCurrent = 128; /* no valid IBAT conversion in the period */

/* 16 bytes, four little-endian words; an erased slot reads all 0xFF. */
struct Record {
	uint32_t seconds;    /* since the session start or resume */
	uint16_t saadc_mv;   /* VBAT divider (SAADC), 0 if none */
	uint16_t charger_mv; /* VBAT from the charger ADC, 0 if none */
	int16_t ibat_ma;     /* mean battery current; negative while discharging */
	uint8_t ceiling;     /* matrix brightness ceiling */
	uint8_t flags;
	int16_t tdie_decic;   /* charger die temperature, 0.1 degC */
	uint16_t ts_permille; /* charger TS as per mille of REGN (board NTC) */
};
constexpr size_t kRecordSize = 16;

inline void put16(uint8_t *out, uint16_t value)
{
	out[0] = static_cast<uint8_t>(value);
	out[1] = static_cast<uint8_t>(value >> 8);
}

inline uint16_t get16(const uint8_t *in)
{
	return static_cast<uint16_t>(in[0] | in[1] << 8);
}

inline void encode(const Record &record, uint8_t out[kRecordSize])
{
	put16(out, static_cast<uint16_t>(record.seconds));
	put16(out + 2, static_cast<uint16_t>(record.seconds >> 16));
	put16(out + 4, record.saadc_mv);
	put16(out + 6, record.charger_mv);
	put16(out + 8, static_cast<uint16_t>(record.ibat_ma));
	out[10] = record.ceiling;
	out[11] = record.flags;
	put16(out + 12, static_cast<uint16_t>(record.tdie_decic));
	put16(out + 14, record.ts_permille);
}

inline Record decode(const uint8_t in[kRecordSize])
{
	Record record{};

	record.seconds = get16(in) | static_cast<uint32_t>(get16(in + 2)) << 16;
	record.saadc_mv = get16(in + 4);
	record.charger_mv = get16(in + 6);
	record.ibat_ma = static_cast<int16_t>(get16(in + 8));
	record.ceiling = in[10];
	record.flags = in[11];
	record.tdie_decic = static_cast<int16_t>(get16(in + 12));
	record.ts_permille = get16(in + 14);
	return record;
}

inline bool erased(const uint8_t in[kRecordSize])
{
	for (size_t i = 0; i < kRecordSize; ++i) {
		if (in[i] != 0xFF) {
			return false;
		}
	}
	return true;
}

/* Mean of a running sum, rounded toward zero, 0 for no samples. */
constexpr int16_t mean_ma(int32_t sum, uint32_t count)
{
	return count == 0 ? 0 : static_cast<int16_t>(sum / static_cast<int32_t>(count));
}

/* What the logger does at boot, from the last record already in flash. */
enum class Boot : uint8_t {
	kIdle,   /* no open session: wait for the next USB unplug */
	kResume, /* open session and still on battery: append a start record */
	kClose,  /* open session but USB is back: append an end record */
};

constexpr Boot boot_action(bool have_last, uint8_t last_flags, bool full, bool usb)
{
	if (!have_last || full || (last_flags & kEnd) != 0) {
		return Boot::kIdle;
	}
	return usb ? Boot::kClose : Boot::kResume;
}

/* Logger inputs, sampled by the power manager on each 240 ms tick. */
struct Input {
	uint32_t now_ms;
	bool usb, charging, loaded, shed, low_power;
	int saadc_mv;   /* last valid VBAT divider reading, -1 if none */
	int charger_mv; /* charger ADC VBAT, -1 if the last read failed */
	bool ibat_valid;
	int ibat_ma;
	int tdie_decic;
	int ts_permille;
	uint8_t ceiling;
};

/* Flash backend (CONFIG_APP_BATTERY_LOG). A session starts once USB has been
 * present and then stays absent for 10 s, appended after any earlier
 * sessions: firmware never erases the log (battery_log.py clears it over
 * SWD), so a recharge cannot lose an unread recording. Records follow every
 * CONFIG_APP_BATTERY_LOG_PERIOD_S with the mean IBAT over the period, plus
 * one at the first low-power reading. USB returning or close() writes an end
 * record. An MCU reset during a session resumes it. Logging stops when the
 * reserve is full. */
void tick(const Input &input);
void close(const Input &input);

} // namespace badge::battery_log
