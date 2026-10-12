/* SPDX-License-Identifier: MIT */
#include "badge/url_update.hpp"
#include <zephyr/ztest.h>
#include <errno.h>
#include <string.h>

using namespace badge;
using Phase = UrlUpdate::Phase;
constexpr char old_url[] = "HTTPS://A.B/X";
constexpr char new_url[] = "HTTPS://A.B/Y";

struct Fixture {
	SettingsService settings;
	Modes modes;
	UrlUpdate update;
	char durable[kPayloadMax + 1]{};
	unsigned writes = 0;
	int storage_error = 0;
	Fixture()
	{
		settings.begin_load();
		const uint8_t schema = kSchemaVersion;
		zassert_true(settings.load(Field::kSchema, &schema, 1));
		zassert_true(settings.load(Field::kUrl, old_url, strlen(old_url)));
		settings.finish_load();
		modes.start(settings.values(), false);
		strcpy(durable, old_url);
	}
	static int save(void *p, Field field, const void *data, size_t length)
	{
		auto &f = *static_cast<Fixture *>(p);
		zassert_equal(field, Field::kUrl);
		++f.writes;
		if (f.storage_error != 0) {
			return f.storage_error;
		}
		memcpy(f.durable, data, length);
		f.durable[length] = '\0';
		return 0;
	}
	bool poll(uint64_t now = 10)
	{
		return update.poll(settings, modes, save, this, now);
	}
	void begin()
	{
		zassert_ok(update.begin(new_url, strlen(new_url), 0));
		zassert_true(poll());
		zassert_equal(update.phase(), Phase::kDisplay);
	}
	void unchanged()
	{
		zassert_equal(strcmp(durable, old_url), 0);
		zassert_equal(strcmp(settings.values().url, old_url), 0);
		SettingsService rebooted;
		rebooted.begin_load();
		const uint8_t schema = kSchemaVersion;
		zassert_true(rebooted.load(Field::kSchema, &schema, 1));
		zassert_true(rebooted.load(Field::kUrl, durable, strlen(durable)));
		rebooted.finish_load();
		zassert_equal(strcmp(rebooted.values().url, old_url), 0);
	}
	void qr_is(const char *url)
	{
		Frame actual, expected;
		modes.render(actual, 0);
		zassert_true(encode_url(url, strlen(url), expected));
		zassert_mem_equal(&actual, &expected, sizeof(actual));
	}
	void take(const char *url, uint64_t now = 30)
	{
		char target[kPayloadMax + 1];
		zassert_true(update.take_tag(target, now));
		zassert_equal(strcmp(target, url), 0);
	}
	void tag_passed()
	{
		update.display_result(0, 20);
		take(new_url);
		update.tag_result(0, 40);
		zassert_equal(update.phase(), Phase::kCommit);
	}
};

ZTEST(url_update, test_success_persists_only_after_both_verifications)
{
	Fixture f;
	f.begin();
	f.unchanged();
	f.qr_is(new_url);
	f.tag_passed();
	zassert_equal(f.writes, 0);
	f.unchanged();
	f.poll(50);
	zassert_equal(f.update.phase(), Phase::kDone);
	zassert_ok(f.update.result());
	zassert_equal(f.writes, 1);
	zassert_equal(strcmp(f.durable, new_url), 0);
	zassert_equal(strcmp(f.settings.values().url, new_url), 0);
}

ZTEST(url_update, test_display_failure_restores_url_and_previous_mode)
{
	Fixture f;
	f.modes.next(0);
	zassert_equal(f.modes.active(), Mode::kName);
	f.begin();
	f.update.display_result(-EBUSY, 20);
	zassert_true(f.poll());
	zassert_equal(f.modes.active(), Mode::kName);
	zassert_equal(f.update.phase(), Phase::kDone);
	zassert_equal(f.update.result(), -EBUSY);
	zassert_equal(f.writes, 0);
	f.unchanged();
}

ZTEST(url_update, test_timeout_after_consumption_ignores_late_display_success)
{
	Fixture f;
	f.begin();
	f.update.expire(3000);
	f.update.display_result(0, 3001);
	f.poll(3002);
	zassert_equal(f.update.result(), -ETIMEDOUT);
	zassert_equal(f.update.phase(), Phase::kDone);
	f.qr_is(old_url);
	f.unchanged();
	zassert_equal(f.writes, 0);
	char target[kPayloadMax + 1];
	zassert_false(f.update.take_tag(target, 3003));
}

ZTEST(url_update, test_expired_unconsumed_request_never_previews_or_persists)
{
	Fixture f;
	zassert_ok(f.update.begin(new_url, strlen(new_url), 0));
	zassert_false(f.poll(3000));
	f.update.display_result(0, 3001);
	f.unchanged();
	f.qr_is(old_url);
	zassert_equal(f.writes, 0);
	zassert_equal(f.update.result(), -ETIMEDOUT);
}

ZTEST(url_update, test_tag_failure_rolls_back_without_new_durable_url)
{
	Fixture f;
	f.begin();
	f.update.display_result(0, 20);
	f.take(new_url);
	f.update.tag_result(-EIO, 40);
	f.poll(50);
	f.qr_is(old_url);
	f.unchanged();
	zassert_equal(f.update.phase(), Phase::kRollbackTag);
	f.take(old_url, 60);
	f.update.tag_result(0, 70);
	zassert_equal(f.update.phase(), Phase::kDone);
	zassert_equal(f.update.result(), -EIO);
	zassert_ok(f.update.rollback_result());
	zassert_equal(f.writes, 0);
}

ZTEST(url_update, test_timeout_during_tag_work_or_before_commit_never_persists)
{
	constexpr bool scenarios[] = {false, true};
	for (bool during_tag : scenarios) {
		Fixture f;
		f.begin();
		f.update.display_result(0, 20);
		f.take(new_url);
		if (during_tag) {
			f.update.expire(3000);
			zassert_true(f.poll(3000)); /* Restore QR without waiting for I2C. */
			f.qr_is(old_url);
			f.unchanged();
			zassert_equal(f.update.phase(), Phase::kWorkingTag);
			zassert_equal(f.update.begin(new_url, strlen(new_url), 3000), -EBUSY);
		}
		f.update.tag_result(0, during_tag ? 3001 : 40);
		f.poll(3002);
		f.qr_is(old_url);
		f.take(old_url, 3003);
		f.update.tag_result(0, 3004);
		f.unchanged();
		zassert_equal(f.writes, 0);
		zassert_equal(f.update.result(), -ETIMEDOUT);
	}
}

ZTEST(url_update, test_storage_failure_and_failed_tag_rollback_remain_errors)
{
	Fixture f;
	f.begin();
	f.tag_passed();
	f.storage_error = -ENOSPC;
	f.poll(50);
	f.qr_is(old_url);
	f.take(old_url, 60);
	f.update.tag_result(-EACCES, 70);
	f.unchanged();
	zassert_equal(f.writes, 1);
	zassert_equal(f.update.result(), -ENOSPC);
	zassert_equal(f.update.rollback_result(), -EACCES);
	zassert_equal(f.update.phase(), Phase::kDone);
}

ZTEST(url_update, test_cancelled_session_cannot_accept_a_replacement_or_stale_ack)
{
	Fixture f;
	f.begin();
	zassert_equal(f.update.begin(old_url, strlen(old_url), 20), -EBUSY);
	f.update.display_result(-EIO, 30);
	f.poll(40);
	zassert_ok(f.update.begin(new_url, strlen(new_url), 50));
	f.update.display_result(0, 51); /* Prior frame, before the new preview. */
	zassert_equal(f.update.phase(), Phase::kPrepare);
	zassert_true(f.poll(52));
	zassert_equal(f.update.phase(), Phase::kDisplay);
	f.unchanged();
	zassert_equal(f.writes, 0);
}

ZTEST_SUITE(url_update, nullptr, nullptr, nullptr, nullptr, nullptr);
