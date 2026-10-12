/* SPDX-License-Identifier: MIT */
#pragma once

#include "badge/ndef.hpp"

namespace badge
{
/* Arm only from a disarmed state. The edge latch must survive interrupt
 * enable; Ntag checks it after the pre-frame settling interval. */
struct NtagFieldIo {
	void *context;
	void (*clear)(void *);
	int (*field)(void *);
	int (*interrupt)(void *, bool);
};
int arm_ntag_field(const NtagFieldIo &io);

/* Transport supplies one START/STOP frame per read/write. Lock spans a
 * complete two-frame read. All callbacks are synchronous; no heap is used. */
struct NtagIo {
	void *context;
	int (*lock)(void *, bool);
	int (*arm)(void *, bool);
	int (*field)(void *);
	int (*edges)(void *);
	int (*write)(void *, uint8_t, const uint8_t *, size_t);
	int (*read)(void *, uint8_t, uint8_t *, size_t);
	void (*wait_us)(void *, uint32_t);
	uint64_t (*now_us)(void *);
	uint32_t fd_latency_us;
};

void erase_sensitive(void *buffer, size_t length);

class Ntag
{
public:
	explicit Ntag(const NtagIo &io) : io_(io)
	{
	}
	int initialize();
	int read_block(uint8_t block, uint8_t output[16]);
	int write_block(uint8_t block, const uint8_t input[16], bool credentials = false);
	int read_register(uint8_t reg, uint8_t &value);
	int write_register(uint8_t reg, uint8_t mask, uint8_t value);
	int verify_policy(uint8_t uid[7]);
	int read_url(char output[kPayloadMax + 1]);
	int synchronize(const char *url, size_t length);
	/* Explicit provisioning only. Caller invalidates its durable RF-proof
	 * receipt first, then obtains fresh-session RF proof before finish. */
	int begin_provision(const char *url, size_t length, const uint8_t credentials[6],
			    uint8_t uid[7]);
	int finish_provision(const uint8_t uid[7]);
	uint8_t address() const
	{
		return address_;
	}
	bool tainted() const
	{
		return tainted_;
	}
	uint32_t rf_events() const
	{
		return rf_events_;
	}

private:
	NtagIo io_;
	uint8_t address_ = 0x55;
	bool tainted_ = false;
	uint32_t rf_events_ = 0;
	bool busy_ = false;
	uint64_t last_observed_ = 0;
	int frame(const uint8_t *tx, uint8_t *rx, size_t length);
	int read_bytes(const uint8_t *selector, size_t length, uint8_t *output, size_t size);
	int wait_ready(uint64_t started);
	int release();
	int inspect(uint8_t uid[7], bool protected_policy);
	int write_ndef(const uint8_t data[kNdefStorage]);
};
} // namespace badge
