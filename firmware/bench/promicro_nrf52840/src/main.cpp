/* SPDX-License-Identifier: MIT */
#include "badge/ble.hpp"
#include "badge/modes.hpp"
#include "badge/nfc.hpp"
#include "badge/settings_store.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

BUILD_ASSERT(IS_ENABLED(CONFIG_BADGE_USB_BENCH));
BUILD_ASSERT(!IS_ENABLED(CONFIG_APP_AUDIO) && !IS_ENABLED(CONFIG_APP_MOTION));
namespace
{
const device *const usb = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
badge::SettingsService settings, candidate;
badge::Modes modes;
badge::Frame frame;
int load_result;
uint32_t rendered;
void reply(const char *text)
{
	/* Direct USB output only. Codes are not RTT/log messages or saved records. */
	while (*text) {
		uart_poll_out(usb, *text++);
	}
}
int apply(void *, const badge::ConfigCommand &command)
{
	candidate = settings;
	if (!candidate.load(command.field, command.value(), command.length)) {
		return -EINVAL;
	}
	const badge::Mode previous = modes.active();
	if (!modes.configure(candidate.values()) ||
	    (command.field == badge::Field::kDefaultMode &&
	     !modes.select(candidate.values().default_mode, k_uptime_get_32()))) {
		(void)modes.configure(settings.values());
		(void)modes.select(previous, k_uptime_get_32());
		return -EINVAL;
	}
	modes.render(frame, k_uptime_get_32());
	const int rc = settings.save(command.field, command.value(), command.length,
				     badge::store_field, nullptr);
	if (rc != 0) {
		(void)modes.configure(settings.values());
		(void)modes.select(previous, k_uptime_get_32());
	}
	return rc;
}
void command(unsigned char key)
{
	if (key == 'p') {
		badge::open_ble_pairing();
		reply("Pairing window requested (60s). Connect Pixel Badge, then type k.\r\n");
	} else if (key == 'k') {
		uint32_t code = 0;
		char line[96];
		if (badge::ble_bench_passkey(code)) {
			snprintf(line, sizeof(line), "Pairing code: %06u\r\n", code);
			reply(line);
			memset(line, 0, sizeof(line));
			code = 0;
		} else {
			reply("No current pairing code. Type p and connect the app first.\r\n");
		}
	} else if (key == 's') {
		char line[160];
		const uint32_t checksum = frame.grayscale
						  ? crc32_ieee(frame.levels, sizeof(frame.levels))
						  : crc32_ieee(frame.bytes, sizeof(frame.bytes));
		snprintf(line, sizeof(line),
			 "BENCH uptime=%u load=%d frames=%u frame_crc=%08x mode=%u brightness=%u "
			 "NFC=%d\r\n",
			 k_uptime_get_32(), load_result, rendered, checksum,
			 static_cast<unsigned>(modes.active()), settings.values().brightness,
			 badge::nfc_last_result());
		reply(line);
	} else if (key == 'h') {
		reply("Pixel Badge headless bench: p=pair, k=show code, s=status, h=help.\r\n"
		      "No matrix, IMU, microphone or NFC tag. No recording.\r\n");
	}
}
} // namespace
int main()
{
	load_result = badge::load_settings(settings, false);
	modes.start(settings.values(), load_result != 0);
	badge::start_nfc(settings.values().url, load_result == 0);
	badge::start_ble();
	while (true) {
		badge::poll_ble(settings.values(), modes, load_result == 0, apply, nullptr);
		// Exercise the actual renderer and URL rollback state without physical IO.
		if (badge::poll_nfc(settings, modes)) {
			modes.render(frame, k_uptime_get_32());
			badge::nfc_display_result(0); // RAM rendering only; never a hardware pass.
		}
		modes.render(frame, k_uptime_get_32());
		++rendered;
		uint32_t dtr = 0;
		if (device_is_ready(usb) &&
		    uart_line_ctrl_get(usb, UART_LINE_CTRL_DTR, &dtr) == 0 && dtr) {
			unsigned char key;
			// Bound USB work so a host input flood cannot starve BLE/main ownership.
			for (unsigned i = 0; i < 16 && uart_poll_in(usb, &key) == 0; ++i) {
				command(key);
			}
		}
		k_msleep(20);
	}
}
