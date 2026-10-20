/*
 * SPDX-License-Identifier: MIT
 *
 * Bring-up self-tests through Zephyr driver APIs:
 * - IMU: identity (checked by the driver at init), one sample, gravity
 *   magnitude at rest, and the data-ready interrupt rate.
 * - NFC tag (NT3H2111): delegated guarded read/session setup, or runtime NFC
 *   status when the service owns the tag. No EEPROM writes in the self-check.
 * - Rev A power: reset cause, clock, rails, charger and inputs (power_diag.cpp).
 * - Rev A matrix: MTX_SAFE_SENSE and TLC chain readback, outputs off (matrix_diag.cpp).
 * - Microphone: PDM capture on both channels; mean, AC RMS and range of each. Raw
 *   samples stay in the capture buffers and are discarded; only levels are
 *   logged.
 */

#include "badge/bringup.hpp"
#include "badge/audio_capture.hpp"
#include "badge/nfc_platform.hpp"
#include "badge/power_diag.hpp"
#include "badge/matrix_diag.hpp"

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#if defined(CONFIG_SENSOR)
#include <zephyr/drivers/sensor.h>
#endif
#if defined(CONFIG_I2C)
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#endif
#if defined(CONFIG_AUDIO_DMIC)
#include <zephyr/audio/dmic.h>
#endif

LOG_MODULE_REGISTER(bringup, CONFIG_APP_LOG_LEVEL);

#define USER_NODE DT_PATH(zephyr_user)
#define IMU_NODE  DT_NODELABEL(imu)
#define PDM_NODE  DT_NODELABEL(pdm0)

#if DT_NODE_HAS_STATUS_OKAY(IMU_NODE) && defined(CONFIG_SENSOR)
#define HAVE_IMU 1
#else
#define HAVE_IMU 0
#endif
#if DT_NODE_HAS_PROP(USER_NODE, ntag_i2c_bus) && defined(CONFIG_I2C)
#define HAVE_NTAG 1
#else
#define HAVE_NTAG 0
#endif
#if DT_NODE_HAS_STATUS_OKAY(PDM_NODE) && defined(CONFIG_AUDIO_DMIC)
#define HAVE_MIC 1
#else
#define HAVE_MIC 0
#endif

namespace badge
{
namespace
{

[[maybe_unused]] uint32_t isqrt(uint64_t v)
{
	uint64_t r = 0;
	uint64_t bit = 1ULL << 62;

	while (bit > v) {
		bit >>= 2;
	}
	while (bit != 0) {
		if (v >= r + bit) {
			v -= r + bit;
			r = (r >> 1) + bit;
		} else {
			r >>= 1;
		}
		bit >>= 2;
	}
	return static_cast<uint32_t>(r);
}

#if HAVE_IMU
const struct device *const imu = DEVICE_DT_GET(IMU_NODE);
atomic_t drdy_count;
/* The driver keeps a pointer to the trigger, so it must stay in scope. */
const struct sensor_trigger drdy_trigger = {SENSOR_TRIG_DATA_READY, SENSOR_CHAN_ALL};

void on_drdy(const struct device *, const struct sensor_trigger *)
{
	atomic_inc(&drdy_count);
}

int imu_read(int32_t accel_mms2[3], int32_t gyro_mdps[3])
{
	struct sensor_value a[3];
	struct sensor_value g[3];
	int err = -EAGAIN;

	/* The first samples after power-up are not ready yet (-EAGAIN). */
	for (int i = 0; i < 10 && err == -EAGAIN; i++) {
		err = bench_bus_lock();
		if (err == 0) {
			err = sensor_sample_fetch(imu);
			bench_bus_unlock();
		}
		if (err == -EAGAIN) {
			k_msleep(20);
		}
	}
	if (err == 0) {
		err = sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, a);
	}
	if (err == 0) {
		err = sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, g);
	}
	if (err == 0) {
		for (int i = 0; i < 3; i++) {
			accel_mms2[i] = static_cast<int32_t>(sensor_value_to_milli(&a[i]));
			/* rad/s to milli-degrees/s */
			gyro_mdps[i] = static_cast<int32_t>(sensor_value_to_micro(&g[i]) * 180000 /
							    3141593);
		}
	}
	return err;
}

int imu_check()
{
	int32_t a[3];
	int32_t g[3];
	int failed = 0;

	if (!device_is_ready(imu)) {
		LOG_ERR("IMU: FAIL, not ready (bus or identity check failed, see driver log)");
		return 1;
	}
	LOG_INF("IMU: identity OK (WHO_AM_I checked by the driver)");

	int err = imu_read(a, g);

	if (err != 0) {
		LOG_ERR("IMU: FAIL, sample read %d", err);
		return 1;
	}
	uint32_t mag = isqrt(static_cast<uint64_t>(static_cast<int64_t>(a[0]) * a[0] +
						   static_cast<int64_t>(a[1]) * a[1] +
						   static_cast<int64_t>(a[2]) * a[2]));

	LOG_INF("IMU: accel %d %d %d mm/s2 (|a| %u), gyro %d %d %d mdps", a[0], a[1], a[2], mag,
		g[0], g[1], g[2]);
	/* At rest the accelerometer reads gravity, 9807 mm/s2. */
	if (mag < 8800 || mag > 10800) {
		LOG_WRN("IMU: CHECK, |a| not near 9807 mm/s2; keep the module still");
		failed++;
	}

	atomic_set(&drdy_count, 0);
	err = bench_bus_lock();
	if (err == 0) {
		err = sensor_trigger_set(imu, &drdy_trigger, on_drdy);
		bench_bus_unlock();
	}
	if (err != 0) {
		LOG_ERR("IMU: FAIL, data-ready trigger %d", err);
		return failed + 1;
	}
	k_msleep(1000);
	int rate = static_cast<int>(atomic_get(&drdy_count));

	LOG_INF("IMU: %d data-ready interrupts in 1 s (ODR 100 Hz)", rate);
	if (rate < 80 || rate > 120) {
		LOG_ERR("IMU: FAIL, interrupt rate");
		failed++;
	}
	return failed;
}
#endif

#if HAVE_NTAG
int ntag_check()
{
	return nfc_bringup_check();
}
void ntag_log_field()
{
	nfc_log_field();
}
#endif

#if HAVE_MIC
const struct device *const mic = DEVICE_DT_GET(PDM_NODE);
constexpr auto kCapture = audio_capture_profile(IS_ENABLED(CONFIG_SOC_NRF52840));
constexpr uint32_t kRate = kCapture.rate;
constexpr uint32_t kBlockMs = 100;
constexpr uint32_t kChannels = 2;
constexpr uint32_t kBlockBytes = kRate * kBlockMs / 1000 * kChannels * sizeof(int16_t);
K_MEM_SLAB_DEFINE_STATIC(mic_slab, kBlockBytes, 4, 4);
bool mic_configured;

int mic_configure()
{
	struct pcm_stream_cfg stream = {};
	struct dmic_cfg cfg = {};

	stream.pcm_rate = kRate;
	stream.pcm_width = 16;
	stream.block_size = kBlockBytes;
	stream.mem_slab = &mic_slab;
	/* MP34DT01-M: clock 1-3.25 MHz, 2.4 MHz typical, duty cycle 40-60 % (ST
	 * DocID026514 Rev 3). At 16 kHz the driver chose 1.25 MHz and the quiet
	 * level was over 3 times higher than at about 2 MHz (bench, 2026-10-11),
	 * so the clock is kept near 2 MHz with a 32 kHz output rate. */
	cfg.io.min_pdm_clk_freq = kCapture.min_clock;
	cfg.io.max_pdm_clk_freq = kCapture.max_clock;
	cfg.io.min_pdm_clk_dc = 40;
	cfg.io.max_pdm_clk_dc = 60;
	cfg.streams = &stream;
	cfg.channel.req_num_streams = 1;
	cfg.channel.req_num_chan = kChannels;
	/* Both channels, so the check works whichever edge L/R selects. */
	cfg.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT) |
				      dmic_build_channel_map(1, 0, PDM_CHAN_RIGHT);
	return dmic_configure(mic, &cfg);
}

struct MicStats {
	int32_t mean[2];
	uint32_t ac_rms[2];
	int32_t min[2];
	int32_t max[2];
	uint32_t pairs;
};

int mic_measure(MicStats &st)
{
	int64_t sum[2] = {};
	uint64_t sum_sq[2] = {};
	int err;

	st = MicStats{};
	st.min[0] = st.min[1] = INT16_MAX;
	st.max[0] = st.max[1] = INT16_MIN;
	if (!mic_configured) {
		err = mic_configure();
		if (err != 0) {
			return err;
		}
		mic_configured = true;
	}
	err = dmic_trigger(mic, DMIC_TRIGGER_START);
	if (err != 0) {
		return err;
	}
	/* Skip the first 200 ms (start-up time), then measure 300 ms. */
	for (int blk = 0; blk < 5 && err == 0; blk++) {
		void *buf;
		size_t size;

		err = dmic_read(mic, 0, &buf, &size, 2 * kBlockMs);
		if (err != 0) {
			break;
		}
		if (blk >= 2) {
			const int16_t *s = static_cast<const int16_t *>(buf);

			for (size_t i = 0; i + 1 < size / sizeof(int16_t); i += 2) {
				for (int c = 0; c < 2; c++) {
					int32_t v = s[i + c];

					sum[c] += v;
					sum_sq[c] +=
						static_cast<uint64_t>(static_cast<int64_t>(v) * v);
					st.min[c] = MIN(st.min[c], v);
					st.max[c] = MAX(st.max[c], v);
				}
				st.pairs++;
			}
		}
		k_mem_slab_free(&mic_slab, buf);
	}
	(void)dmic_trigger(mic, DMIC_TRIGGER_STOP);
	if (err == 0 && st.pairs > 0) {
		for (int c = 0; c < 2; c++) {
			int64_t mean = sum[c] / st.pairs;
			uint64_t mean_sq = sum_sq[c] / st.pairs;
			uint64_t dc_sq = static_cast<uint64_t>(mean * mean);

			st.mean[c] = static_cast<int32_t>(mean);
			st.ac_rms[c] = isqrt(mean_sq > dc_sq ? mean_sq - dc_sq : 0);
		}
	}
	return err;
}

void mic_log(const MicStats &st)
{
	LOG_INF("Mic: left mean %d AC RMS %u range %d..%d; right mean %d AC RMS %u range %d..%d",
		st.mean[0], st.ac_rms[0], st.min[0], st.max[0], st.mean[1], st.ac_rms[1], st.min[1],
		st.max[1]);
}

int mic_check()
{
	MicStats st;

	if (!device_is_ready(mic)) {
		LOG_ERR("Mic: FAIL, PDM not ready");
		return 1;
	}
	int err = mic_measure(st);

	if (err != 0) {
		LOG_ERR("Mic: FAIL, capture %d", err);
		return 1;
	}
	mic_log(st);
	/* A PDM mic sends about as many ones as zeros, so its decimated output
	 * averages near zero. A DATA line nothing drives sits at one level
	 * (pulled down on the DK) and decimates to a large constant offset: -8906
	 * with nothing connected, against a mean of about 60 with the bench mic
	 * (2026-10-11). The idle half-cycle of a real mic can hold the previous
	 * bit, so both channels may carry nearly the same data. */
	constexpr int32_t kMaxDataMean = 4096;
	bool data = false;

	for (int c = 0; c < 2; c++) {
		data = data || (st.min[c] != st.max[c] && st.mean[c] > -kMaxDataMean &&
				st.mean[c] < kMaxDataMean);
	}
	if (!data) {
		LOG_ERR("Mic: FAIL, no microphone data on DATA (stuck at one level)");
		return 1;
	}
	LOG_INF("Mic: data present; confirm by sound: the AC RMS must rise when you clap");
	return 0;
}
#endif

} // namespace

int run_bringup_checks()
{
	int failed = power_diag_check();

	failed += matrix_chain_check();

#if HAVE_IMU
	failed += imu_check();
#endif
#if HAVE_NTAG
	failed += ntag_check();
#endif
#if HAVE_MIC
	failed += mic_check();
#endif
	LOG_INF("Bring-up checks done: %d failed", failed);
	return failed;
}

void log_bringup_readings()
{
#if HAVE_IMU
	int32_t a[3];
	int32_t g[3];

	if (device_is_ready(imu) && imu_read(a, g) == 0) {
		LOG_INF("IMU: accel %d %d %d mm/s2, gyro %d %d %d mdps, %d data-ready", a[0], a[1],
			a[2], g[0], g[1], g[2], static_cast<int>(atomic_get(&drdy_count)));
	}
#endif
#if HAVE_NTAG
	ntag_log_field();
#endif
#if HAVE_MIC
	MicStats st;

	if (device_is_ready(mic) && mic_measure(st) == 0) {
		mic_log(st);
	}
#endif
}

} // namespace badge
