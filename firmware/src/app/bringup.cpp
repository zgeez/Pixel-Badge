/*
 * SPDX-License-Identifier: MIT
 *
 * Bring-up self-tests through Zephyr driver APIs:
 * - IMU: identity (checked by the driver at init), one sample, gravity
 *   magnitude at rest, and the data-ready interrupt rate.
 * - NFC tag (NT3H2111): read block 00h over I2C (read only) and the field
 *   detect level. No EEPROM writes: they need the tag's write procedure.
 * - Microphone: PDM capture on both channels; mean, AC RMS and range of each. Raw
 *   samples stay in the capture buffers and are discarded; only levels are
 *   logged.
 */

#include "badge/bringup.hpp"

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
		err = sensor_sample_fetch(imu);
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
	err = sensor_trigger_set(imu, &drdy_trigger, on_drdy);
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
const struct device *const ntag_bus = DEVICE_DT_GET(DT_PHANDLE(USER_NODE, ntag_i2c_bus));
constexpr uint16_t kNtagAddr = 0x55;

#if DT_NODE_HAS_PROP(USER_NODE, nfc_fd_gpios) && defined(CONFIG_GPIO)
/*
 * Every I2C transaction is guarded by field detect, as docs/nfc.md requires
 * (NXP errata ES_NT3H2111_2211 section 2.1): RF activity during I2C can turn a
 * read into an unintended EEPROM write. FD must signal RF field on and off
 * (the tag's FD setting; not yet verified on the bench click). The FD latency
 * is not measured on the DK yet, so kFdLatencyUs is an assumed margin.
 */
const struct gpio_dt_spec ntag_fd = GPIO_DT_SPEC_GET(USER_NODE, nfc_fd_gpios);
struct gpio_callback ntag_fd_cb;
atomic_t ntag_fd_edges;
constexpr uint32_t kFdLatencyUs = 500;
constexpr uint32_t kFrameGapUs = 60; /* at least 50 us between frames */

void on_ntag_fd(const struct device *, struct gpio_callback *, uint32_t)
{
	atomic_inc(&ntag_fd_edges);
}

bool ntag_field_active()
{
	return gpio_pin_get_dt(&ntag_fd) > 0;
}

/* Reads one 16-byte block: block address and data as two frames, each with
 * START and STOP, inside an FD watch window. No I2C runs while a field is
 * present: rf_seen is set and nothing is sent if FD is active when the
 * window opens, and also if an edge or an active FD is seen by its end. */
int ntag_read_block(uint8_t block, uint8_t out[16], bool &rf_seen)
{
	atomic_set(&ntag_fd_edges, 0);
	int err = gpio_pin_interrupt_configure_dt(&ntag_fd, GPIO_INT_EDGE_BOTH);

	if (err != 0) {
		return err;
	}
	/* Armed first, so a field arriving after this check is still caught. */
	rf_seen = ntag_field_active();
	if (!rf_seen) {
		k_busy_wait(20 + kFdLatencyUs);
		rf_seen = ntag_field_active() || atomic_get(&ntag_fd_edges) != 0;
	}
	if (!rf_seen) {
		err = i2c_write(ntag_bus, &block, 1, kNtagAddr);
		if (err == 0) {
			k_busy_wait(kFrameGapUs);
			err = i2c_read(ntag_bus, out, 16, kNtagAddr);
		}
		k_busy_wait(20 + kFdLatencyUs);
		rf_seen = ntag_field_active() || atomic_get(&ntag_fd_edges) != 0;
	}
	(void)gpio_pin_interrupt_configure_dt(&ntag_fd, GPIO_INT_DISABLE);
	return err;
}

int ntag_check()
{
	uint8_t b[16];

	if (!device_is_ready(ntag_bus) || !gpio_is_ready_dt(&ntag_fd)) {
		LOG_ERR("NFC tag: FAIL, I2C bus or field-detect GPIO not ready");
		return 1;
	}
	/* FD is open drain; the pull-up keeps an unconnected pin inactive. */
	int err = gpio_pin_configure_dt(&ntag_fd, GPIO_INPUT | GPIO_PULL_UP);

	if (err == 0 && ntag_fd_cb.handler == NULL) {
		gpio_init_callback(&ntag_fd_cb, on_ntag_fd, BIT(ntag_fd.pin));
		err = gpio_add_callback(ntag_fd.port, &ntag_fd_cb);
	}
	if (err != 0) {
		LOG_ERR("NFC tag: FAIL, field-detect setup %d", err);
		return 1;
	}
	bool rf_seen = false;

	err = ntag_read_block(0x00, b, rf_seen);
	if (rf_seen) {
		/* The check does not retry: RF activity around a read may have
		 * written EEPROM, so the tag's address, configuration and NDEF
		 * blocks need full verification before use (docs/nfc.md), which
		 * this read-only check does not do. */
		LOG_ERR("NFC tag: FAIL, RF field or activity during the check; keep phones "
			"away, and re-verify the tag before use");
		return 1;
	}
	if (err != 0) {
		LOG_ERR("NFC tag: FAIL, no answer at 0x%02x (%d)", kNtagAddr, err);
		return 1;
	}
	LOG_HEXDUMP_INF(b, sizeof(b), "NFC tag: block 00h");
	/* Byte 0 of block 00h always reads 04h over I2C (docs/nfc.md). */
	if (b[0] != 0x04) {
		LOG_ERR("NFC tag: FAIL, block 00h byte 0 is 0x%02x, expected 0x04", b[0]);
		return 1;
	}
	LOG_INF("NFC tag: OK, CC %02x %02x %02x %02x", b[12], b[13], b[14], b[15]);
	return 0;
}
#else
int ntag_check()
{
	/* Without field detect the read cannot be guarded (docs/nfc.md). */
	LOG_WRN("NFC tag: skipped, no field-detect line to guard I2C reads");
	return 0;
}
#endif
#endif

#if HAVE_MIC
const struct device *const mic = DEVICE_DT_GET(PDM_NODE);
constexpr uint32_t kRate = 16000;
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
	/* MP34DT01-M: clock 1-3.25 MHz, duty cycle 40-60 % (ST DocID026514 Rev 3). */
	cfg.io.min_pdm_clk_freq = 1000000;
	cfg.io.max_pdm_clk_freq = 3250000;
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
	/* With DATA pulled down, an open pin reads constant on both channels. A
	 * floating pin gives the same statistics on both channels. A mic drives
	 * one channel, so that channel varies and the two differ. */
	bool constant = st.min[0] == st.max[0] && st.min[1] == st.max[1];
	bool same = st.mean[0] == st.mean[1] && st.ac_rms[0] == st.ac_rms[1] &&
		    st.min[0] == st.min[1] && st.max[0] == st.max[1];

	if (constant || same) {
		LOG_ERR("Mic: FAIL, no microphone data on DATA (%s)",
			constant ? "constant" : "both channels alike");
		return 1;
	}
	LOG_INF("Mic: data present; confirm by sound: the AC RMS must rise when you clap");
	return 0;
}
#endif

} // namespace

int run_bringup_checks()
{
	int failed = 0;

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
#if HAVE_MIC
	MicStats st;

	if (device_is_ready(mic) && mic_measure(st) == 0) {
		mic_log(st);
	}
#endif
}

} // namespace badge
