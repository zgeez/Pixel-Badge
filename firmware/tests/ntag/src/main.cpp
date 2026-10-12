/* SPDX-License-Identifier: MIT */
#include "badge/ntag.hpp"
#include <zephyr/ztest.h>
#include <errno.h>
#include <string.h>

using namespace badge;

struct Tag {
	uint8_t memory[0x3b][16]{};
	uint8_t session[7]{};
	uint8_t address = 0x55;
	uint8_t selector = 0, reg = 0;
	bool locked = false, armed = false, register_read = false, rf = false;
	bool busy_forever = false, drop_write = false;
	bool fail_error_clear = false, drop_error_clear = false;
	unsigned fail_program_writes = 0, error_clears = 0;
	unsigned releases = 0;
	int release_error = 0, fail_read_block = -1;
	bool programming_pending = false, status_failure_forever = false;
	bool status_lock_forever = false, ambiguous_write_error = false;
	unsigned status_failures = 0, status_lock_failures = 0, status_polls = 0;
	int status_error = -EIO, status_lock_error = -EBUSY;
	unsigned status_lock_errors = 0;
	uint32_t frame_us = 0;
	bool fail_disarm = false;
	bool pulse_on_enable = false, field_on_enable = false, fail_enable = false;
	unsigned field_reads = 0, fail_field_read = 0;
	unsigned writes = 0, frames = 0, fail_frame = 0;
	int edges = 0;
	uint64_t clock = 0, done = 0;
	Tag()
	{
		memory[0][0] = 4;
		memory[0][1] = 0x12;
		memory[0x38][15] = 0xff;
		memory[0x3a][0] = 1;
		memory[0x3a][3] = 0x48;
		memory[0x3a][4] = 8;
		session[0] = 1;
	}
	static int lock(void *p, bool acquire)
	{
		auto &t = *static_cast<Tag *>(p);
		if (acquire && t.programming_pending &&
		    (t.status_lock_forever || t.status_lock_failures != 0)) {
			if (t.status_lock_failures != 0) {
				--t.status_lock_failures;
			}
			++t.status_lock_errors;
			return t.status_lock_error;
		}
		zassert_equal(t.locked, !acquire);
		t.locked = acquire;
		return 0;
	}
	static void clear_edges(void *p)
	{
		auto &t = *static_cast<Tag *>(p);
		zassert_false(t.armed, "Latch cleared after interrupt enable");
		t.edges = 0;
	}
	static int interrupt(void *p, bool enable)
	{
		auto &t = *static_cast<Tag *>(p);
		zassert_true(t.locked);
		t.armed = enable;
		if (!enable && t.fail_disarm) {
			return -EIO;
		}
		if (enable) {
			if (t.pulse_on_enable) {
				t.edges += 2; /* Field rises and falls inside interrupt enable. */
			}
			if (t.field_on_enable) {
				t.rf = true;
			}
			if (t.fail_enable) {
				return -EIO;
			}
		}
		return 0;
	}
	static int arm(void *p, bool enable)
	{
		return enable ? arm_ntag_field({p, clear_edges, field, interrupt})
			      : interrupt(p, false);
	}
	static int field(void *p)
	{
		auto &t = *static_cast<Tag *>(p);
		return ++t.field_reads == t.fail_field_read ? -EIO : t.rf;
	}
	static int edge_count(void *p)
	{
		return static_cast<Tag *>(p)->edges;
	}
	static void wait(void *p, uint32_t duration)
	{
		static_cast<Tag *>(p)->clock += duration;
	}
	static uint64_t time(void *p)
	{
		return static_cast<Tag *>(p)->clock;
	}
	static int start(Tag &t, uint8_t address)
	{
		zassert_true(t.locked && t.armed);
		++t.frames;
		t.clock += t.frame_us;
		if (t.frames == t.fail_frame) {
			++t.edges;
		}
		if (address != t.address) {
			return -EIO;
		}
		/* Successful I2C addressing takes arbitration until explicit release.
		 * Do not simulate watchdog expiry: the host must clean up itself. */
		t.session[6] |= 0x40;
		return 0;
	}
	static int write(void *p, uint8_t address, const uint8_t *data, size_t size)
	{
		auto &t = *static_cast<Tag *>(p);
		const int rc = start(t, address);
		if (rc != 0) {
			return rc;
		}
		if (size == 1) {
			t.selector = data[0];
			t.register_read = false;
		} else if (size == 2) {
			zassert_equal(data[0], 0xfe);
			t.reg = data[1];
			t.register_read = true;
		} else if (size == 4) {
			zassert_equal(data[0], 0xfe);
			if (data[1] == 6 && data[2] == 0x40 && data[3] == 0) {
				++t.releases;
				if (t.release_error != 0) {
					return t.release_error;
				}
			}
			if (data[1] == 6 && data[2] == 4 && data[3] == 0) {
				++t.error_clears;
				if (t.fail_error_clear) {
					return -EIO;
				}
				if (t.drop_error_clear) {
					return 0;
				}
			}
			t.session[data[1]] = (t.session[data[1]] & ~data[2]) | (data[3] & data[2]);
		} else {
			zassert_equal(size, 17);
			zassert_true(t.clock >= t.done, "EEPROM command during programming");
			++t.writes;
			const bool failed = t.fail_program_writes != 0;
			if (failed) {
				--t.fail_program_writes;
				t.session[6] |= 4; /* Stays set until explicitly cleared. */
			}
			if (!t.drop_write && !failed) {
				memcpy(t.memory[data[0]], data + 1, 16);
			}
			if (data[0] == 0 && !t.drop_write && !failed) {
				t.address = data[1] >> 1;
				t.memory[0][0] = 4;
			}
			t.done = t.clock + (t.busy_forever ? 10000000 : 4000);
			t.programming_pending = true;
			if (t.ambiguous_write_error) {
				return -EIO; /* Programming began, but the transport reports
						failure. */
			}
		}
		return 0;
	}
	static int read(void *p, uint8_t address, uint8_t *data, size_t size)
	{
		auto &t = *static_cast<Tag *>(p);
		const int rc = start(t, address);
		if (rc != 0) {
			return rc;
		}
		if (t.register_read) {
			zassert_equal(size, 1);
			if (t.reg == 6 && t.programming_pending) {
				++t.status_polls;
				if (t.status_failure_forever || t.status_failures != 0) {
					if (t.status_failures != 0) {
						--t.status_failures;
					}
					return t.status_error;
				}
			}
			data[0] = t.reg == 6 ? (t.session[6] & ~2) | (t.clock < t.done ? 2 : 0)
					     : t.session[t.reg];
			if (t.reg == 6 && (data[0] & 2) == 0) {
				t.programming_pending = false;
			}
		} else {
			zassert_true(t.clock >= t.done, "EEPROM read during programming");
			zassert_equal(size, 16);
			if (t.selector == t.fail_read_block) {
				return -EIO;
			}
			memcpy(data, t.memory[t.selector], 16);
			if (t.selector == 0x39) {
				memset(data + 4, 0, 6);
			}
		}
		return 0;
	}
	NtagIo io()
	{
		return {this, lock, arm, field, edge_count, write, read, wait, time, 500};
	}
};

ZTEST(ntag, test_ndef_scheme_and_paths)
{
	uint8_t encoded[kNdefStorage];
	zassert_true(encode_ndef_uri("hTtPs://A.B/x", 13, encoded));
	/* Length excludes the scheme and includes the URI identifier. */
	zassert_equal(encoded[6], 4);
	zassert_mem_equal(encoded + 7, "A.B/x", 5);
	zassert_equal(encoded[1], 10);
	zassert_equal(encoded[4], 6);
	zassert_equal(encoded[12], 0xfe);
	zassert_true(encode_ndef_uri("HTTPS://A/%2F", 13, encoded));
	zassert_mem_equal(encoded + 7, "A/%2F", 5);
	memset(encoded, 0xaa, sizeof(encoded));
	zassert_false(encode_ndef_uri("bad", 3, encoded));
	zassert_equal(encoded[0], 0xaa);
	zassert_true(encode_ndef_uri("HTTP://A.B/X", 12, encoded));
	zassert_equal(encoded[6], 3);
	char decoded[kPayloadMax + 1];
	zassert_true(decode_ndef_uri(encoded, decoded));
	zassert_equal(strcmp(decoded, "HTTP://A.B/X"), 0);
	encoded[4] = 0xff;
	zassert_false(decode_ndef_uri(encoded, decoded));
	zassert_equal(strcmp(decoded, "HTTP://A.B/X"), 0);
}

static void provision(Tag &model, Ntag &tag, uint8_t uid[7])
{
	const uint8_t synthetic[] = {1, 2, 3, 4, 5, 6};
	zassert_ok(tag.initialize());
	zassert_ok(tag.begin_provision("HTTPS://A.B/X", 13, synthetic, uid));
	zassert_equal(tag.address(), 2);
	zassert_ok(tag.finish_provision(uid));
	zassert_equal(model.memory[0][10], 9);
}

ZTEST(ntag, test_provision_policy_and_sync)
{
	Tag model;
	Ntag tag(model.io());
	uint8_t uid[7];
	provision(model, tag, uid);
	zassert_ok(tag.verify_policy(uid));
	const unsigned previous = model.writes;
	zassert_ok(tag.synchronize("HTTPS://A.B/X", 13));
	zassert_equal(model.writes, previous); /* no EEPROM wear at every boot */
	zassert_ok(tag.synchronize("HTTPS://A.B/Y", 13));
	zassert_equal(model.writes, previous + 3);
	uint8_t expected[kNdefStorage];
	zassert_true(encode_ndef_uri("HTTPS://A.B/Y", 13, expected));
	zassert_mem_equal(model.memory[1], expected, sizeof(expected));
	char decoded[kPayloadMax + 1];
	zassert_ok(tag.read_url(decoded));
	zassert_equal(strcmp(decoded, "HTTPS://A.B/Y"), 0);
	zassert_equal(model.memory[0x3a][6], 0); /* never REG_LOCK */
}

ZTEST(ntag, test_busy_timeout_and_ack_without_programming)
{
	Tag model;
	Ntag tag(model.io());
	uint8_t uid[7];
	provision(model, tag, uid);
	model.drop_write = true;
	zassert_equal(tag.synchronize("HTTPS://A.B/Y", 13), -EIO);
	model.drop_write = false;
	model.busy_forever = true;
	zassert_equal(tag.synchronize("HTTPS://A.B/Y", 13), -ETIMEDOUT);
	const unsigned writes = model.writes;
	zassert_equal(tag.read_block(1, model.memory[1]), -EBUSY);
	zassert_equal(tag.synchronize("HTTPS://A.B/Y", 13), -EBUSY);
	zassert_equal(model.writes, writes);
}

ZTEST(ntag, test_unattempted_write_does_not_poison_busy_state)
{
	for (unsigned fault = 0; fault < 3; ++fault) {
		Tag model;
		model.clock = 100000; /* Any old last_observed timestamp is stale. */
		model.rf = fault == 0;
		model.pulse_on_enable = fault == 1;
		model.fail_field_read = fault == 2 ? 1 : 0;
		Ntag tag(model.io());
		uint8_t data[16]{};
		zassert_equal(tag.write_block(2, data), fault == 2 ? -EIO : -EAGAIN);
		zassert_equal(model.frames, 0);
		zassert_equal(model.writes, 0);
		zassert_equal(model.status_polls, 0);
		zassert_true(model.clock < 101000); /* No completion wait for a rejected frame. */
		zassert_false(model.locked || model.armed);
		model.rf = model.pulse_on_enable = false;
		zassert_ok(tag.read_block(0, data));
		zassert_ok(tag.write_block(2, data));
		zassert_equal(model.writes, 1);
	}
}

ZTEST(ntag, test_started_write_with_transport_error_remains_busy_until_observed)
{
	Tag model;
	model.ambiguous_write_error = model.busy_forever = true;
	Ntag tag(model.io());
	uint8_t data[16]{};
	zassert_equal(tag.write_block(2, data), -EIO);
	zassert_equal(model.writes, 1);
	zassert_true(model.status_polls > 0);
	zassert_equal(tag.read_block(0, data), -EBUSY);
	zassert_equal(tag.write_block(2, data), -EBUSY);
	zassert_equal(model.writes, 1);
}

ZTEST(ntag, test_transient_status_reads_retry_without_restarting_programming)
{
	constexpr int errors[] = {-EAGAIN, -EIO, -ENXIO, -ETIMEDOUT};
	for (int error : errors) {
		Tag model;
		auto io = model.io();
		io.fd_latency_us = 0;
		model.status_failures = 1;
		model.status_error = error;
		Ntag tag(io);
		uint8_t data[16]{};
		zassert_ok(tag.write_block(2, data));
		zassert_equal(model.status_polls, 2);
		zassert_equal(model.writes, 1);
		zassert_true(model.clock >= model.done);
		zassert_ok(tag.read_block(2, data));
	}
}

ZTEST(ntag, test_transient_status_lock_failure_retries_with_time_progress)
{
	constexpr int errors[] = {-EBUSY, -EAGAIN, -ETIMEDOUT};
	for (int error : errors) {
		Tag model;
		model.status_lock_failures = 1;
		model.status_lock_error = error;
		Ntag tag(model.io()); /* Default 500 us still fits an immediate lock retry. */
		uint8_t data[16]{};
		zassert_ok(tag.write_block(2, data));
		zassert_equal(model.status_lock_errors, 1);
		zassert_equal(model.status_polls, 1);
		zassert_equal(model.writes, 1);
	}
}

ZTEST(ntag, test_rf_affected_status_poll_clears_busy_but_requires_revalidation)
{
	Tag model;
	auto io = model.io();
	io.fd_latency_us = 0;
	model.fail_frame = 3; /* Write, status selector, then RF-affected status data. */
	Ntag tag(io);
	uint8_t data[16]{};
	zassert_equal(tag.write_block(2, data), -EAGAIN);
	zassert_equal(tag.rf_events(), 1);
	zassert_true(tag.tainted());
	zassert_equal(model.writes, 1);
	zassert_ok(tag.read_block(2, data)); /* Completion observed; not stuck EBUSY. */
}

ZTEST(ntag, test_persistent_status_failures_remain_bounded_and_block_eeprom)
{
	for (unsigned fault = 0; fault < 3; ++fault) {
		Tag model;
		auto io = model.io();
		io.fd_latency_us = 0;
		model.status_lock_forever = fault == 0;
		model.status_failure_forever = fault != 0;
		model.status_error = fault == 2 ? -ENODEV : -EIO;
		Ntag tag(io);
		uint8_t data[16]{};
		zassert_equal(tag.write_block(2, data), fault == 2 ? -ENODEV : -ETIMEDOUT);
		zassert_true(model.clock <= 5050);
		zassert_true(model.status_polls < 20);
		zassert_true(model.status_lock_errors < 30);
		zassert_equal(model.writes, 1);
		zassert_equal(tag.read_block(2, data), -EBUSY);
		zassert_equal(tag.write_block(2, data), -EBUSY);
		zassert_equal(model.writes, 1);
	}
}

ZTEST(ntag, test_rf_transition_revalidation_and_no_active_field_writes)
{
	Tag model;
	Ntag tag(model.io());
	uint8_t uid[7];
	provision(model, tag, uid);
	model.fail_frame = model.frames + 1;
	zassert_ok(tag.synchronize("HTTPS://A.B/Y", 13));
	zassert_false(tag.tainted());
	zassert_equal(tag.rf_events(), 1); /* hidden credentials still need RF proof */
	model.rf = true;
	const unsigned writes = model.writes;
	zassert_equal(tag.synchronize("HTTPS://A.B/X", 13), -EAGAIN);
	zassert_equal(model.writes, writes);
}

ZTEST(ntag, test_rf_lock_recovery_preserves_cc_and_auth)
{
	Tag model;
	Ntag tag(model.io());
	uint8_t uid[7];
	provision(model, tag, uid);
	model.memory[0][10] = 0xff;
	model.memory[0][11] = 0xff;
	model.memory[0x38][8] = model.memory[0x38][9] = model.memory[0x38][10] = 0xff;
	zassert_ok(tag.synchronize("HTTPS://A.B/Y", 13));
	zassert_equal(model.memory[0][10], 9);
	zassert_equal(model.memory[0][11], 0);
	zassert_equal(model.memory[0x38][8], 0);
	zassert_equal(model.memory[0x38][9], 0);
	zassert_equal(model.memory[0x38][10], 0);
	zassert_equal(model.memory[0x38][15], 4);
	zassert_equal(model.memory[0x39][12], 4);
}

ZTEST(ntag, test_reject_unprotected_and_irreversible_writes)
{
	Tag model;
	Ntag tag(model.io());
	zassert_ok(tag.initialize());
	zassert_equal(tag.synchronize("HTTPS://A.B/X", 13), -EACCES);
	zassert_equal(model.writes, 0);
	uint8_t configuration[16]{};
	configuration[6] = 3;
	zassert_equal(tag.write_block(0x3a, configuration), -EPERM);
	zassert_equal(model.writes, 0);
}

ZTEST(ntag, test_rejected_provisioning_releases_arbitration_without_programming)
{
	const uint8_t synthetic[] = {1, 2, 3, 4, 5, 6};
	for (unsigned reason = 0; reason < 4; ++reason) {
		Tag model;
		if (reason == 0) {
			model.memory[0][0] = 5;
		} else {
			model.memory[0x3a][6] = reason; /* Each REG_LOCK combination. */
		}
		model.session[6] = 0x10; /* Release preserves unrelated status. */
		uint8_t previous[sizeof(model.memory)], uid[7];
		memcpy(previous, model.memory, sizeof(previous));
		memset(uid, 0xaa, sizeof(uid));
		Ntag tag(model.io());
		zassert_equal(tag.begin_provision("HTTPS://A.B/X", 13, synthetic, uid), -EPERM);
		zassert_equal(model.session[6], 0x10);
		zassert_equal(model.releases, 1);
		zassert_equal(model.writes, 0);
		zassert_mem_equal(model.memory, previous, sizeof(previous));
		for (uint8_t byte : uid) {
			zassert_equal(byte, 0xaa);
		}
		zassert_false(model.locked || model.armed);
	}
}

ZTEST(ntag, test_preflight_read_failure_releases_arbitration)
{
	const uint8_t blocks[] = {0, 0x38, 0x39, 0x3a};
	const uint8_t synthetic[] = {1, 2, 3, 4, 5, 6};
	for (uint8_t block : blocks) {
		Tag model;
		model.fail_read_block = block;
		Ntag tag(model.io());
		uint8_t uid[7];
		zassert_equal(tag.begin_provision("HTTPS://A.B/X", 13, synthetic, uid), -EIO);
		zassert_equal(model.session[6] & 0x40, 0);
		zassert_equal(model.releases, 1);
		zassert_equal(model.writes, 0);
		zassert_false(model.locked || model.armed);
	}
}

ZTEST(ntag, test_failed_cleanup_preserves_the_original_preflight_error)
{
	const uint8_t synthetic[] = {1, 2, 3, 4, 5, 6};
	for (unsigned reason = 0; reason < 2; ++reason) {
		Tag model;
		model.memory[0x3a][6] = 1;
		model.fail_read_block = reason == 0 ? -1 : 0x39;
		model.release_error = -ETIMEDOUT;
		Ntag tag(model.io());
		uint8_t uid[7];
		zassert_equal(tag.begin_provision("HTTPS://A.B/X", 13, synthetic, uid),
			      reason == 0 ? -EPERM : -EIO);
		zassert_equal(model.session[6] & 0x40, 0x40);
		zassert_equal(model.releases, 1);
		zassert_equal(model.writes, 0);
		zassert_false(model.locked || model.armed);
	}
}

ZTEST(ntag, test_failed_fd_guard_revokes_previous_proof)
{
	Tag model;
	Ntag tag(model.io());
	uint8_t uid[7], data[16];
	provision(model, tag, uid);
	model.fail_disarm = true;
	zassert_equal(tag.read_block(1, data), -EIO);
	zassert_true(tag.tainted());
	zassert_equal(tag.rf_events(), 1);
	zassert_false(model.locked);
}

ZTEST(ntag, test_arm_preserves_completed_handoff_pulse)
{
	Tag model;
	model.edges = 7; /* A previous disarmed frame's stale count is cleared. */
	model.pulse_on_enable = true;
	Ntag tag(model.io());
	uint8_t data[16];
	zassert_equal(tag.read_block(0, data), -EAGAIN);
	zassert_equal(model.edges, 2); /* New edges survive even with field low. */
	zassert_equal(model.frames, 0);
	zassert_equal(model.writes, 0);
	zassert_false(model.locked || model.armed);
}

ZTEST(ntag, test_latched_program_error_is_reported_then_cleared_for_retry)
{
	Tag model;
	Ntag tag(model.io());
	uint8_t uid[7], data[16]{};
	provision(model, tag, uid);
	model.session[6] = 0x40; /* Clear must preserve I2C_LOCKED. */
	model.fail_program_writes = 1;
	zassert_equal(tag.write_block(2, data), -EIO);
	zassert_equal(model.session[6], 0x40);
	zassert_equal(model.error_clears, 1);
	zassert_ok(tag.write_block(2, data));
}

ZTEST(ntag, test_reconciliation_recovers_a_single_error_but_bounds_repeated_errors)
{
	constexpr unsigned attempts[] = {1, 10};
	for (unsigned failures : attempts) {
		Tag model;
		Ntag tag(model.io());
		uint8_t uid[7];
		provision(model, tag, uid);
		const unsigned previous = model.writes;
		model.fail_program_writes = failures;
		const int rc = tag.synchronize("HTTPS://A.B/Y", 13);
		zassert_equal(rc, failures == 1 ? 0 : -EIO);
		zassert_equal(model.error_clears, failures == 1 ? 1 : 3);
		zassert_equal(model.writes - previous, failures == 1 ? 4 : 3);
		if (rc == 0) {
			char url[kPayloadMax + 1];
			zassert_ok(tag.read_url(url));
			zassert_equal(strcmp(url, "HTTPS://A.B/Y"), 0);
		}
	}
}

ZTEST(ntag, test_failed_or_dropped_error_clear_blocks_further_programming)
{
	for (unsigned fault = 0; fault < 2; ++fault) {
		Tag model;
		Ntag tag(model.io());
		uint8_t uid[7], data[16]{};
		provision(model, tag, uid);
		model.fail_program_writes = 1;
		model.fail_error_clear = fault == 0;
		model.drop_error_clear = fault == 1;
		zassert_equal(tag.write_block(2, data), -EIO);
		const unsigned writes = model.writes;
		zassert_equal(tag.write_block(2, data), -EBUSY);
		zassert_equal(model.writes, writes);
		zassert_equal(model.session[6] & 4, 4);
	}
}

ZTEST(ntag, test_supported_fd_margins_complete_programming_with_frame_time)
{
	for (uint32_t margin = 0; margin <= kNtagMaxFdLatencyUs; margin += 25) {
		Tag model;
		model.frame_us = 100; /* Conservative status-frame time, not a measurement. */
		auto io = model.io();
		io.fd_latency_us = margin;
		Ntag tag(io);
		uint8_t uid[7];
		provision(model, tag, uid);
		zassert_ok(tag.synchronize("HTTPS://A.B/Y", 13));
	}
}

ZTEST(ntag, test_unsupported_fd_margins_are_rejected_before_any_io)
{
	constexpr uint32_t unsupported[] = {501, 2000, UINT32_MAX};
	for (uint32_t margin : unsupported) {
		Tag model;
		auto io = model.io();
		io.fd_latency_us = margin;
		Ntag tag(io);
		uint8_t data[16]{};
		zassert_equal(tag.initialize(), -ERANGE);
		zassert_equal(tag.read_block(0, data), -ERANGE);
		zassert_equal(tag.write_block(2, data), -ERANGE);
		zassert_equal(model.frames, 0);
		zassert_equal(model.writes, 0);
		zassert_false(model.armed || model.locked);
	}
}

ZTEST(ntag, test_arm_checks_field_across_enable_and_errors)
{
	for (unsigned fault = 0; fault < 5; ++fault) {
		Tag model;
		model.rf = fault == 0;
		model.field_on_enable = fault == 1;
		model.fail_enable = fault == 2;
		model.fail_field_read = fault >= 3 ? fault - 2 : 0;
		Ntag tag(model.io());
		uint8_t data[16];
		zassert_equal(tag.read_block(0, data), fault < 2 ? -EAGAIN : -EIO);
		zassert_equal(model.frames, 0);
		zassert_equal(model.writes, 0);
		zassert_false(model.locked || model.armed);
	}
}

ZTEST(ntag, test_fault_at_every_update_frame)
{
	Tag reference;
	Ntag baseline(reference.io());
	uint8_t uid[7];
	provision(reference, baseline, uid);
	const unsigned start = reference.frames;
	zassert_ok(baseline.synchronize("HTTPS://A.B/Y", 13));
	const unsigned frames = reference.frames - start;
	for (unsigned fault = 1; fault <= frames; ++fault) {
		Tag model;
		Ntag tag(model.io());
		provision(model, tag, uid);
		model.fail_frame = model.frames + fault;
		const int rc = tag.synchronize("HTTPS://A.B/Y", 13);
		zassert_false(model.locked);
		zassert_true(tag.rf_events() > 0);
		if (rc == 0) {
			char url[kPayloadMax + 1];
			zassert_ok(tag.read_url(url));
			zassert_equal(strcmp(url, "HTTPS://A.B/Y"), 0);
		}
		zassert_equal(model.memory[0x3a][6], 0);
	}
}

ZTEST_SUITE(ntag, nullptr, nullptr, nullptr, nullptr, nullptr);
