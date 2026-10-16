/* SPDX-License-Identifier: MIT */
#include "badge/motion_runtime.hpp"
#include "badge/nfc_platform.hpp"
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(badge_motion, CONFIG_APP_LOG_LEVEL);
namespace badge
{
namespace
{
const device *const imu = DEVICE_DT_GET(DT_NODELABEL(imu));
K_MUTEX_DEFINE(motion_mutex);
K_SEM_DEFINE(data_ready, 0, 1);
K_SEM_DEFINE(first_sample, 0, 1);
const sensor_trigger trigger{SENSOR_TRIG_DATA_READY, SENSOR_CHAN_ALL};
struct Snapshot {
	int32_t a[3]{};
	uint32_t at = 0, sequence = 0;
	int error = -ENODEV;
	bool interrupt = false;
} sample;
uint32_t consumed;
uint8_t status[16] = {1};
uint32_t status_sample_at;
bool status_sampled;
bool started;
void on_ready(const device *, const sensor_trigger *)
{
	k_sem_give(&data_ready);
}
void worker(void *, void *, void *)
{
	bool armed = false;
	bool first = true;
	int previous_error = -ENODEV;
	for (;;) {
		int rc = -ENODEV;
		bool interrupt = false;
		int32_t acceleration[3]{};
		if (device_is_ready(imu)) {
			if (!armed) {
				rc = bench_bus_lock();
				if (rc == 0) {
					rc = sensor_trigger_set(imu, &trigger, on_ready);
					bench_bus_unlock();
				}
				armed = rc == 0;
			}
			interrupt = armed;
			// Coalesce DRDY at 100 Hz to at most 50 fetches/s. A timed fallback
			// keeps sampling available if the interrupt wire is absent.
			if (armed) {
				(void)k_sem_take(&data_ready, K_MSEC(40));
			}
			rc = bench_bus_lock();
			if (rc == 0) {
				sensor_value values[3];
				rc = sensor_sample_fetch(imu);
				if (rc == 0) {
					rc = sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, values);
				}
				if (rc == 0) {
					for (unsigned i = 0; i < 3; ++i) {
						acceleration[i] = sensor_value_to_milli(&values[i]);
					}
				}
				bench_bus_unlock();
			}
		}
		k_mutex_lock(&motion_mutex, K_FOREVER);
		sample.error = rc;
		sample.interrupt = interrupt;
		if (rc == 0) {
			for (unsigned i = 0; i < 3; ++i) {
				sample.a[i] = acceleration[i];
			}
			sample.at = k_uptime_get_32();
			++sample.sequence;
		}
		k_mutex_unlock(&motion_mutex);
		if (first) {
			k_sem_give(&first_sample);
			first = false;
		}
		if ((rc == 0) != (previous_error == 0)) {
			LOG_INF("IMU sample state %d", rc);
		}
		previous_error = rc;
		k_sleep(K_MSEC(rc == 0 ? 20 : 100));
	}
}
K_THREAD_DEFINE(motion_thread, 2048, worker, nullptr, nullptr, nullptr, 7, 0, SYS_FOREVER_MS);
} // namespace
void start_motion()
{
	if (!started) {
		started = true;
		k_thread_start(motion_thread);
		// One bounded startup attempt lets QR capture orientation before its
		// first presentation. Missing/busy IMU falls back to upright, not a late
		// rotation of an already visible QR.
		(void)k_sem_take(&first_sample, K_MSEC(100));
	}
}
void poll_motion(Modes &modes, uint32_t now)
{
	k_mutex_lock(&motion_mutex, K_FOREVER);
	const Snapshot latest = sample;
	k_mutex_unlock(&motion_mutex);
	const uint32_t age = k_uptime_get_32() - latest.at;
	if (latest.sequence != consumed) {
		consumed = latest.sequence;
		if (age <= 250) {
			modes.motion_sample(latest.a, now - age);
		}
	}
	modes.tick(now);
	const auto &motion = modes.motion();
	k_mutex_lock(&motion_mutex, K_FOREVER);
	status[0] = 1;
	status[1] = (device_is_ready(imu) ? 1 : 0) | (motion.fresh(now) ? 2 : 0) |
		    (latest.interrupt ? 4 : 0);
	status[2] = static_cast<uint8_t>(modes.display_rotation());
	status[3] = 0;
	status_sampled = motion.age(now) != UINT32_MAX;
	status_sample_at = k_uptime_get_32() - motion.age(now);
	const int32_t gx = motion.x() < -32767 ? -32767 : motion.x() > 32767 ? 32767 : motion.x();
	const int32_t gy = motion.y() < -32767 ? -32767 : motion.y() > 32767 ? 32767 : motion.y();
	sys_put_le16(static_cast<uint16_t>(static_cast<int16_t>(gx)), status + 6);
	sys_put_le16(static_cast<uint16_t>(static_cast<int16_t>(gy)), status + 8);
	sys_put_le16(0, status + 10);
	sys_put_le32(static_cast<uint32_t>(latest.error), status + 12);
	k_mutex_unlock(&motion_mutex);
}
void motion_status(uint8_t output[16])
{
	k_mutex_lock(&motion_mutex, K_FOREVER);
	memcpy(output, status, 16);
	const uint32_t age = status_sampled ? k_uptime_get_32() - status_sample_at : UINT32_MAX;
	if (age > 250) {
		output[1] &= ~2;
	}
	sys_put_le16(age > 65535 ? 65535 : age, output + 4);
	k_mutex_unlock(&motion_mutex);
}
} // namespace badge
