/* SPDX-License-Identifier: MIT */
#include "badge/audio_runtime.hpp"
#include "badge/audio_capture.hpp"
#if defined(CONFIG_APP_BLE)
#include "badge/ble.hpp"
#endif
#include "badge/audio_crypto.hpp"
#include "badge/adpcm.hpp"
#include "badge/recording.hpp"
#include "badge/text.hpp"
#include <zephyr/audio/dmic.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/atomic.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_REGISTER(badge_audio, CONFIG_APP_LOG_LEVEL);

namespace badge
{
namespace
{
#define AUDIO_UUID(n) BT_UUID_128_ENCODE(0xf3b00100 + (n), 0x6e9b, 0x4f4c, 0xa8f8, 0x731a9d46e1c0)
bt_uuid_128 service_uuid = BT_UUID_INIT_128(AUDIO_UUID(1));
bt_uuid_128 state_uuid = BT_UUID_INIT_128(AUDIO_UUID(2));
bt_uuid_128 control_uuid = BT_UUID_INIT_128(AUDIO_UUID(3));
bt_uuid_128 stream_uuid = BT_UUID_INIT_128(AUDIO_UUID(4));
K_MUTEX_DEFINE(audio_mutex);
K_SEM_DEFINE(sent, 0, 2);
atomic_t revoked;
atomic_t completion_token;
bool closing;
int64_t closing_deadline;
constexpr auto kCapture = audio_capture_profile(IS_ENABLED(CONFIG_SOC_NRF52840));
constexpr uint32_t kBlockMs = 20, kRate = kCapture.rate, kChannels = 2;
constexpr size_t kCaptureSamples = kRate * kBlockMs / 1000;
BUILD_ASSERT(kCaptureSamples >= kAudioFftSize && kCaptureSamples <= kAudioBlockSamples);
constexpr size_t kBlockBytes = kCaptureSamples * kChannels * sizeof(int16_t), kBuffers = 4;
K_MEM_SLAB_DEFINE_STATIC(capture_slab, kBlockBytes, kBuffers, 4);
const device *const microphone = DEVICE_DT_GET(DT_NODELABEL(pdm0));
AudioAnalyzer analyzer;
RecordingProcessor recording_processor;
AudioAnalysis analysis{};
int16_t mono[kCaptureSamples]{}, voice[320]{};
/* 31-tap FIR low-pass before 2:1 decimation; history is reset at every start. */
int16_t history[31]{};
unsigned history_cursor;
// Generated windowed-sinc coefficients: cutoff=0.22 cycles/input sample, unity DC.
constexpr int16_t filter[] = {53,    32,   -74,   -112,  106,   307,  -58,   -639,
			      -217,  1071, 942,   -1510, -2672, 1840, 10122, 14386,
			      10122, 1840, -2672, -1510, 942,   1071, -217,  -639,
			      -58,   307,  106,   -112,  -74,   32,   53};
bt_conn *connection;
uint8_t app_key[32]{}, public_key[65]{};
uint8_t nonce[16]{}, recording_nonce[16]{}, button_nonce[16]{}, recording_key[32]{};
uint8_t staging[96]{};
size_t staged;
bool initialized, keyed, proved, requested, recording, active, fatal;
bool stop_requested, visible, allow_start;
uint32_t counter, sequence;
int error;
int64_t unavailable_deadline;
int64_t window_deadline, button_deadline, heartbeat;
unsigned skipped;
constexpr uint16_t permissions_read = BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_READ_LESC;
constexpr uint16_t permissions_write = BT_GATT_PERM_WRITE_AUTHEN | BT_GATT_PERM_WRITE_LESC;
bool secure(bt_conn *conn)
{
	bt_conn_info info{};
	return conn && conn == connection && bt_conn_get_security(conn) == BT_SECURITY_L4 &&
	       bt_conn_get_info(conn, &info) == 0 &&
	       bt_le_bond_exists(info.id, bt_conn_get_dst(conn));
}
bool random_bytes(uint8_t *data, size_t size)
{
	return psa_generate_random(data, size) == PSA_SUCCESS;
}
void clear_session()
{
	keyed = proved = false;
	clear_audio(app_key, sizeof(app_key));
	clear_audio(public_key, sizeof(public_key));
}
ssize_t read_state(bt_conn *conn, const bt_gatt_attr *attr, void *buf, uint16_t length,
		   uint16_t offset)
{
	k_mutex_lock(&audio_mutex, K_FOREVER);
	if (!secure(conn)) {
		k_mutex_unlock(&audio_mutex);
		return BT_GATT_ERR(BT_ATT_ERR_AUTHENTICATION);
	}
	uint8_t state[125]{};
	state[0] = 2;
	state[1] = (keyed ? 1 : 0) | (proved ? 2 : 0) |
		   ((recording || requested || closing) ? 8 : 0) | (button_deadline ? 16 : 0) |
		   (IS_ENABLED(CONFIG_APP_AUDIO_RECORDING_BENCH) ? 32 : 0);
	sys_put_le32(error, state + 8);
	memcpy(state + 12, nonce, 16);
	memcpy(state + 28, recording_nonce, 16);
	memcpy(state + 44, button_nonce, 16);
	memcpy(state + 60, public_key, 65);
	ssize_t rc = bt_gatt_attr_read(conn, attr, buf, length, offset, state, sizeof(state));
	k_mutex_unlock(&audio_mutex);
	return rc;
}
bool subscribed();
int command(const uint8_t *data, size_t length)
{
	if (length == 66 && data[0] == 1) {
		// One automatic ephemeral exchange per authenticated bonded connection.
		if (keyed || recording || requested || closing) {
			return -EBUSY;
		}
		if (!audio_enrollment(data + 1, nonce, public_key, app_key)) {
			clear_session();
			return -EIO;
		}
		keyed = true;
		counter = 0;
		return 0;
	}
	if (length != 23 || data[0] < 2 || data[0] > 7 || data[0] == 6 || !keyed) {
		return -EACCES;
	}
	uint16_t value = 0;
	if (!audio_authenticate(app_key, nonce, button_nonce, data, length, counter, value)) {
		return -EACCES;
	}
	if (data[0] == 2) {
		proved = true;
		heartbeat = k_uptime_get();
		return 0;
	}
	if (!proved) {
		return -EACCES;
	}
	if (data[0] == 4) {
		stop_requested = true;
		atomic_set(&revoked, 1);
		return 0;
	}
	if (data[0] == 7) {
		heartbeat = k_uptime_get();
		return 0;
	}
	if (!IS_ENABLED(CONFIG_APP_AUDIO_RECORDING_BENCH)) {
		return -ENOTSUP;
	}
	if (!allow_start || recording || requested || closing || !subscribed() ||
#if defined(CONFIG_APP_BLE)
	    ble_artwork_busy() ||
#endif
	    bt_gatt_get_mtu(connection) < 108) {
		return -EBUSY;
	}
	if (data[0] == 5 && (!button_deadline || button_deadline <= k_uptime_get())) {
		return -ETIMEDOUT;
	}
	button_deadline = 0;
	clear_audio(button_nonce, 16);
	if (!random_bytes(recording_nonce, 16)) {
		return -EIO;
	}
	uint8_t derivation[20]{};
	memcpy(derivation, "PB3R", 4);
	memcpy(derivation + 4, recording_nonce, 16);
	bool ok = audio_mac(app_key, derivation, sizeof(derivation), recording_key);
	clear_audio(derivation, sizeof(derivation));
	if (!ok) {
		return -EIO;
	}
	requested = true;
	atomic_clear(&revoked);
	stop_requested = false;
	visible = false;
	heartbeat = k_uptime_get();
	sequence = 0;
	return 0;
}
ssize_t write_control(bt_conn *conn, const bt_gatt_attr *, const void *buffer, uint16_t length,
		      uint16_t offset, uint8_t flags)
{
	if (offset || flags || length < 1 || length > 20) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	const auto *data = static_cast<const uint8_t *>(buffer);
	k_mutex_lock(&audio_mutex, K_FOREVER);
	int rc = -EACCES;
	if (secure(conn) && initialized) {
		if (length == 1 && data[0] == 255) {
			const bool heartbeat_command = staged > 0 && staging[0] == 7;
			rc = command(staging, staged);
			staged = 0;
			clear_audio(staging, sizeof(staging));
			/* A late app heartbeat must not erase the capture failure it is
			 * about to read back. */
			if (rc != 0 || !heartbeat_command || recording || requested) {
				error = rc;
			}
		} else if (length >= 2 && data[0] == staged &&
			   staged + length - 1 <= sizeof(staging)) {
			memcpy(staging + staged, data + 1, length - 1);
			staged += length - 1;
			rc = 0;
		} else {
			staged = 0;
			clear_audio(staging, sizeof(staging));
			rc = -EINVAL;
		}
	}
	k_mutex_unlock(&audio_mutex);
	return rc == 0 ? length
		       : BT_GATT_ERR(rc == -EACCES ? BT_ATT_ERR_AUTHENTICATION
						   : BT_ATT_ERR_VALUE_NOT_ALLOWED);
}
void ccc_changed(const bt_gatt_attr *, uint16_t value)
{
	if (value == 0) {
		atomic_set(&revoked, 1);
		k_mutex_lock(&audio_mutex, K_FOREVER);
		stop_requested = true;
		atomic_set(&revoked, 1);
		k_mutex_unlock(&audio_mutex);
	}
}
bt_gatt_ccc_managed_user_data audio_ccc{};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
BT_GATT_SERVICE_DEFINE(audio_service, BT_GATT_PRIMARY_SERVICE(&service_uuid),
		       BT_GATT_CHARACTERISTIC(&state_uuid.uuid, BT_GATT_CHRC_READ, permissions_read,
					      read_state, nullptr, nullptr),
		       BT_GATT_CHARACTERISTIC(&control_uuid.uuid, BT_GATT_CHRC_WRITE,
					      permissions_write, nullptr, write_control, nullptr),
		       BT_GATT_CHARACTERISTIC(&stream_uuid.uuid, BT_GATT_CHRC_NOTIFY,
					      permissions_read, nullptr, nullptr, nullptr),
		       BT_GATT_CCC_MANAGED(&audio_ccc, permissions_read | permissions_write));
#pragma GCC diagnostic pop
bool subscribed()
{
	return secure(connection) &&
	       bt_gatt_is_subscribed(connection, &audio_service.attrs[6], BT_GATT_CCC_NOTIFY);
}
void release(void *buffer)
{
	clear_audio(buffer, kBlockBytes);
	k_mem_slab_free(&capture_slab, buffer);
}
int stop_capture()
{
	int rc = dmic_trigger(microphone, DMIC_TRIGGER_STOP);
	if (rc != 0) {
		return rc;
	}
	int64_t deadline = k_uptime_get() + 200;
	pcm_stream_cfg stream{};
	dmic_cfg config{};
	config.streams = &stream;
	config.channel.req_num_streams = 1;
	config.channel.req_num_chan = kChannels;
	config.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT) |
					 dmic_build_channel_map(1, 0, PDM_CHAN_RIGHT);
	while (true) {
		void *buffer = nullptr;
		size_t size = 0;
		while (dmic_read(microphone, 0, &buffer, &size, 0) == 0) {
			release(buffer);
		}
		unsigned irq = irq_lock();
		uint32_t used = k_mem_slab_num_used_get(&capture_slab);
		irq_unlock(irq);
		rc = used == 0 ? dmic_configure(microphone, &config) : -EBUSY;
		if (rc == 0) {
			for (size_t i = 0; i < kBuffers; ++i) {
				clear_audio(capture_slab.buffer + i * kBlockBytes + sizeof(void *),
					    kBlockBytes - sizeof(void *));
			}
			clear_audio(mono, sizeof(mono));
			clear_audio(voice, sizeof(voice));
			clear_audio(history, sizeof(history));
			analyzer.clear();
			recording_processor.clear();
			analysis = {};
			active = false;
			LOG_INF("capture stopped and buffers cleared");
			return 0;
		}
		if (rc != -EBUSY || k_uptime_get() >= deadline) {
			return rc == -EBUSY ? -ETIMEDOUT : rc;
		}
		(void)dmic_trigger(microphone, DMIC_TRIGGER_STOP);
		k_sleep(K_MSEC(1));
	}
}
int start_capture()
{
	recording_processor.clear();
	pcm_stream_cfg stream{};
	dmic_cfg config{};
	stream.pcm_rate = kRate;
	stream.pcm_width = 16;
	stream.block_size = kBlockBytes;
	stream.mem_slab = &capture_slab;
	config.io.min_pdm_clk_freq = kCapture.min_clock;
	config.io.max_pdm_clk_freq = kCapture.max_clock;
	config.io.min_pdm_clk_dc = 40;
	config.io.max_pdm_clk_dc = 60;
	config.streams = &stream;
	config.channel.req_num_streams = 1;
	config.channel.req_num_chan = kChannels;
	config.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT) |
					 dmic_build_channel_map(1, 0, PDM_CHAN_RIGHT);
	int rc = device_is_ready(microphone) ? dmic_configure(microphone, &config) : -ENODEV;
	if (rc == 0) {
		active = true;
		rc = dmic_trigger(microphone, DMIC_TRIGGER_START);
	}
	if (rc != 0) {
		LOG_WRN("capture start failed (%d)", rc);
	} else {
		LOG_INF("capture start requested");
	}
	skipped = 0;
	history_cursor = 0;
	clear_audio(history, sizeof(history));
	return rc;
}
void halt()
{
	requested = false;
	stop_requested = false;
	button_deadline = 0;
	clear_audio(button_nonce, 16);
	if (active && stop_capture() != 0) {
		fatal = true; /* Never remove REC after unconfirmed DMA stop. */
		sys_reboot(SYS_REBOOT_COLD);
	}
	recording = false;
	visible = false;
	clear_audio(recording_key, 32);
	clear_audio(recording_nonce, 16);
}
void transmitted(bt_conn *, void *token)
{
	if (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(token)) ==
	    static_cast<uint32_t>(atomic_get(&completion_token))) {
		k_sem_give(&sent);
	}
}
int send_voice(const int16_t *samples)
{
	if (sequence > UINT32_MAX - 2) {
		return -EOVERFLOW;
	}
	// A capture block contains two independent 10 ms packets. Queue both before
	// awaiting completions so the controller can send them in one connection event.
	uint8_t packets[2][105]{};
	int rc = 0;
	for (unsigned i = 0; i < 2; ++i) {
		uint8_t plain[kVoiceBytes]{}, iv[12]{}, aad[25]{};
		uint8_t *packet = packets[i];
		encode_voice(samples + i * kVoiceSamples, plain);
		packet[0] = 1;
		sys_put_le32(sequence + i, packet + 1);
		memcpy(iv, recording_nonce, 8);
		sys_put_le32(sequence + i, iv + 8);
		memcpy(aad, "PB3A", 4);
		memcpy(aad + 4, recording_nonce, 16);
		memcpy(aad + 20, packet, 5);
		bool ok = audio_encrypt(recording_key, iv, aad, sizeof(aad), plain, sizeof(plain),
					packet + 5);
		clear_audio(plain, sizeof(plain));
		clear_audio(iv, sizeof(iv));
		clear_audio(aad, sizeof(aad));
		if (!ok) {
			rc = -EIO;
			break;
		}
	}
	unsigned accepted = 0;
	k_sem_reset(&sent);
	const uintptr_t token = static_cast<uint32_t>(atomic_inc(&completion_token) + 1);
	if (rc == 0) {
		for (unsigned i = 0; i < 2; ++i) {
			if (!subscribed() || stop_requested || atomic_get(&revoked)) {
				rc = -ECANCELED;
				break;
			}
			bt_gatt_notify_params params{};
			params.attr = &audio_service.attrs[6];
			params.data = packets[i];
			params.len = sizeof(packets[i]);
			params.func = transmitted;
			params.user_data = reinterpret_cast<void *>(token);
			rc = bt_gatt_notify_cb(connection, &params);
			if (rc != 0) {
				break;
			}
			++accepted;
		}
	}
	const int64_t deadline = k_uptime_get() + 40;
	for (unsigned i = 0; i < accepted; ++i) {
		const int64_t remaining = deadline - k_uptime_get();
		if (remaining <= 0 || k_sem_take(&sent, K_MSEC(remaining)) != 0) {
			rc = -ETIMEDOUT;
			// Keep REC until unsent controller ciphertext has been discarded.
			closing = true;
			closing_deadline = k_uptime_get() + 5000;
			(void)bt_conn_disconnect(connection, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
			break;
		}
	}
	clear_audio(packets, sizeof(packets));
	sequence += 2;
	return rc;
}
} // namespace
void start_audio_service()
{
	k_mutex_lock(&audio_mutex, K_FOREVER);
	audio_ccc.cfg_changed = ccc_changed;
	initialized = psa_crypto_init() == PSA_SUCCESS;
	k_mutex_unlock(&audio_mutex);
}
void audio_connected(bt_conn *conn)
{
	k_mutex_lock(&audio_mutex, K_FOREVER);
	connection = conn;
	static const bt_le_conn_param parameters{6, 12, 0, 400};
	(void)bt_conn_le_param_update(conn, &parameters);
	proved = false;
	counter = 0;
	staged = 0;
	stop_requested = true;
	error = 0;
	clear_session();
	clear_audio(staging, sizeof(staging));
	if (!random_bytes(nonce, 16)) {
		initialized = false;
	}
	k_mutex_unlock(&audio_mutex);
}
void audio_disconnected()
{
	atomic_set(&revoked, 1);
	k_mutex_lock(&audio_mutex, K_FOREVER);
	connection = nullptr;
	closing = false;
	proved = false;
	stop_requested = true;
	atomic_set(&revoked, 1);
	clear_session();
	clear_audio(staging, sizeof(staging));
	staged = 0;
	clear_audio(nonce, 16);
	k_mutex_unlock(&audio_mutex);
}
bool audio_pairing_window()
{
	k_mutex_lock(&audio_mutex, K_FOREVER);
	const bool allowed = initialized && !recording && !requested && !closing;
	if (allowed) {
		window_deadline = k_uptime_get() + 60000;
	}
	k_mutex_unlock(&audio_mutex);
	return allowed;
}
bool audio_new_pairing()
{
	k_mutex_lock(&audio_mutex, K_FOREVER);
	const bool allowed = initialized && !recording && !requested && !closing &&
			     window_deadline > k_uptime_get();
	if (allowed) {
		clear_session();
	}
	k_mutex_unlock(&audio_mutex);
	return allowed;
}
bool audio_button(bool long_press)
{
	k_mutex_lock(&audio_mutex, K_FOREVER);
	bool consumed = false;
	if (recording || requested || closing) {
		stop_requested = true;
		atomic_set(&revoked, 1);
		consumed = true;
	} else if (long_press && IS_ENABLED(CONFIG_APP_AUDIO_RECORDING_BENCH) && proved &&
		   secure(connection) && subscribed() && allow_start
#if defined(CONFIG_APP_BLE)
		   && !ble_artwork_busy()
#endif
	) {
		consumed = true;
		if (random_bytes(button_nonce, 16)) {
			button_deadline = k_uptime_get() + 3000;
		}
	}
	if (long_press && !consumed) {
		unavailable_deadline = k_uptime_get() + 1000;
		consumed = true;
	}
	k_mutex_unlock(&audio_mutex);
	return consumed;
}
bool audio_ui_busy()
{
	k_mutex_lock(&audio_mutex, K_FOREVER);
	bool result = recording || requested || closing;
	k_mutex_unlock(&audio_mutex);
	return result;
}
bool audio_recording()
{
	k_mutex_lock(&audio_mutex, K_FOREVER);
	bool result = recording || requested || closing;
	k_mutex_unlock(&audio_mutex);
	return result;
}
bool audio_quiescent()
{
	k_mutex_lock(&audio_mutex, K_FOREVER);
	const bool quiet = !active && !recording && !requested && !closing;
	k_mutex_unlock(&audio_mutex);
	return quiet;
}

bool audio_capture_active()
{
	return active;
} // main owner only
AudioOverlay audio_overlay(Frame &frame)
{
	k_mutex_lock(&audio_mutex, K_FOREVER);
	const AudioOverlay result = audio_overlay_kind(recording, requested, closing,
						       unavailable_deadline > k_uptime_get());
	if (result == AudioOverlay::kRecording) {
		render_recording_audio(frame, analysis);
	} else if (result == AudioOverlay::kUnavailable) {
		draw_name(frame, "NO REC", 0);
	}
	k_mutex_unlock(&audio_mutex);
	return result;
}
void audio_display_result(int result)
{
	k_mutex_lock(&audio_mutex, K_FOREVER);
	if (result != 0) {
		clear_session();
		window_deadline = 0;
		allow_start = false;
		halt();
		error = result;
	} else if (requested) {
		visible = true;
	}
	k_mutex_unlock(&audio_mutex);
}
void audio_halt()
{
	k_mutex_lock(&audio_mutex, K_FOREVER);
	halt();
	k_mutex_unlock(&audio_mutex);
}
void audio_tick(bool display_healthy, bool visual, bool spectrum)
{
	k_mutex_lock(&audio_mutex, K_FOREVER);
	const int64_t now = k_uptime_get();
	if (closing && now >= closing_deadline) {
		sys_reboot(SYS_REBOOT_COLD);
	}
	if (button_deadline && button_deadline <= now) {
		button_deadline = 0;
		clear_audio(button_nonce, 16);
	}
	allow_start = initialized && display_healthy && !fatal;
	if ((recording || requested) && (!allow_start || stop_requested || !subscribed() ||
					 !proved || now - heartbeat > 1500)) {
		halt();
	}
	if (requested && visible) {
		// Discard visualization data and stop its DMA before starting this session.
		if (active && stop_capture() != 0) {
			fatal = true;
			sys_reboot(SYS_REBOOT_COLD);
		}
		error = start_capture();
		if (error == 0) {
			recording = true;
			requested = false;
		} else {
			halt();
		}
	}
	if (!recording && !requested) {
		if (!allow_start || !visual || closing) {
			if (active) {
				halt();
			}
		} else if (!active) {
			error = start_capture();
			if (error) {
				halt();
			}
		}
	}
	if (active && (!requested || recording)) {
		void *buffer = nullptr;
		size_t bytes = 0;
		/* DMIC START returns before clock/DMA startup. A 20 ms block needs
		 * startup and scheduler margin; match the working screening path. */
		int rc = dmic_read(microphone, 0, &buffer, &bytes, 100);
		if (rc != 0 || !buffer || bytes != kBlockBytes) {
			if (buffer) {
				release(buffer);
			}
			error = rc ? rc : -EIO;
			LOG_WRN("capture read failed (%d)", error);
			halt();
		} else if (skipped++ < 10) {
			if (skipped == 1) {
				LOG_INF("capture DMA active");
			}
			release(buffer);
		} else {
			const auto *pcm = static_cast<const int16_t *>(buffer);
			for (size_t i = 0; i < kCaptureSamples; ++i) {
				mono[i] = pcm[2 * i + CONFIG_APP_AUDIO_CHANNEL];
			}
			release(buffer);
			analyzer.analyze(mono, kCaptureSamples, analysis);
			if (recording) {
				if constexpr (kRate == 16000) {
					static_assert(kCaptureSamples == kRecordingSamples ||
						      kRate != 16000);
					memcpy(voice, mono, sizeof(voice));
				} else {
					for (unsigned i = 0; i < kCaptureSamples; ++i) {
						history[history_cursor] = mono[i];
						history_cursor = (history_cursor + 1) % 31;
						if (i & 1) {
							int64_t sum = 0;
							for (unsigned j = 0; j < 31; ++j) {
								sum += static_cast<int32_t>(
									       history[(history_cursor +
											j) %
										       31]) *
								       filter[j];
							}
							int64_t value = sum / 32768;
							voice[i / 2] = value < -32768  ? -32768
								       : value > 32767 ? 32767
										       : value;
						}
					}
				}
				rc = recording_processor.process(voice, kRecordingSamples)
					     ? send_voice(voice)
					     : -EIO;
				if (rc != 0) {
					error = rc;
					LOG_WRN("encrypted audio send failed (%d)", error);
					halt();
				}
			}
			clear_audio(mono, sizeof(mono));
			clear_audio(voice, sizeof(voice));
		}
	}
	(void)spectrum;
	k_mutex_unlock(&audio_mutex);
}
/* Local meter is only copied into the display; it is never a BLE audio source. */
void audio_visual_frame(Frame &frame, bool spectrum)
{
	render_audio(frame, analysis, spectrum);
}
} // namespace badge
