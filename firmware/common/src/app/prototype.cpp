/* SPDX-License-Identifier: MIT */
#include "badge/prototype.hpp"
#include "badge/boot_guard.hpp"
#include "badge/boot_retention.hpp"
#include "badge/buttons.hpp"
#include "badge/display_health.hpp"
#include "badge/modes.hpp"
#include "badge/settings_store.hpp"
#include "badge/nfc.hpp"
#if defined(CONFIG_APP_MOTION)
#include "badge/motion_runtime.hpp"
#endif
#if defined(CONFIG_APP_AUDIO)
#include "badge/audio_runtime.hpp"
#endif
#if defined(CONFIG_BADGE_DISPLAY_SYNC)
#include "badge/display_sync.h"
#endif
#if defined(CONFIG_APP_BLE)
#include "badge/ble.hpp"
#endif

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <errno.h>

LOG_MODULE_REGISTER(prototype, CONFIG_APP_LOG_LEVEL);

namespace badge
{
namespace
{
const gpio_dt_spec buttons[] = {GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
				GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios)};
// Single main-loop owner. Bounded settings/particles live in BSS, not on its stack.
Modes modes;
SettingsService prototype_settings, prototype_candidate;

int present(const device *display, const Frame &frame)
{
	display_buffer_descriptor descriptor{};
	const int format_rc = display_set_pixel_format(
		display, frame.grayscale ? PIXEL_FORMAT_L_8 : PIXEL_FORMAT_MONO01);
	if (format_rc != 0) {
		return format_rc;
	}
	descriptor.buf_size = frame.grayscale ? sizeof(frame.levels) : sizeof(frame.bytes);
	descriptor.width = descriptor.pitch = kMatrixSize;
	descriptor.height = kMatrixSize;
	return display_write(display, 0, 0, &descriptor,
			     frame.grayscale ? frame.levels : frame.bytes);
}

int initialize_display(const device *display, const Frame &frame, uint8_t brightness)
{
	if (!device_is_ready(display)) {
		return -ENODEV;
	}
	int rc = display_blanking_on(display);
	if (rc == 0) {
		rc = display_set_pixel_format(display, PIXEL_FORMAT_MONO01);
	}
	if (rc == 0) {
		rc = display_set_brightness(display, brightness);
	}
	if (rc == 0) {
		rc = present(display, frame);
	}
	return rc == 0 ? display_blanking_off(display) : rc;
}
#if defined(CONFIG_APP_BLE)
struct ConfigContext {
	SettingsService &settings;
	Modes &modes;
	const device *display;
	uint32_t now;
};
int apply_config(void *pointer, const ConfigCommand &command)
{
	if (nfc_update_busy()
#if defined(CONFIG_APP_AUDIO)
	    || audio_ui_busy()
#endif
	) {
		return -EBUSY;
	}
	auto &context = *static_cast<ConfigContext *>(pointer);
	auto &candidate = prototype_candidate;
	candidate = context.settings;
	if (!candidate.load(command.field, command.value(), command.length)) {
		return -EINVAL;
	}
	const Mode previous_mode = context.modes.active();
#if defined(CONFIG_APP_AUDIO)
	if (command.field == Field::kDefaultMode) {
		/* Finish DMA cleanup before a configuration update can show a new mode. */
		audio_halt();
	}
#endif
	if (!context.modes.configure(candidate.values())) {
		return -EINVAL;
	}
	if (command.field == Field::kDefaultMode &&
	    !context.modes.select(candidate.values().default_mode, context.now)) {
		(void)context.modes.configure(context.settings.values());
		return -EINVAL;
	}
	Frame frame;
	context.modes.render(frame, context.now);
	int rc = display_set_brightness(context.display, candidate.values().brightness);
	if (rc == 0) {
		rc = present(context.display, frame);
	}
	if (rc == 0) {
		rc = context.settings.save(command.field, command.value(), command.length,
					   store_field, nullptr);
	}
	if (rc != 0) {
		context.modes.configure(context.settings.values());
		if (context.modes.active() != previous_mode) {
			(void)context.modes.select(previous_mode, context.now);
		}
		context.modes.render(frame, context.now);
		(void)display_set_brightness(context.display, context.settings.values().brightness);
		(void)present(context.display, frame);
	}
	return rc;
}
#endif
} // namespace

void run_prototype()
{
	uint32_t cause = 0;
	const int cause_rc = hwinfo_get_reset_cause(&cause);
	LOG_INF("reset cause 0x%08x (read %d)", cause, cause_rc);
	BootGuard guard;
	retain_boot_count(guard.start(retained_boot_count(), (cause & RESET_WATCHDOG) != 0));
	hwinfo_clear_reset_cause();
	bool safe = guard.safe_mode();
	if (safe) {
		LOG_WRN("watchdog reset loop: safe mode, default QR");
	}

	const device *watchdog = DEVICE_DT_GET(DT_NODELABEL(wdt0));
	wdt_timeout_cfg timeout{};
	timeout.window.max = 8000;
	timeout.flags = WDT_FLAG_RESET_SOC;
	int channel = device_is_ready(watchdog) ? wdt_install_timeout(watchdog, &timeout) : -ENODEV;
	if (channel < 0 || wdt_setup(watchdog, 0) != 0) {
		LOG_ERR("watchdog unavailable (%d); safe mode", channel);
		safe = true;
		channel = -1;
	}

	bool buttons_ready = true;
	for (const auto &button : buttons) {
		if (!gpio_is_ready_dt(&button) || gpio_pin_configure_dt(&button, GPIO_INPUT) != 0) {
			buttons_ready = false;
		}
	}
	if (!buttons_ready) {
		LOG_ERR("buttons unavailable; factory reset and UI disabled");
	}
	/* Require a continuously held Button 2 over a one-second confirmation
	 * window. No boot UI event escapes the classifier's initial suppression. */
	bool factory_reset = buttons_ready && gpio_pin_get_dt(&buttons[1]) == 1;
	if (factory_reset) {
		for (int i = 0; i < 50; ++i) {
			k_sleep(K_MSEC(20));
			if (gpio_pin_get_dt(&buttons[1]) != 1) {
				factory_reset = false;
				break;
			}
			if (channel >= 0) {
				wdt_feed(watchdog, channel);
			}
		}
	}
	auto &settings = prototype_settings;
	const int load_rc = load_settings(settings, factory_reset);
	if (load_rc != 0) {
		LOG_ERR("settings load/reset failed (%d); safe mode", load_rc);
		safe = true;
	}
	if (factory_reset) {
		retain_boot_count(0xa0);
	}
	modes.start(settings.values(), safe);
#if defined(CONFIG_APP_MOTION)
	if (!safe) {
		start_motion();
		poll_motion(modes, 0);
		(void)modes.select(modes.active(), 0);
	}
#endif
	const device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	Frame initial;
	modes.render(initial, 0);
	const int display_rc = initialize_display(display, initial,
						  safe ? kDefaultSettings.brightness
						       : settings.values().brightness);
	bool display_ready = display_rc == 0;
	const bool display_disarmed = IS_ENABLED(CONFIG_REVA_MATRIX) &&
				      !IS_ENABLED(CONFIG_REVA_MATRIX_OUTPUT) &&
				      display_rc == -EPERM;
	DisplayHealth display_health;
	display_health.report(display_rc, 0, display_disarmed);
	if (display_disarmed) {
		LOG_INF("matrix intentionally disarmed; service loop remains active");
	} else if (!display_ready) {
		LOG_WRN("display initialization failed (%d); retrying", display_rc);
	}
	Button classifier[2];
	for (unsigned i = 0; i < 2; ++i) {
		classifier[i].reset(buttons_ready && gpio_pin_get_dt(&buttons[i]) == 1, 0);
	}
	const int64_t started_at = k_uptime_get();
	uint32_t last_render = 0;
	LOG_INF("badge ready: %s mode%s", modes.active() == Mode::kQr ? "QR" : "name",
		safe ? " (safe)" : "");
#if defined(CONFIG_APP_NFC)
	start_nfc(safe ? kDefaultSettings.url : settings.values().url, !safe && load_rc == 0);
#endif
#if defined(CONFIG_APP_BLE)
	ConfigContext config_context{settings, modes, display, 0};
	start_ble();
#endif
	while (true) {
		const uint32_t now = static_cast<uint32_t>(k_uptime_get() - started_at);
		if (IS_ENABLED(CONFIG_APP_WATCHDOG_TEST_LOOP) && !safe) {
			/* Explicit bench-only fault: do not feed, even during sleep. */
			k_sleep(K_FOREVER);
		}
		bool changed = false;
#if defined(CONFIG_APP_MOTION)
		if (!safe) {
			poll_motion(modes, now);
		}
#endif
#if defined(CONFIG_APP_NFC)
		changed = poll_nfc(settings, modes);
#endif
#if defined(CONFIG_APP_BLE)
		config_context.now = now;
		poll_ble(settings.values(), modes,
			 !safe && load_rc == 0 && display_ready && !display_health.recovering() &&
				 !nfc_update_busy()
#if defined(CONFIG_APP_AUDIO)
				 && !audio_ui_busy()
#endif
				 ,
			 apply_config, &config_context);
#endif
		for (unsigned i = 0; buttons_ready && i < 2; ++i) {
			const int level = gpio_pin_get_dt(&buttons[i]);
			if (level < 0) {
				continue;
			}
			const Press event = classifier[i].sample(level == 1, now);
#if defined(CONFIG_APP_AUDIO)
			if (event != Press::kNone &&
			    (audio_recording() || (i == 1 && audio_ui_busy()))) {
				(void)audio_button(event == Press::kLong);
				continue;
			}
			if (i == 0 && event == Press::kLong && audio_button(true)) {
				continue;
			}
			if (audio_ui_busy()) {
				continue;
			}
#endif
			if (event == Press::kLong) {
#if defined(CONFIG_APP_BLE)
				if (i == 1 && !safe && display_ready) {
					open_ble_pairing();
				}
#else
				LOG_INF("button %u long press reserved", i + 1);
#endif
			} else if (event == Press::kShort && !safe
#if defined(CONFIG_APP_BLE)
				   && !ble_config_busy() && !nfc_update_busy()
#endif
			) {
				if (i == 0) {
					modes.next(now);
					LOG_INF("mode %s",
						modes.active() == Mode::kQr ? "QR" : "name");
					changed = true;
				} else if (load_rc == 0 && display_ready) {
					const uint8_t previous = settings.values().brightness;
					const uint8_t brightness = next_brightness(previous);
					int rc = display_set_brightness(display, brightness);
					if (rc == 0) {
						rc = settings.save(Field::kBrightness, &brightness,
								   1, store_field, nullptr);
						if (rc != 0) {
							display_set_brightness(display, previous);
						}
					}
					LOG_INF("brightness %u, result %d",
						settings.values().brightness, rc);
				}
			}
		}
#if defined(CONFIG_APP_AUDIO)
		audio_tick(!safe && load_rc == 0 && display_ready && !display_health.recovering() &&
				   !nfc_update_busy()
#if defined(CONFIG_APP_BLE)
				   && !ble_artwork_busy()
#endif
				   ,
			   modes.active() == Mode::kAudioLevel ||
				   modes.active() == Mode::kAudioSpectrum,
			   modes.active() == Mode::kAudioSpectrum);
#endif
		/* Sample fast content at the main-loop cadence. Recovery retains its
		 * bounded retry rate; animation timing is elapsed-time based. */
		const bool fast_content =
#if defined(CONFIG_APP_AUDIO)
			audio_capture_active() || audio_ui_busy() ||
#endif
			modes.active() == Mode::kAnimation || modes.active() == Mode::kFluid ||
			(modes.active() == Mode::kName && text_animated(settings.values().text));
		const uint32_t render_interval =
			fast_content && !display_health.recovering() ? 20 : DisplayHealth::kRetryMs;
		if ((changed && !display_health.recovering()) ||
		    now - last_render >= render_interval) {
			Frame frame;
			int brightness_rc = 0;
			modes.render(frame, now);
			[[maybe_unused]] bool pairing_overlay = false;
#if defined(CONFIG_APP_BLE)
			pairing_overlay = ble_pairing_frame(frame);
#if defined(CONFIG_APP_MOTION)
			bool rotate_overlay = pairing_overlay;
#endif
#if defined(CONFIG_APP_AUDIO)
			bool recording_indicator = false;
			if (!pairing_overlay) {
				if (modes.active() == Mode::kAudioLevel ||
				    modes.active() == Mode::kAudioSpectrum) {
					audio_visual_frame(frame,
							   modes.active() == Mode::kAudioSpectrum);
#if defined(CONFIG_APP_MOTION)
					rotate_overlay = true;
#endif
				}
				const AudioOverlay overlay = audio_overlay(frame);
				recording_indicator = overlay == AudioOverlay::kRecording;
				pairing_overlay = overlay != AudioOverlay::kNone;
#if defined(CONFIG_APP_MOTION)
				rotate_overlay = rotate_overlay || overlay != AudioOverlay::kNone;
#endif
			}
			if (!display_disarmed) {
				brightness_rc = display_set_brightness(
					display,
					audio_recording() ? 255 : settings.values().brightness);
			}
#endif
#endif
#if defined(CONFIG_APP_MOTION)
			if (rotate_overlay) {
				rotate(frame, frame, modes.display_rotation());
			}
#endif
			/* Disarmed is a stable configuration, not a retryable driver fault.
			 * Keep the physical error for visibility gates without repeating IO. */
			int rc = display_disarmed ? -EPERM : brightness_rc;
			if (rc == 0) {
				auto submit = [&](bool initialize) {
					if (initialize) {
						const uint8_t level =
							safe ? kDefaultSettings.brightness
							     : settings.values().brightness;
						return initialize_display(display, frame, level);
					}
					return present(display, frame);
				};
				auto confirm_visible = [&]() {
#if defined(CONFIG_BADGE_DISPLAY_SYNC)
					return badge_display_wait_visible(display, 50);
#else
					return 0;
#endif
				};
				rc = display_health.present(display_ready, submit, confirm_visible);
			}
#if defined(CONFIG_APP_AUDIO)
			/* Recovery already confirmed the submitted frame above. */
			if (rc == 0 && audio_ui_busy() && !display_health.recovering()) {
				rc = badge_display_wait_visible(display, 50);
			}
			audio_display_result(audio_recording() && !recording_indicator ? -EAGAIN
										       : rc);
#endif
			const uint32_t completed =
				static_cast<uint32_t>(k_uptime_get() - started_at);
#if defined(CONFIG_APP_NFC)
			nfc_display_result(
				!pairing_overlay && modes.active() == Mode::kQr ? rc : -EAGAIN);
#endif
			const bool recovering = display_health.recovering();
			display_health.report(rc, completed, display_disarmed);
			if (rc != 0) {
				if (!display_disarmed && !recovering) {
					LOG_WRN("display write failed (%d); retrying", rc);
				}
			} else {
				display_ready = true;
				if (recovering && !display_health.recovering()) {
					LOG_INF("display recovered");
				}
			}
			last_render = completed;
		}
		const uint32_t completed = static_cast<uint32_t>(k_uptime_get() - started_at);
		if (display_health.restart_required(completed)) {
			LOG_ERR("persistent display failure; restarting");
#if defined(CONFIG_APP_AUDIO)
			audio_halt();
#endif
			if (device_is_ready(display)) {
				display_blanking_on(display);
			}
			if (channel < 0) {
				sys_reboot(SYS_REBOOT_COLD);
			}
			/* Let the configured watchdog reset the SoC and preserve its cause. */
			k_sleep(K_FOREVER);
		}
		if (channel >= 0) {
			wdt_feed(watchdog, channel);
		}
		if (display_ready && guard.healthy(display_health.healthy_ms(completed))) {
			retain_boot_count(0xa0);
			LOG_INF("healthy boot; reset-loop counter cleared");
		}
#if defined(CONFIG_APP_AUDIO)
		if (!audio_capture_active())
#endif
			k_sleep(K_MSEC(20));
	}
}
} // namespace badge
