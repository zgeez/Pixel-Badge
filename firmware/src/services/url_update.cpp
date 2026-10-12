/* SPDX-License-Identifier: MIT */
#include "badge/url_update.hpp"
#include <errno.h>
#include <string.h>

namespace badge
{
int UrlUpdate::begin(const char *url, size_t length, uint64_t now)
{
	if (phase_ != Phase::kIdle && phase_ != Phase::kDone) {
		return -EBUSY;
	}
	if (!valid_url(url, length)) {
		return -EINVAL;
	}
	memcpy(requested_, url, length);
	requested_[length] = '\0';
	deadline_ = now + 3000;
	result_ = -EINPROGRESS;
	rollback_result_ = 0;
	previewed_ = tag_touched_ = false;
	phase_ = Phase::kPrepare;
	return 0;
}

void UrlUpdate::fail(int result)
{
	if (result_ == -EINPROGRESS) {
		result_ = result;
	}
	/* A running I2C operation must finish before its rollback is claimed. */
	if (phase_ != Phase::kWorkingTag) {
		phase_ = previewed_ ? Phase::kRestore : Phase::kDone;
	}
}

void UrlUpdate::expire(uint64_t now)
{
	if (now >= deadline_ &&
	    (phase_ == Phase::kPrepare || phase_ == Phase::kDisplay || phase_ == Phase::kTag ||
	     phase_ == Phase::kWorkingTag || phase_ == Phase::kCommit)) {
		fail(-ETIMEDOUT);
	}
}

bool UrlUpdate::poll(SettingsService &settings, Modes &modes, SaveField write, void *context,
		     uint64_t now)
{
	expire(now);
	if (phase_ == Phase::kPrepare) {
		Settings candidate = settings.values();
		memcpy(previous_, candidate.url, sizeof(previous_));
		memcpy(candidate.url, requested_, sizeof(candidate.url));
		previous_mode_ = modes.active();
		if (!modes.configure(candidate)) {
			fail(-EIO);
			return false;
		}
		if (modes.active() != Mode::kQr) {
			modes.next(0);
		}
		previewed_ = true;
		phase_ = Phase::kDisplay;
		return true;
	}
	if (phase_ == Phase::kCommit) {
		/* No setting write occurs until display and tag verification passed.
		 * The owner's critical section makes this final decision indivisible
		 * with worker cancellation; a late request cannot persist afterwards. */
		const int rc =
			settings.save(Field::kUrl, requested_, strlen(requested_), write, context);
		if (rc == 0) {
			result_ = 0;
			phase_ = Phase::kDone;
			return false;
		}
		fail(rc);
	}
	if (phase_ == Phase::kRestore ||
	    (phase_ == Phase::kWorkingTag && result_ != -EINPROGRESS && previewed_)) {
		const bool changed = previewed_;
		const bool restored = !previewed_ || modes.configure(settings.values());
		if (previewed_ && restored && modes.active() != previous_mode_) {
			modes.next(0);
		}
		previewed_ = false;
		if (phase_ == Phase::kRestore) {
			phase_ = tag_touched_ ? Phase::kRollbackTag : Phase::kDone;
		}
		return changed && restored;
	}
	return false;
}

void UrlUpdate::display_result(int result, uint64_t now)
{
	expire(now);
	if (phase_ == Phase::kDisplay) {
		if (result == 0) {
			phase_ = Phase::kTag;
		} else {
			fail(result);
		}
	}
}

bool UrlUpdate::take_tag(char output[kPayloadMax + 1], uint64_t now)
{
	expire(now);
	if (phase_ == Phase::kTag) {
		memcpy(output, requested_, sizeof(requested_));
		tag_touched_ = true;
		phase_ = Phase::kWorkingTag;
		return true;
	}
	if (phase_ == Phase::kRollbackTag) {
		memcpy(output, previous_, sizeof(previous_));
		phase_ = Phase::kWorkingRollback;
		return true;
	}
	return false;
}

void UrlUpdate::tag_result(int result, uint64_t now)
{
	expire(now);
	if (phase_ == Phase::kWorkingTag) {
		if (result == 0 && result_ == -EINPROGRESS) {
			phase_ = Phase::kCommit;
		} else {
			if (result_ == -EINPROGRESS) {
				result_ = result;
			}
			phase_ = Phase::kRestore;
		}
	} else if (phase_ == Phase::kWorkingRollback) {
		rollback_result_ = result;
		phase_ = Phase::kDone;
	}
}
} // namespace badge
