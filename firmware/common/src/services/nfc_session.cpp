/* SPDX-License-Identifier: MIT */
#include "badge/nfc_session.hpp"
#include <errno.h>
#include <string.h>

namespace badge
{
void NfcSession::bind(const uint8_t uid[7])
{
	memcpy(uid_, uid, sizeof(uid_));
	bound_ = true;
}
int NfcSession::synchronize(Ntag &tag, const char *url, size_t length,
			    char previous[kPayloadMax + 1])
{
	const uint32_t events = tag.rf_events();
	int rc = tag.initialize();
	uint8_t actual[7];
	if (rc == 0) {
		rc = tag.verify_policy(actual);
	}
	if (rc == 0 && bound_ && memcmp(uid_, actual, sizeof(uid_)) != 0) {
		rc = -EACCES;
	}
	if (rc == 0) {
		bind(actual);
		char old[kPayloadMax + 1];
		const int read = tag.read_url(old);
		if (read == 0) {
			memcpy(previous, old, sizeof(old));
		} else if (read != -EBADMSG) {
			rc = read;
		}
	}
	if (rc == 0) {
		rc = tag.synchronize(url, length);
	}
	if (tag.rf_events() != events) {
		return -EAGAIN; // Retry through a new guarded reconciliation, never success.
	}
	return rc;
}
} // namespace badge
