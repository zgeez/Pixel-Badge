/*
 * SPDX-License-Identifier: MIT
 *
 * Rev A USB device. Its only purpose is the input-current policy: a host that
 * configures the badge (bMaxPower 500 mA) allows the 500 mA SDP limit, and a
 * suspended host requires suspend current, which the power manager meets with
 * the charger in HIZ. The CDC ACM interface uses the hosts' built-in drivers so
 * every host selects the configuration; nothing reads or writes it.
 */

#include "badge/usb_device.h"

#include <errno.h>
#include <hal/nrf_power.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
/* usbd_msg.h tests an enum with >= 0, which -Wextra flags as type-limits. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#include <zephyr/usb/usbd.h>
#pragma GCC diagnostic pop

LOG_MODULE_REGISTER(usb_device, CONFIG_APP_LOG_LEVEL);

/* 250 x 2 mA = 500 mA (USB 2.0 bMaxPower). */
#define BADGE_USB_MAX_POWER          250U
#define BADGE_USB_ENABLE_FALLBACK_MS 1000U

USBD_DEVICE_DEFINE(badge_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), CONFIG_APP_USB_VID,
		   CONFIG_APP_USB_PID);
USBD_DESC_LANG_DEFINE(badge_lang);
USBD_DESC_MANUFACTURER_DEFINE(badge_mfr, CONFIG_APP_USB_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(badge_product, CONFIG_APP_USB_PRODUCT);
IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(badge_sn)));
USBD_DESC_CONFIG_DEFINE(badge_cfg_desc, "Smart LED Badge");
USBD_CONFIGURATION_DEFINE(badge_config, 0, BADGE_USB_MAX_POWER, &badge_cfg_desc);

static struct k_sem *event_sem;
static atomic_t vbus, enabled, host_seen, configured, suspended;
static atomic_t vbus_since;

static void notify(void)
{
	if (event_sem != NULL) {
		k_sem_give(event_sem);
	}
}

static void msg_cb(struct usbd_context *const ctx, const struct usbd_msg *msg)
{
	switch (msg->type) {
	case USBD_MSG_VBUS_READY:
		atomic_set(&vbus_since, (atomic_val_t)k_uptime_get_32());
		atomic_set(&vbus, 1);
		break;
	case USBD_MSG_VBUS_REMOVED:
		atomic_clear(&vbus);
		atomic_clear(&host_seen);
		atomic_clear(&configured);
		atomic_clear(&suspended);
		if (atomic_cas(&enabled, 1, 0) && usbd_disable(ctx) != 0) {
			LOG_WRN("USB disable failed");
		}
		break;
	case USBD_MSG_RESET:
		/* Only a host resets the bus: a dedicated charger's idle lines are
		 * never treated as a suspend. */
		atomic_set(&host_seen, 1);
		atomic_clear(&configured);
		atomic_clear(&suspended);
		break;
	case USBD_MSG_CONFIGURATION:
		atomic_set(&configured, msg->status != 0);
		LOG_INF("USB configuration %d", msg->status);
		break;
	case USBD_MSG_SUSPEND:
		if (atomic_get(&host_seen) != 0) {
			atomic_set(&suspended, 1);
		}
		break;
	case USBD_MSG_RESUME:
		atomic_clear(&suspended);
		break;
	default:
		return;
	}
	notify();
}

int badge_usb_start(struct k_sem *event)
{
	struct usbd_context *ctx = &badge_usbd;
	int err;

	event_sem = event;
	if ((err = usbd_add_descriptor(ctx, &badge_lang)) != 0 ||
	    (err = usbd_add_descriptor(ctx, &badge_mfr)) != 0 ||
	    (err = usbd_add_descriptor(ctx, &badge_product)) != 0) {
		return err;
	}
#if defined(CONFIG_HWINFO)
	if ((err = usbd_add_descriptor(ctx, &badge_sn)) != 0) {
		return err;
	}
#endif
	if ((err = usbd_add_configuration(ctx, USBD_SPEED_FS, &badge_config)) != 0 ||
	    (err = usbd_register_all_classes(ctx, USBD_SPEED_FS, 1, NULL)) != 0) {
		return err;
	}
	/* CDC ACM carries an Interface Association Descriptor. */
	(void)usbd_device_set_code_triple(ctx, USBD_SPEED_FS, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
	usbd_self_powered(ctx, false);
	if ((err = usbd_msg_register_cb(ctx, msg_cb)) != 0 || (err = usbd_init(ctx)) != 0) {
		return err;
	}
	/* VBUS may already be present at boot (USB attach powers the badge). */
	if (!usbd_can_detect_vbus(ctx) || nrf_power_usbregstatus_vbusdet_get(NRF_POWER)) {
		atomic_set(&vbus_since, (atomic_val_t)k_uptime_get_32());
		atomic_set(&vbus, 1);
	}
	return 0;
}

void badge_usb_poll(bool detection_done)
{
	if (atomic_get(&vbus) == 0 || atomic_get(&enabled) != 0) {
		return;
	}
	const uint32_t since = k_uptime_get_32() - (uint32_t)atomic_get(&vbus_since);

	if (!detection_done && since < BADGE_USB_ENABLE_FALLBACK_MS) {
		return;
	}
	int err = usbd_enable(&badge_usbd);

	if (err == 0 || err == -EALREADY) {
		atomic_set(&enabled, 1);
	} else if (since < BADGE_USB_ENABLE_FALLBACK_MS + 100U) {
		LOG_WRN("USB enable failed (%d); retrying", err);
	}
}

enum badge_usb_host badge_usb_host_state(void)
{
	if (atomic_get(&suspended) != 0) {
		return BADGE_USB_SUSPENDED;
	}
	return atomic_get(&configured) != 0 ? BADGE_USB_CONFIGURED : BADGE_USB_NOT_CONFIGURED;
}
