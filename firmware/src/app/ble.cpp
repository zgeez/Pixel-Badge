/* SPDX-License-Identifier: MIT */
#include "badge/ble.hpp"
#include "badge/nfc.hpp"
#include "badge/text.hpp"
#include "badge/modes.hpp"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(badge_ble, CONFIG_APP_LOG_LEVEL);
namespace badge
{
namespace
{
#define BADGE_UUID(n) BT_UUID_128_ENCODE(0xf3b00000 + (n), 0x6e9b, 0x4f4c, 0xa8f8, 0x731a9d46e1c0)
bt_uuid_128 service_uuid = BT_UUID_INIT_128(BADGE_UUID(1));
bt_uuid_128 info_uuid = BT_UUID_INIT_128(BADGE_UUID(2));
bt_uuid_128 name_uuid = BT_UUID_INIT_128(BADGE_UUID(3));
bt_uuid_128 url_uuid = BT_UUID_INIT_128(BADGE_UUID(4));
bt_uuid_128 brightness_uuid = BT_UUID_INIT_128(BADGE_UUID(5));
bt_uuid_128 mode_uuid = BT_UUID_INIT_128(BADGE_UUID(6));
bt_uuid_128 apply_uuid = BT_UUID_INIT_128(BADGE_UUID(7));
bt_uuid_128 status_uuid = BT_UUID_INIT_128(BADGE_UUID(8));
bt_uuid_128 text_uuid = BT_UUID_INIT_128(BADGE_UUID(9));
bt_uuid_128 artwork_uuid = BT_UUID_INIT_128(BADGE_UUID(10));
bt_uuid_128 nfc_uuid = BT_UUID_INIT_128(BADGE_UUID(11));
K_MUTEX_DEFINE(ble_mutex);
K_SEM_DEFINE(ble_started, 0, 1);
K_SEM_DEFINE(main_requested, 0, 1);
K_SEM_DEFINE(main_completed, 0, 1);
K_MSGQ_DEFINE(config_requests, sizeof(ConfigCommand), 1, 4);
BleProtocol protocol;
Settings snapshot = kDefaultSettings;
Mode active_mode = Mode::kQr;
bt_conn *connection;
bool writable, enabled, job_active;
uint64_t pairing_deadline;
bool passkey_visible;
uint32_t displayed_passkey;
ConfigCommand main_command;
int main_result;
uint16_t artwork_read_offset;

uint8_t flags()
{
	return (writable && enabled ? 1 : 0) | (nfc_ready() ? 2 : 0) |
	       (pairing_deadline > static_cast<uint64_t>(k_uptime_get()) ? 4 : 0);
}
bool secure(bt_conn *conn)
{
	return bt_conn_get_security(conn) == BT_SECURITY_L4;
}
ssize_t att_error(int rc)
{
	return BT_GATT_ERR(rc == -EACCES  ? BT_ATT_ERR_AUTHENTICATION
			   : rc == -EBUSY ? BT_ATT_ERR_UNLIKELY
					  : BT_ATT_ERR_VALUE_NOT_ALLOWED);
}
ssize_t read_info(bt_conn *conn, const bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
	k_mutex_lock(&ble_mutex, K_FOREVER);
	const uint8_t info[] = {kBleVersion, kPayloadMax, kNameMax,       4,
				4,           flags(),     kArtworkFrames, 7};
	k_mutex_unlock(&ble_mutex);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, info, sizeof(info));
}
ssize_t read_setting(bt_conn *conn, const bt_gatt_attr *attr, void *buf, uint16_t len,
		     uint16_t offset)
{
	if (!secure(conn)) {
		return att_error(-EACCES);
	}
	uint8_t data[kTextBytes];
	size_t length = 1;
	const Field field = static_cast<Field>(reinterpret_cast<uintptr_t>(attr->user_data));
	k_mutex_lock(&ble_mutex, K_FOREVER);
	if (field == Field::kUrl) {
		length = strlen(snapshot.url);
		memcpy(data, snapshot.url, length);
	} else if (field == Field::kName) {
		length = strlen(snapshot.name);
		memcpy(data, snapshot.name, length);
	} else if (field == Field::kText) {
		length = snapshot.text.length;
		memcpy(data, snapshot.text.bytes, length);
	} else if (field == Field::kArtwork) {
		const uint16_t total = snapshot.artwork.length;
		const uint16_t cursor = artwork_read_offset <= total ? artwork_read_offset : total;
		length = total - cursor < 16 ? total - cursor : 16;
		data[0] = cursor;
		data[1] = cursor >> 8;
		data[2] = total;
		data[3] = total >> 8;
		memcpy(data + 4, snapshot.artwork.bytes + cursor, length);
		length += 4;
	} else if (field == kNfcSyncField) {
		const int result = nfc_last_result();
		data[0] = 1;
		data[1] = nfc_ready();
		data[2] = nfc_update_busy();
		for (unsigned i = 0; i < 4; ++i) {
			data[3 + i] = static_cast<uint32_t>(result) >> (i * 8);
		}
		length = 7;
	} else {
		data[0] = field == Field::kBrightness ? snapshot.brightness
						      : static_cast<uint8_t>(active_mode);
	}
	k_mutex_unlock(&ble_mutex);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, data, length);
}
ssize_t stage_setting(bt_conn *conn, const bt_gatt_attr *attr, const void *buf, uint16_t len,
		      uint16_t offset, uint8_t write_flags)
{
	if (offset != 0 || write_flags != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	const Field field = static_cast<Field>(reinterpret_cast<uintptr_t>(attr->user_data));
	k_mutex_lock(&ble_mutex, K_FOREVER);
	if (field == Field::kArtwork && len == 3 && static_cast<const uint8_t *>(buf)[0] == 0) {
		const int rc = secure(conn) && connection == conn ? 0 : -EACCES;
		const uint16_t cursor = content_interval(static_cast<const uint8_t *>(buf) + 1);
		const bool within = cursor <= snapshot.artwork.length;
		if (rc == 0 && within) {
			artwork_read_offset = cursor;
		}
		k_mutex_unlock(&ble_mutex);
		return rc != 0 ? att_error(rc) : !within ? att_error(-EINVAL) : len;
	}
	const int rc = job_active ? -EBUSY
				  : protocol.stage(field, static_cast<const uint8_t *>(buf), len,
						   secure(conn) && connection == conn && writable &&
							   !passkey_visible);
	k_mutex_unlock(&ble_mutex);
	return rc == 0 ? len : att_error(rc);
}
ssize_t read_status(bt_conn *conn, const bt_gatt_attr *attr, void *buf, uint16_t len,
		    uint16_t offset)
{
	if (!secure(conn)) {
		return att_error(-EACCES);
	}
	uint8_t data[kBleStatusSize];
	k_mutex_lock(&ble_mutex, K_FOREVER);
	protocol.status(data, flags());
	k_mutex_unlock(&ble_mutex);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, data, sizeof(data));
}
void notify_status(k_work *);
k_work status_work{};
ssize_t apply_setting(bt_conn *conn, const bt_gatt_attr *, const void *buf, uint16_t len,
		      uint16_t offset, uint8_t write_flags)
{
	if (offset != 0 || write_flags != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	/* Large bounded asset storage must not live on the Bluetooth RX stack. */
	static ConfigCommand command;
	k_mutex_lock(&ble_mutex, K_FOREVER);
	int rc = job_active ? -EBUSY
			    : protocol.accept(static_cast<const uint8_t *>(buf), len,
					      secure(conn) && connection == conn && writable &&
						      !passkey_visible,
					      command);
	if (rc == 0) {
		rc = k_msgq_put(&config_requests, &command, K_NO_WAIT);
		if (rc == 0) {
			job_active = true;
		} else {
			protocol.complete(command, rc);
		}
	}
	k_mutex_unlock(&ble_mutex);
	if (rc == 0) {
		k_work_submit(&status_work);
	}
	return rc == 0 ? len : att_error(rc);
}
void ccc_changed(const bt_gatt_attr *, uint16_t)
{
}
bt_gatt_ccc_managed_user_data status_ccc{};
constexpr uint16_t read_permission = BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_READ_LESC;
constexpr uint16_t write_permission = BT_GATT_PERM_WRITE_AUTHEN | BT_GATT_PERM_WRITE_LESC;
#define SETTING_CHARACTERISTIC(uuid_value, field)                                                  \
	BT_GATT_CHARACTERISTIC(&(uuid_value).uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,         \
			       read_permission | write_permission, read_setting, stage_setting,    \
			       reinterpret_cast<void *>(static_cast<uintptr_t>(field)))
/* Zephyr's C attribute macros omit internal zero-initialized handle fields.
 * Keep the exception local to their declaration; application code stays strict. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
BT_GATT_SERVICE_DEFINE(badge_service, BT_GATT_PRIMARY_SERVICE(&service_uuid),
		       BT_GATT_CHARACTERISTIC(&info_uuid.uuid, BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
					      read_info, nullptr, nullptr),
		       SETTING_CHARACTERISTIC(name_uuid, Field::kName),
		       SETTING_CHARACTERISTIC(url_uuid, Field::kUrl),
		       SETTING_CHARACTERISTIC(brightness_uuid, Field::kBrightness),
		       SETTING_CHARACTERISTIC(mode_uuid, Field::kDefaultMode),
		       BT_GATT_CHARACTERISTIC(&apply_uuid.uuid, BT_GATT_CHRC_WRITE,
					      write_permission, nullptr, apply_setting, nullptr),
		       BT_GATT_CHARACTERISTIC(&status_uuid.uuid,
					      BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
					      read_permission, read_status, nullptr, nullptr),
		       BT_GATT_CCC_MANAGED(&status_ccc, read_permission | write_permission),
		       /* Append extensions to preserve the original v1 attribute order. */
		       SETTING_CHARACTERISTIC(text_uuid, Field::kText),
		       SETTING_CHARACTERISTIC(artwork_uuid, Field::kArtwork),
		       SETTING_CHARACTERISTIC(nfc_uuid, kNfcSyncField));
#pragma GCC diagnostic pop

void notify_status(k_work *)
{
	uint8_t data[kBleStatusSize];
	k_mutex_lock(&ble_mutex, K_FOREVER);
	bt_conn *conn = connection != nullptr ? bt_conn_ref(connection) : nullptr;
	protocol.status(data, flags());
	k_mutex_unlock(&ble_mutex);
	if (conn != nullptr) {
		if (secure(conn)) {
			(void)bt_gatt_notify_uuid(conn, &status_uuid.uuid, badge_service.attrs,
						  data, sizeof(data));
		}
		bt_conn_unref(conn);
	}
}
void advertise(k_work *)
{
	/* BT_DATA_BYTES creates C compound literals. In C++ their temporary
	 * buffers expire at the declaration's semicolon. Keep every payload
	 * alive through bt_le_adv_start(), using the service's static UUID. */
	static const uint8_t advertising_flags[] = {BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR};
	const bt_data ad[] = {
		BT_DATA(BT_DATA_FLAGS, advertising_flags, sizeof(advertising_flags)),
		BT_DATA(BT_DATA_UUID128_ALL, service_uuid.val, sizeof(service_uuid.val))};
	const bt_data sd[] = {BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
				      sizeof(CONFIG_BT_DEVICE_NAME) - 1)};
	int rc = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (rc != 0 && rc != -EALREADY) {
		LOG_WRN("BLE advertising failed (%d)", rc);
	} else {
		LOG_INF("BLE advertising started");
	}
}
k_work advertise_work{};
void connected(bt_conn *conn, uint8_t error)
{
	if (error != 0) {
		return;
	}
	k_mutex_lock(&ble_mutex, K_FOREVER);
	connection = bt_conn_ref(conn);
	protocol.reset();
	artwork_read_offset = 0;
	k_mutex_unlock(&ble_mutex);
	/* Bonded reconnection does not require the physical pairing window. The
	 * pairing_accept callback blocks all new pairing outside that window. */
	const int rc = bt_conn_set_security(conn, BT_SECURITY_L4);
	if (rc != 0) {
		LOG_WRN("BLE security request failed (%d)", rc);
		bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
	}
}
void disconnected(bt_conn *conn, uint8_t)
{
	k_mutex_lock(&ble_mutex, K_FOREVER);
	if (connection == conn) {
		bt_conn_unref(connection);
		connection = nullptr;
	}
	protocol.reset();
	passkey_visible = false;
	k_mutex_unlock(&ble_mutex);
	/* Recycling has not finished here. Advertising starts in recycled(). */
}
void recycled()
{
	k_work_submit(&advertise_work);
}
void security_changed(bt_conn *conn, bt_security_t level, bt_security_err error)
{
	LOG_INF("BLE security outcome: level %u error %u", static_cast<unsigned>(level),
		static_cast<unsigned>(error));
	if (error != BT_SECURITY_ERR_SUCCESS || level != BT_SECURITY_L4) {
		bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
	}
}
bt_conn_cb connection_callbacks{};
bt_security_err pairing_accept(bt_conn *, const bt_conn_pairing_feat *const)
{
	k_mutex_lock(&ble_mutex, K_FOREVER);
	const bool allowed =
		writable && !job_active && pairing_deadline > static_cast<uint64_t>(k_uptime_get());
	k_mutex_unlock(&ble_mutex);
	LOG_INF("BLE new pairing policy: %s", allowed ? "allowed" : "rejected");
	return allowed ? BT_SECURITY_ERR_SUCCESS : BT_SECURITY_ERR_PAIR_NOT_ALLOWED;
}
void passkey_display(bt_conn *conn, unsigned key)
{
	k_mutex_lock(&ble_mutex, K_FOREVER);
	const bool allowed = writable && pairing_deadline > static_cast<uint64_t>(k_uptime_get());
	displayed_passkey = key;
	passkey_visible = allowed;
	k_mutex_unlock(&ble_mutex);
	if (!allowed) {
		bt_conn_auth_cancel(conn);
		bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
	}
}
void pairing_cancel(bt_conn *)
{
	k_mutex_lock(&ble_mutex, K_FOREVER);
	passkey_visible = false;
	pairing_deadline = 0;
	k_mutex_unlock(&ble_mutex);
}
bt_conn_auth_cb authentication{};
void paired(bt_conn *conn, bool bonded)
{
	LOG_INF("BLE pairing completed: bonded %u", static_cast<unsigned>(bonded));
	pairing_cancel(conn);
}
void pairing_failed(bt_conn *conn, bt_security_err error)
{
	LOG_INF("BLE pairing failed (%u)", static_cast<unsigned>(error));
	pairing_cancel(conn);
}
bt_conn_auth_info_cb authentication_info{};

void worker(void *, void *, void *)
{
	k_sem_take(&ble_started, K_FOREVER);
	LOG_INF("BLE startup: registering callbacks");
	k_work_init(&status_work, notify_status);
	k_work_init(&advertise_work, advertise);
	status_ccc.cfg_changed = ccc_changed;
	connection_callbacks.connected = connected;
	connection_callbacks.disconnected = disconnected;
	connection_callbacks.recycled = recycled;
	connection_callbacks.security_changed = security_changed;
	bt_conn_cb_register(&connection_callbacks);
	authentication.pairing_accept = pairing_accept;
	authentication.passkey_display = passkey_display;
	authentication.cancel = pairing_cancel;
	authentication_info.pairing_complete = paired;
	authentication_info.pairing_failed = pairing_failed;
	int rc = bt_conn_auth_cb_register(&authentication);
	if (rc == 0) {
		rc = bt_conn_auth_info_cb_register(&authentication_info);
	}
	if (rc == 0) {
		LOG_INF("BLE startup: enabling host and IPC radio");
		rc = bt_enable(nullptr);
	}
	if (rc == 0) {
		LOG_INF("BLE startup: loading bonds");
		rc = settings_load_subtree("bt");
	}
	if (rc != 0) {
		LOG_ERR("BLE startup failed (%d)", rc);
		return;
	}
	k_mutex_lock(&ble_mutex, K_FOREVER);
	enabled = true;
	k_mutex_unlock(&ble_mutex);
	LOG_INF("BLE ready: configuration service available");
	k_work_submit(&advertise_work);
	while (true) {
		/* One worker owns the command; keep the 2 KiB asset off its stack,
		 * including the startup path through bt_enable(). */
		static ConfigCommand command;
		k_msgq_get(&config_requests, &command, K_FOREVER);
		k_mutex_lock(&ble_mutex, K_FOREVER);
		const bool current = protocol.current(command) && writable;
		k_mutex_unlock(&ble_mutex);
		int result = current ? 0 : -ECANCELED;
		if (result == 0 && command.field == Field::kUrl) {
			result = change_nfc_url(reinterpret_cast<const char *>(command.data),
						command.length);
		}
		if (result == 0 && command.field == kNfcSyncField) {
			result = resync_nfc();
		}
		/* Main owns all settings/mode/display state and publishes readback
		 * before status completion. No Bluetooth callback touches flash/I2C. */
		main_command = command;
		main_result = result;
		k_sem_give(&main_requested);
		k_sem_take(&main_completed, K_FOREVER);
	}
}
K_THREAD_DEFINE(ble_tid, 4096, worker, nullptr, nullptr, nullptr, 9, 0, 0);
} // namespace

void start_ble()
{
	k_sem_give(&ble_started);
}
void open_ble_pairing()
{
	k_mutex_lock(&ble_mutex, K_FOREVER);
	if (writable && enabled && !job_active) {
		pairing_deadline = k_uptime_get() + 60000;
	}
	k_mutex_unlock(&ble_mutex);
}
bool ble_config_busy()
{
	k_mutex_lock(&ble_mutex, K_FOREVER);
	const bool busy = job_active || passkey_visible;
	k_mutex_unlock(&ble_mutex);
	return busy;
}
bool ble_pairing_frame(Frame &frame)
{
	k_mutex_lock(&ble_mutex, K_FOREVER);
	const bool visible = passkey_visible;
	const uint32_t key = displayed_passkey;
	k_mutex_unlock(&ble_mutex);
	if (visible) {
		draw_passkey(frame, key);
	}
	return visible;
}
void poll_ble(const Settings &settings, Modes &modes, bool allow_writes, ApplyConfig apply,
	      void *context)
{
	const bool requested = k_sem_take(&main_requested, K_NO_WAIT) == 0;
	k_mutex_lock(&ble_mutex, K_FOREVER);
	writable = allow_writes;
	bool expired = pairing_deadline != 0 &&
		       (!writable || pairing_deadline <= static_cast<uint64_t>(k_uptime_get()));
	bt_conn *cancel = expired && passkey_visible && connection != nullptr
				  ? bt_conn_ref(connection)
				  : nullptr;
	if (expired) {
		pairing_deadline = 0;
		passkey_visible = false;
	}
	const bool current = requested && protocol.current(main_command);
	k_mutex_unlock(&ble_mutex);
	if (expired) {
		LOG_INF("BLE pairing window expired");
	}
	if (cancel != nullptr) {
		bt_conn_auth_cancel(cancel);
		bt_conn_disconnect(cancel, BT_HCI_ERR_AUTH_FAIL);
		bt_conn_unref(cancel);
	}
	if (requested && main_result == 0 && main_command.field != Field::kUrl &&
	    main_command.field != kNfcSyncField) {
		main_result = current && allow_writes ? apply(context, main_command) : -ECANCELED;
	}
	k_mutex_lock(&ble_mutex, K_FOREVER);
	/* settings references the live main-owned values, including apply() changes. */
	snapshot = settings;
	active_mode = modes.active();
	if (requested) {
		protocol.complete(main_command, main_result);
		job_active = false;
	}
	k_mutex_unlock(&ble_mutex);
	if (requested) {
		k_work_submit(&status_work);
		k_sem_give(&main_completed);
	}
}
} // namespace badge
