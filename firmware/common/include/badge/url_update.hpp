/* SPDX-License-Identifier: MIT */
#pragma once

#include "badge/modes.hpp"

namespace badge
{
/* Caller serializes all methods. Main owns settings/modes; the NFC worker
 * claims tag work and reports its completion outside that critical section. */
class UrlUpdate
{
public:
	enum class Phase {
		kIdle,
		kPrepare,
		kDisplay,
		kTag,
		kWorkingTag,
		kCommit,
		kRestore,
		kRollbackTag,
		kWorkingRollback,
		kDone
	};
	int begin(const char *url, size_t length, uint64_t now);
	bool poll(SettingsService &settings, Modes &modes, SaveField write, void *context,
		  uint64_t now);
	void display_result(int result, uint64_t now);
	bool take_tag(char output[kPayloadMax + 1], uint64_t now);
	void tag_result(int result, uint64_t now);
	void expire(uint64_t now);
	Phase phase() const
	{
		return phase_;
	}
	int result() const
	{
		return result_;
	}
	int rollback_result() const
	{
		return rollback_result_;
	}

private:
	Phase phase_ = Phase::kIdle;
	char requested_[kPayloadMax + 1]{}, previous_[kPayloadMax + 1]{};
	Mode previous_mode_ = Mode::kQr;
	uint64_t deadline_ = 0;
	int result_ = 0, rollback_result_ = 0;
	bool previewed_ = false, tag_touched_ = false;
	void fail(int result);
};
} // namespace badge
