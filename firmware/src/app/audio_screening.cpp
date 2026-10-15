/* SPDX-License-Identifier: MIT */
#include "badge/audio_screening.hpp"
#include "badge/audio.hpp"
#include "badge/buttons.hpp"
#include <zephyr/device.h>
#include <zephyr/audio/dmic.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <errno.h>

LOG_MODULE_REGISTER(audio_screening, CONFIG_APP_LOG_LEVEL);
namespace badge
{
namespace
{
constexpr uint32_t kRate = 32000, kChannels = 2, kBlockMs = 20;
constexpr size_t kBlockBytes = kAudioBlockSamples * kChannels * sizeof(int16_t);
constexpr size_t kBuffers = 4;
K_MEM_SLAB_DEFINE_STATIC(capture_slab, kBlockBytes, kBuffers, 4);
const device *const microphone = DEVICE_DT_GET(DT_NODELABEL(pdm0));
const device *const display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
const gpio_dt_spec buttons[] = {GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
				GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios)};
AudioAnalyzer analyzer;
int16_t mono[kAudioBlockSamples]{};

int show(const Frame &frame)
{
	display_buffer_descriptor descriptor{};
	descriptor.buf_size = kFrameBytes;
	descriptor.width = descriptor.height = descriptor.pitch = kMatrixSize;
	return display_write(display, 0, 0, &descriptor, frame.bytes);
}
void release(void *buffer)
{
	clear_audio(buffer, kBlockBytes);
	k_mem_slab_free(&capture_slab, buffer);
}
int stop_capture()
{
	/* Nordic DMIC STOP is asynchronous. Never wipe memory still owned by DMA. */
	const int rc = dmic_trigger(microphone, DMIC_TRIGGER_STOP);
	if (rc != 0) {
		return rc;
	}
	const int64_t deadline = k_uptime_get() + 200;
	pcm_stream_cfg disabled_stream{};
	dmic_cfg disabled_config{};
	disabled_config.streams = &disabled_stream;
	disabled_config.channel.req_num_streams = 1;
	disabled_config.channel.req_num_chan = kChannels;
	disabled_config.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT) |
						  dmic_build_channel_map(1, 0, PDM_CHAN_RIGHT);
	while (true) {
		void *buffer = nullptr;
		size_t size = 0;
		while (dmic_read(microphone, 0, &buffer, &size, 0) == 0) {
			release(buffer);
		}
		const unsigned key = irq_lock();
		const uint32_t used = k_mem_slab_num_used_get(&capture_slab);
		irq_unlock(key);
		/* Zero used blocks alone does not prove an asynchronous clock start
		 * cannot begin later. A zero-rate reconfigure succeeds only after the
		 * driver is inactive and uninitializes PDM before pool erasure. */
		const int disabled =
			used == 0 ? dmic_configure(microphone, &disabled_config) : -EBUSY;
		if (disabled != 0 && disabled != -EBUSY) {
			return disabled;
		}
		if (disabled == 0) {
			/* Free slab blocks contain an intrusive list pointer; preserve it.
			 * All other PCM bytes are erased once the driver owns no buffers. */
			for (size_t block = 0; block < kBuffers; ++block) {
				clear_audio(capture_slab.buffer + block * kBlockBytes +
						    sizeof(void *),
					    kBlockBytes - sizeof(void *));
			}
			clear_audio(mono, sizeof(mono));
			analyzer.clear();
			return 0;
		}
		if (k_uptime_get() >= deadline) {
			return -ETIMEDOUT;
		}
		/* Also stop a transfer whose clock became ready after the first STOP. */
		(void)dmic_trigger(microphone, DMIC_TRIGGER_STOP);
		k_sleep(K_MSEC(1));
	}
}
int start_capture()
{
	pcm_stream_cfg stream{};
	dmic_cfg config{};
	stream.pcm_rate = kRate;
	stream.pcm_width = 16;
	stream.block_size = kBlockBytes;
	stream.mem_slab = &capture_slab;
	config.io.min_pdm_clk_freq = 1800000;
	config.io.max_pdm_clk_freq = 3250000;
	config.io.min_pdm_clk_dc = 40;
	config.io.max_pdm_clk_dc = 60;
	config.streams = &stream;
	config.channel.req_num_streams = 1;
	config.channel.req_num_chan = kChannels;
	config.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT) |
					 dmic_build_channel_map(1, 0, PDM_CHAN_RIGHT);
	int rc = dmic_configure(microphone, &config);
	if (rc == 0) {
		rc = dmic_trigger(microphone, DMIC_TRIGGER_START);
	}
	return rc;
}
} // namespace

void run_audio_screening()
{
	if (!device_is_ready(microphone) || !device_is_ready(display)) {
		LOG_ERR("audio bench device unavailable");
		return;
	}
	Button classifier[2];
	bool buttons_ready[2]{};
	for (unsigned i = 0; i < 2; ++i) {
		buttons_ready[i] = gpio_is_ready_dt(&buttons[i]) &&
				   gpio_pin_configure_dt(&buttons[i], GPIO_INPUT) == 0;
		classifier[i].reset(buttons_ready[i] && gpio_pin_get_dt(&buttons[i]) == 1, 0);
	}
	Frame frame;
	frame.clear();
	int rc = display_set_pixel_format(display, PIXEL_FORMAT_MONO01);
	if (rc == 0) {
		rc = display_set_brightness(display, 128);
	}
	if (rc == 0) {
		rc = show(frame);
	}
	if (rc == 0) {
		rc = display_blanking_off(display);
	}
	if (rc != 0) {
		LOG_ERR("audio bench display initialization failed (%d)", rc);
		return;
	}
	bool spectrum = false, paused = false;
	unsigned channel = CONFIG_APP_AUDIO_CHANNEL, skipped = 0;
	uint8_t brightness = 128;
	rc = start_capture();
	if (rc != 0) {
		LOG_ERR("audio bench capture start failed (%d)", rc);
		(void)stop_capture();
		display_blanking_on(display);
		return;
	}
	LOG_INF("audio bench running; samples stay volatile; no recording or BLE audio");
	const int64_t began = k_uptime_get();
	AudioAnalysis analysis{};
	while (rc == 0) {
		const uint32_t now = static_cast<uint32_t>(k_uptime_get() - began);
		for (unsigned i = 0; i < 2; ++i) {
			if (!buttons_ready[i]) {
				continue;
			}
			const int level = gpio_pin_get_dt(&buttons[i]);
			if (level < 0) {
				continue;
			}
			const Press press = classifier[i].sample(level == 1, now);
			if (i == 0 && press == Press::kShort) {
				spectrum = !spectrum;
			}
			if (i == 0 && press == Press::kLong) {
				if (!paused) {
					rc = stop_capture();
					if (rc != 0) {
						break;
					}
					paused = true;
				} else {
					rc = start_capture();
					if (rc != 0) {
						break;
					}
					paused = false;
					skipped = 0;
				}
				analysis = AudioAnalysis{};
				LOG_INF("audio bench %s", paused ? "paused" : "running");
			}
			if (i == 1 && press == Press::kLong) {
				channel ^= 1;
				analysis = AudioAnalysis{};
				LOG_INF("audio bench channel %u", channel);
			}
			if (i == 1 && press == Press::kShort) {
				brightness = brightness == 16    ? 64
					     : brightness == 64  ? 128
					     : brightness == 128 ? 255
								 : 16;
				rc = display_set_brightness(display, brightness);
				if (rc != 0) {
					break;
				}
			}
		}
		if (rc != 0) {
			break;
		}
		if (paused) {
			frame.clear();
			/* Pause symbol; PDM remains stopped. */
			for (int y = 8; y < 24; ++y) {
				frame.set(11, y);
				frame.set(20, y);
			}
			rc = show(frame);
			if (rc == -EBUSY) {
				rc = 0;
			}
			k_sleep(K_MSEC(kBlockMs));
			continue;
		}
		void *buffer = nullptr;
		size_t bytes = 0;
		rc = dmic_read(microphone, 0, &buffer, &bytes, 100);
		if (rc != 0) {
			break;
		}
		if (buffer == nullptr || bytes != kBlockBytes) {
			if (buffer != nullptr) {
				release(buffer);
			}
			rc = -EIO;
			break;
		}
		const auto *stereo = static_cast<const int16_t *>(buffer);
		if (skipped < 10) {
			++skipped; // Discard startup transients for 200 ms.
			release(buffer);
			continue;
		}
		for (size_t i = 0; i < kAudioBlockSamples; ++i) {
			mono[i] = stereo[i * 2 + channel];
		}
		release(buffer);
		const bool valid = analyzer.analyze(mono, kAudioBlockSamples, analysis);
		clear_audio(mono, sizeof(mono));
		if (!valid) {
			rc = -EINVAL;
			break;
		}
		render_audio(frame, analysis, spectrum);
		rc = show(frame);
		if (rc == -EBUSY) {
			rc = 0; // No new samples retained for display retry.
		}
	}
	const int stop_rc = stop_capture();
	clear_audio(mono, sizeof(mono));
	analyzer.clear();
	clear_audio(&analysis, sizeof(analysis));
	(void)display_blanking_on(display);
	LOG_ERR("audio bench stopped (%d), capture cleanup (%d)", rc, stop_rc);
}
} // namespace badge
