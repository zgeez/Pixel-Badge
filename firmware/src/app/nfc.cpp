/* SPDX-License-Identifier: MIT */
#include "badge/nfc.hpp"
#include "badge/nfc_platform.hpp"
#include "badge/nfc_session.hpp"
#include "badge/settings_store.hpp"
#include "badge/modes.hpp"
#include "badge/url_update.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/barrier.h>
#include <zephyr/sys/atomic.h>
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(nfc, CONFIG_APP_LOG_LEVEL);

namespace badge
{
namespace
{
K_SEM_DEFINE(started, 0, 1);
K_MUTEX_DEFINE(state_mutex);
char desired[kPayloadMax + 1];
char verified[kPayloadMax + 1];
uint8_t expected_uid[7];
bool writable;
NfcSession session;
int last_result = -EAGAIN;
UrlUpdate url_update;
struct UrlRequest {
	char url[kPayloadMax + 1];
	bool resync;
};
K_MSGQ_DEFINE(url_requests, sizeof(UrlRequest), 1, 4);
K_MUTEX_DEFINE(request_mutex);
K_SEM_DEFINE(request_done, 0, 1);
atomic_t ready_state;
atomic_t operation_active;
atomic_t public_result;
int request_result;

int synchronize(Ntag &tag, const char *url)
{
	int rc = prepare_nfc_transport();
	if (rc == 0) {
		rc = session.synchronize(tag, url, strlen(url), verified);
		memcpy(expected_uid, session.uid(), sizeof(expected_uid));
	}
	if (rc == 0) {
		memcpy(verified, url, strlen(url) + 1);
	}
	return rc;
}

#if defined(CONFIG_APP_NFC_DEBUG_COMMANDS)
/* Optional legacy RF tools retain mailbox v3. Readiness is local sync only. */
constexpr uint8_t kProofVersion = 3;
struct Mailbox {
	uint32_t magic, version, request, response, command;
	int32_t result;
	uint32_t state, address;
	uint8_t uid[8];
	char desired[24], verified[24], input[24];
	uint8_t credentials[8];
};
static_assert(sizeof(Mailbox) == 120);
extern "C" {
__used volatile Mailbox badge_nfc_mailbox = {
	0x424e4643, kProofVersion, 0, 0, 0, -EAGAIN, 0, 0, {}, {}, {}, {}, {}};
}

void publish(Ntag &tag, int result, uint32_t sequence)
{
	badge_nfc_mailbox.result = result;
	badge_nfc_mailbox.state = last_result == 0 ? 1 : 0;
	badge_nfc_mailbox.address = tag.address();
	for (unsigned i = 0; i < sizeof(expected_uid); ++i) {
		badge_nfc_mailbox.uid[i] = expected_uid[i];
	}
	for (unsigned i = 0; i < sizeof(desired); ++i) {
		badge_nfc_mailbox.desired[i] = desired[i];
		badge_nfc_mailbox.verified[i] = verified[i];
	}
	/* Sequence is the publication barrier; the host waits for equality. */
	barrier_dmem_fence_full();
	badge_nfc_mailbox.response = sequence;
}

#endif

int change_url(Ntag &tag, const char *url, size_t length)
{
	k_mutex_lock(&state_mutex, K_FOREVER);
	int rc = url_update.begin(url, length, k_uptime_get());
	k_mutex_unlock(&state_mutex);
	if (rc != 0) {
		return rc;
	}
	while (true) {
		char target[kPayloadMax + 1];
		k_mutex_lock(&state_mutex, K_FOREVER);
		const bool work = url_update.take_tag(target, k_uptime_get());
		const bool done = url_update.phase() == UrlUpdate::Phase::kDone;
		if (done) {
			rc = url_update.result();
			if (rc == 0) {
				memcpy(desired, url, length);
				desired[length] = '\0';
			} else if (url_update.rollback_result() != 0) {
				LOG_WRN("NFC URL rollback failed (%d); tag remains unverified",
					url_update.rollback_result());
			}
		}
		k_mutex_unlock(&state_mutex);
		if (done) {
			return rc;
		}
		if (work) {
			const int result = synchronize(tag, target);
			k_mutex_lock(&state_mutex, K_FOREVER);
			url_update.tag_result(result, k_uptime_get());
			k_mutex_unlock(&state_mutex);
		} else {
			/* Cancellation restores main-loop state before completion. A
			 * permanently stalled main loop resets through its watchdog. */
			k_sleep(K_MSEC(20));
		}
	}
}

void service_url_request(Ntag &tag)
{
	UrlRequest request;
	if (k_msgq_get(&url_requests, &request, K_NO_WAIT) == 0) {
		atomic_set(&operation_active, 1);
		atomic_clear(&ready_state);
		request_result = !writable ? -EPERM
				 : request.resync
					 ? synchronize(tag, desired)
					 : change_url(tag, request.url, strlen(request.url));
		last_result = request_result;
		atomic_set(&public_result, last_result);
		atomic_set(&ready_state, last_result == 0);
		atomic_clear(&operation_active);
		k_sem_give(&request_done);
	}
}

#if defined(CONFIG_APP_NFC_DEBUG_COMMANDS)
void commands(Ntag &tag)
{
	bool pending = false;
	uint8_t pending_uid[7]{};
	int64_t pending_at = 0;
	while (true) {
		service_url_request(tag);
		const uint32_t sequence = badge_nfc_mailbox.request;
		if (sequence == badge_nfc_mailbox.response) {
			k_sleep(K_MSEC(20));
			continue;
		}
		const uint32_t command = badge_nfc_mailbox.command;
		atomic_set(&operation_active, 1);
		char input[24];
		uint8_t credentials[8];
		for (unsigned i = 0; i < sizeof(input); ++i) {
			input[i] = badge_nfc_mailbox.input[i];
			badge_nfc_mailbox.input[i] = 0;
		}
		for (unsigned i = 0; i < sizeof(credentials); ++i) {
			credentials[i] = badge_nfc_mailbox.credentials[i];
			badge_nfc_mailbox.credentials[i] = 0;
		}
		int rc = -EINVAL;
		if (command == 1) {
			rc = last_result;
		} else if (!writable) {
			rc = -EPERM;
		} else if (command == 2) {
			rc = change_url(tag, input, strnlen(input, sizeof(input)));
			last_result = rc;
		} else if (command == 3) {
			pending = false;
			rc = 0;
			const uint32_t events = tag.rf_events();
			if (rc == 0) {
				rc = prepare_nfc_transport();
			}
			if (rc == 0) {
				rc = tag.initialize();
			}
			if (rc == 0) {
				rc = tag.begin_provision(desired, strlen(desired), credentials,
							 pending_uid);
			}
			if (tag.rf_events() != events) {
				rc = -EACCES;
			}
			if (rc == 0) {
				memcpy(expected_uid, pending_uid, sizeof(expected_uid));
				pending = true;
				pending_at = k_uptime_get();
				LOG_INF("NFC setup pending finalization; deadline 120000 ms");
			}
			last_result = -EAGAIN; /* Local finalization still required. */
		} else if (command == 4 || command == 6) {
			const int64_t elapsed = k_uptime_get() - pending_at;
			if (!pending) {
				LOG_WRN("NFC confirmation rejected: no pending provision");
				rc = -EACCES;
			} else if (elapsed > 120000) {
				LOG_WRN("NFC confirmation rejected: expired after %lld ms",
					static_cast<long long>(elapsed));
				rc = -EACCES;
			} else if (memcmp(input, pending_uid, sizeof(pending_uid)) != 0) {
				LOG_WRN("NFC confirmation rejected: tag identity mismatch");
				rc = -EACCES;
			} else {
				LOG_INF("NFC confirmation received after %lld ms",
					static_cast<long long>(elapsed));
				const uint32_t events = tag.rf_events();
				rc = tag.finish_provision(pending_uid);
				if (rc != 0) {
					LOG_WRN("NFC guarded finalization check failed (%d)", rc);
				}
				if (rc == 0 && tag.rf_events() != events) {
					LOG_WRN("NFC confirmation rejected: RF transition during "
						"finalization");
					rc = -EACCES;
				}
				if (rc == 0) {
					session.bind(pending_uid);
					rc = synchronize(tag, desired);
				}
				last_result = rc;
			}
			pending = false;
			last_result = rc;
		} else if (command == 5) {
			rc = synchronize(tag, desired);
			last_result = rc;
		}
		erase_sensitive(credentials, sizeof(credentials));
		erase_sensitive(input, sizeof(input));
		publish(tag, rc, sequence);
		atomic_set(&public_result, last_result);
		atomic_set(&ready_state, last_result == 0);
		atomic_clear(&operation_active);
		LOG_INF("NFC command %u result %d; verified %s", command, rc,
			last_result == 0 ? "yes" : "no");
	}
}
#endif

void worker(void *, void *, void *)
{
	k_sem_take(&started, K_FOREVER);
	Ntag tag(nfc_transport());
	last_result = writable ? synchronize(tag, desired) : -EPERM;
	atomic_set(&public_result, last_result);
	atomic_set(&ready_state, last_result == 0);
	atomic_clear(&operation_active);
	LOG_INF("NFC boot result %d; %s", last_result,
		last_result == 0 ? "QR/NFC synchronized"
				 : "sync failed; remove reader field and retry");
#if defined(CONFIG_APP_NFC_DEBUG_COMMANDS)
	publish(tag, last_result, 0);
	commands(tag);
#else
	while (true) {
		service_url_request(tag);
		k_sleep(K_MSEC(20));
	}
#endif
}
K_THREAD_DEFINE(nfc_tid, 4096, worker, nullptr, nullptr, nullptr, 8, 0, 0);
} // namespace

void start_nfc(const char *url, bool allow_writes)
{
	atomic_set(&operation_active, 1);
	memcpy(desired, url, strnlen(url, kPayloadMax) + 1);
	writable = allow_writes;
	k_sem_give(&started);
}

bool poll_nfc(SettingsService &settings, Modes &modes)
{
	k_mutex_lock(&state_mutex, K_FOREVER);
	const bool changed = url_update.poll(settings, modes, store_field, nullptr, k_uptime_get());
	k_mutex_unlock(&state_mutex);
	return changed;
}

void nfc_display_result(int result)
{
	k_mutex_lock(&state_mutex, K_FOREVER);
	url_update.display_result(result, k_uptime_get());
	k_mutex_unlock(&state_mutex);
}

int change_nfc_url(const char *url, size_t length)
{
	if (!valid_url(url, length)) {
		return -EINVAL;
	}
	if (k_mutex_lock(&request_mutex, K_NO_WAIT) != 0) {
		return -EBUSY;
	}
	UrlRequest request{};
	memcpy(request.url, url, length);
	k_sem_reset(&request_done);
	int rc = k_msgq_put(&url_requests, &request, K_NO_WAIT);
	if (rc == 0) {
		k_sem_take(&request_done, K_FOREVER);
		rc = request_result;
	}
	k_mutex_unlock(&request_mutex);
	return rc;
}

int resync_nfc()
{
	if (k_mutex_lock(&request_mutex, K_NO_WAIT) != 0) {
		return -EBUSY;
	}
	UrlRequest request{};
	request.resync = true;
	k_sem_reset(&request_done);
	int rc = k_msgq_put(&url_requests, &request, K_NO_WAIT);
	if (rc == 0) {
		k_sem_take(&request_done, K_FOREVER);
		rc = request_result;
	}
	k_mutex_unlock(&request_mutex);
	return rc;
}

int nfc_last_result()
{
	return atomic_get(&public_result);
}

bool nfc_ready()
{
	return atomic_get(&ready_state) != 0;
}
bool nfc_update_busy()
{
	return atomic_get(&operation_active) != 0;
}
} // namespace badge
