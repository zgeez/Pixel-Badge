/*
 * SPDX-License-Identifier: MIT
 *
 * Rev A charger and battery manager (CONFIG_APP_POWER_MANAGER), shared by the
 * product and diagnostics images: USB-C/BC1.2 input-current policy, charger
 * configuration, gated charging, battery sampling, matrix load shedding and
 * the power-switch reading. docs/power.md describes the policy.
 */

#pragma once

#include <stdint.h>

namespace badge
{

/* Configures the pins, SAADC and charger, then starts the manager thread.
 * verbose adds the bring-up logs. Returns the number of failed start-up
 * checks (charger identity or configuration, SAADC, VLED off). */
int power_manager_start(bool verbose);

/* The manager thread has run within the last second. */
bool power_manager_healthy();

/* Debounced power switch: 1 on, 0 off, -1 not read yet. Waits up to
 * timeout_ms for the first reading. */
int power_switch_state(uint32_t timeout_ms = 0);

/* Load shedding holds the LED rail off (ceiling 0). */
bool power_display_held();

/* Unloaded VBAT stayed below the lowest-power threshold on battery. */
bool power_low_battery();
/* Coarse battery estimate for the app (battery_level.hpp; BLE Battery
 * characteristic in docs/ble-protocol.md). level is a power::BatteryLevel;
 * mv is the near-rest reading behind it, 0 before the first one. */
constexpr uint8_t kBatteryUsb = 1;      /* USB power present */
constexpr uint8_t kBatteryCharging = 2; /* charger reports a charge phase */
constexpr uint8_t kBatteryShed = 4;     /* matrix shed or lowest-power state: level empty */
struct BatteryReport {
	uint8_t level;
	uint8_t flags;
	uint16_t mv;
};
BatteryReport power_battery_report();

/* Charging-only state: puts the IMU to sleep (retry until 0); the matrix,
 * microphone, NFC I2C side and Bluetooth are never started in that state. */
int power_enter_charging_only();

/* Lowest-power state: stops the manager, turns the LED rail, charging, VBAT
 * divider, NFC supply and IMU off and enters System OFF, woken by a charger
 * interrupt (VBUS change). Returns only if USB appeared meanwhile. */
void power_enter_off();

} // namespace badge
