/* SPDX-License-Identifier: MIT */
#include "badge/battery_log.hpp"

#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(battery_log, CONFIG_APP_LOG_LEVEL);

namespace badge::battery_log
{
namespace
{
constexpr uint32_t kPeriodMs = CONFIG_APP_BATTERY_LOG_PERIOD_S * 1000U;
constexpr uint32_t kUnplugMs = 10000; /* USB absent this long starts a session */

enum class State : uint8_t {
	kInit,
	kIdle,
	kLogging,
	kFull,
	kFault
};

State state = State::kInit;
const flash_area *area;
size_t capacity; /* records */
size_t next;     /* first erased slot */
bool usb_seen;
uint32_t usb_gone_ms;
bool usb_gone;
uint32_t session_ms;
uint32_t written_ms;
bool low_logged;
int32_t ibat_sum;
uint32_t ibat_count;

uint16_t clamp_mv(int mv)
{
	return mv <= 0 ? 0 : mv > 0xFFFF ? 0xFFFF : static_cast<uint16_t>(mv);
}

int append(const Input &in, uint8_t extra)
{
	if (next >= capacity) {
		state = State::kFull;
		LOG_WRN("Battery log: full after %u records; logging stopped",
			static_cast<unsigned>(next));
		return -ENOSPC;
	}
	Record record{};

	record.seconds = (in.now_ms - session_ms) / 1000U;
	record.saadc_mv = clamp_mv(in.saadc_mv);
	record.charger_mv = clamp_mv(in.charger_mv);
	record.ibat_ma = ibat_count != 0 ? mean_ma(ibat_sum, ibat_count) : 0;
	record.ceiling = in.ceiling;
	record.flags = static_cast<uint8_t>(extra | (in.loaded ? kLoaded : 0) |
					    (in.usb ? kUsb : 0) | (in.charging ? kCharging : 0) |
					    (in.shed ? kShed : 0) | (in.low_power ? kLowPower : 0) |
					    (ibat_count == 0 ? kNoCurrent : 0));
	record.tdie_decic = static_cast<int16_t>(in.tdie_decic);
	record.ts_permille = clamp_mv(in.ts_permille);

	uint8_t bytes[kRecordSize];

	encode(record, bytes);
	const int rc = flash_area_write(area, next * kRecordSize, bytes, sizeof(bytes));

	if (rc != 0) {
		state = State::kFault;
		LOG_ERR("Battery log: write failed (%d); logging stopped", rc);
		return rc;
	}
	++next;
	ibat_sum = 0;
	ibat_count = 0;
	written_ms = in.now_ms;
	return 0;
}

void start_session(const Input &in, const char *why)
{
	session_ms = in.now_ms;
	low_logged = false;
	ibat_sum = 0;
	ibat_count = 0;
	if (append(in, kStart) == 0) {
		state = State::kLogging;
		LOG_INF("Battery log: %s at record %u of %u", why, static_cast<unsigned>(next - 1),
			static_cast<unsigned>(capacity));
	}
}

void initialize(const Input &in)
{
	state = State::kFault;
	const int rc = flash_area_open(PARTITION_ID(reserved), &area);

	if (rc != 0) {
		LOG_ERR("Battery log: reserve unavailable (%d)", rc);
		return;
	}
	capacity = area->fa_size / kRecordSize;

	uint8_t bytes[kRecordSize];
	Record last{};

	for (next = 0; next < capacity; ++next) {
		if (flash_area_read(area, next * kRecordSize, bytes, sizeof(bytes)) != 0) {
			LOG_ERR("Battery log: read failed");
			return;
		}
		if (erased(bytes)) {
			break;
		}
		last = decode(bytes);
	}
	state = State::kIdle;
	LOG_INF("Battery log: %u of %u records used", static_cast<unsigned>(next),
		static_cast<unsigned>(capacity));
	switch (boot_action(next > 0, last.flags, next >= capacity, in.usb)) {
	case Boot::kResume:
		start_session(in, "resumed after reset");
		break;
	case Boot::kClose:
		session_ms = close_base_ms(in.now_ms, last.seconds);
		(void)append(in, kEnd);
		state = state == State::kFault ? state : State::kIdle;
		break;
	case Boot::kIdle:
		if (next >= capacity) {
			state = State::kFull;
		}
		break;
	}
}
} // namespace

void tick(const Input &in)
{
	if (state == State::kInit) {
		initialize(in);
	}
	if (in.usb) {
		usb_seen = true;
		usb_gone = false;
	} else if (!usb_gone) {
		usb_gone = true;
		usb_gone_ms = in.now_ms;
	}
	switch (state) {
	case State::kIdle:
		if (usb_seen && usb_gone && in.now_ms - usb_gone_ms >= kUnplugMs) {
			usb_seen = false;
			start_session(in, "USB removed; session started");
		}
		break;
	case State::kLogging:
		if (in.ibat_valid) {
			ibat_sum += in.ibat_ma;
			++ibat_count;
		}
		if (in.usb) {
			if (append(in, kEnd) == 0) {
				state = State::kIdle;
				LOG_INF("Battery log: USB back; session closed with %u records",
					static_cast<unsigned>(next));
			}
		} else if ((in.low_power && !low_logged) || in.now_ms - written_ms >= kPeriodMs) {
			low_logged = low_logged || in.low_power;
			(void)append(in, 0);
		}
		break;
	default:
		break;
	}
}

void close(const Input &in)
{
	if (state == State::kLogging && append(in, kEnd) == 0) {
		state = State::kIdle;
		LOG_INF("Battery log: closed before System OFF with %u records",
			static_cast<unsigned>(next));
	}
}

} // namespace badge::battery_log
