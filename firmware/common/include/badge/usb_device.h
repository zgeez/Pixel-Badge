/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/* What the USB host has done with the badge (CONFIG_APP_USB_DEVICE). */
enum badge_usb_host {
	BADGE_USB_NOT_CONFIGURED, /* no host, or not configured: 100 mA */
	BADGE_USB_CONFIGURED,     /* bMaxPower 500 mA configuration selected */
	BADGE_USB_SUSPENDED,      /* a host suspended the bus: suspend current */
};

/* Registers the device and its inert CDC ACM interface. Each USB state change
 * gives event, so the power manager can react within the USB 2.0 suspend
 * deadline. */
int badge_usb_start(struct k_sem *event);

/* Connects the D+ pull-up once VBUS is present and the charger's BC1.2
 * detection has finished (or 1 s after VBUS without a charger reading), so
 * the pull-up does not disturb that detection. Called every 20 ms. */
void badge_usb_poll(bool detection_done);

enum badge_usb_host badge_usb_host_state(void);

/* usbd_enable() has kept failing for 5 s with VBUS present: a host suspend
 * would go unseen, so the power manager fails closed. */
bool badge_usb_enable_failing(void);

#ifdef __cplusplus
}
#endif
