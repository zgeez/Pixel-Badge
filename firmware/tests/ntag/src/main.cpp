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
		if (t.frames == t.fail_frame) {
			++t.edges;
		}
		return address == t.address ? 0 : -EIO;
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
			t.session[data[1]] = (t.session[data[1]] & ~data[2]) | (data[3] & data[2]);
		} else {
			zassert_equal(size, 17);
			zassert_true(t.clock >= t.done, "EEPROM command during programming");
			++t.writes;
			if (!t.drop_write) {
				memcpy(t.memory[data[0]], data + 1, 16);
			}
			if (data[0] == 0 && !t.drop_write) {
				t.address = data[1] >> 1;
				t.memory[0][0] = 4;
			}
			t.done = t.clock + (t.busy_forever ? 10000000 : 4000);
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
			data[0] = t.reg == 6 ? (t.clock < t.done ? 2 : 0) : t.session[t.reg];
		} else {
			zassert_true(t.clock >= t.done, "EEPROM read during programming");
			zassert_equal(size, 16);
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
