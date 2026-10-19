/* SPDX-License-Identifier: MIT */
#include "badge/nfc_platform.hpp"
#include "bus_lock.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <errno.h>

LOG_MODULE_REGISTER(nfc_transport, CONFIG_APP_LOG_LEVEL);

namespace badge
{
int bench_bus_lock()
{
	return badge_bus_lock();
}
void bench_bus_unlock()
{
	badge_bus_unlock();
}

#define NFC_USER DT_PATH(zephyr_user)
#if defined(CONFIG_I2C) && DT_NODE_HAS_PROP(NFC_USER, ntag_i2c_bus) &&                             \
	DT_NODE_HAS_PROP(NFC_USER, nfc_fd_gpios)
namespace
{
const device *const bus = DEVICE_DT_GET(DT_PHANDLE(NFC_USER, ntag_i2c_bus));
const gpio_dt_spec fd = GPIO_DT_SPEC_GET(NFC_USER, nfc_fd_gpios);
gpio_callback callback;
atomic_t edges;
bool prepared;
#if DT_NODE_HAS_PROP(NFC_USER, nfc_pwr_gpios)
const gpio_dt_spec power = GPIO_DT_SPEC_GET(NFC_USER, nfc_pwr_gpios);
bool powered;
#endif

void on_field(const device *, gpio_callback *, uint32_t)
{
	atomic_inc(&edges);
}
int lock_bus(void *, bool acquire)
{
	if (acquire) {
		return bench_bus_lock();
	}
	bench_bus_unlock();
	return 0;
}
void clear_edges(void *)
{
	atomic_clear(&edges);
}
int field(void *)
{
	return gpio_pin_get_dt(&fd);
}
int interrupt(void *, bool enable)
{
	return gpio_pin_interrupt_configure_dt(&fd, enable ? GPIO_INT_EDGE_BOTH : GPIO_INT_DISABLE);
}
int arm(void *context, bool enable)
{
	return enable ? arm_ntag_field({context, clear_edges, field, interrupt})
		      : interrupt(context, false);
}
int edge_count(void *)
{
	return atomic_get(&edges);
}
int write(void *, uint8_t address, const uint8_t *data, size_t size)
{
	return i2c_write(bus, data, size, address);
}
int read(void *, uint8_t address, uint8_t *data, size_t size)
{
	return i2c_read(bus, data, size, address);
}
void wait(void *, uint32_t duration)
{
	k_busy_wait(duration);
}
uint64_t time(void *)
{
	return k_cyc_to_us_floor64(k_cycle_get_64());
}
} // namespace

int prepare_nfc_transport()
{
	int rc = bench_bus_lock();
	if (rc != 0) {
		return rc;
	}
	if (!prepared) {
#if DT_NODE_HAS_PROP(NFC_USER, nfc_pwr_gpios)
		if (!powered) {
			rc = gpio_is_ready_dt(&power)
				     ? gpio_pin_configure_dt(&power, GPIO_OUTPUT_ACTIVE)
				     : -ENODEV;
			if (rc != 0) {
				bench_bus_unlock();
				return rc;
			}
			/* Conservative initial bring-up guard, not a measured power-on time.
			 * Keep VCC on while the runtime owns FD and the session registers. */
			k_msleep(10);
			powered = true;
		}
#endif
		if (!device_is_ready(bus) || !gpio_is_ready_dt(&fd)) {
			rc = -ENODEV;
		} else {
			rc = gpio_pin_configure_dt(&fd, GPIO_INPUT | GPIO_PULL_UP);
			if (rc == 0) {
				gpio_init_callback(&callback, on_field, BIT(fd.pin));
				rc = gpio_add_callback(fd.port, &callback);
			}
			prepared = rc == 0;
		}
	}
	bench_bus_unlock();
	return rc;
}

NtagIo nfc_transport()
{
#ifdef CONFIG_APP_NFC_FD_LATENCY_US
	constexpr uint32_t margin = CONFIG_APP_NFC_FD_LATENCY_US;
#else
	constexpr uint32_t margin = 500;
#endif
	static_assert(margin <= kNtagMaxFdLatencyUs, "FD margin exceeds write polling budget");
	return {nullptr, lock_bus, arm, field, edge_count, write, read, wait, time, margin};
}

int nfc_bringup_check()
{
#if defined(CONFIG_APP_NFC)
	/* The NFC worker owns runtime transactions and the FD latch. */
	LOG_INF("NFC tag: runtime service owns the check; see NFC status");
	return 0;
#else
	int rc = prepare_nfc_transport();
	if (rc == 0) {
		Ntag tag(nfc_transport());
		rc = tag.initialize();
		uint8_t metadata[16];
		if (rc == 0) {
			rc = tag.read_block(0, metadata);
		}
		if (rc == 0 && metadata[0] != 4) {
			rc = -EIO;
		}
		(void)tag.write_register(6, 0x40, 0);
	}
	LOG_INF("NFC tag: guarded read result %d", rc);
	return rc == 0 ? 0 : 1;
#endif
}

void nfc_log_field()
{
	if (prepared) {
		LOG_INF("NFC tag: field detect %d", gpio_pin_get_dt(&fd));
	}
}
#else
int prepare_nfc_transport()
{
	return -ENODEV;
}
NtagIo nfc_transport()
{
	return {};
}
int nfc_bringup_check()
{
	return 0;
}
void nfc_log_field()
{
}
#endif
} // namespace badge
