/* SPDX-License-Identifier: MIT */
#include "badge/ntag.hpp"
#include <errno.h>
#include <string.h>

namespace badge
{
namespace
{
constexpr uint8_t kBusy = 0x02;
constexpr uint8_t kWriteError = 0x04;
constexpr uint8_t kCc[] = {0xe1, 0x10, 0x6d, 0x00};
} // namespace

int arm_ntag_field(const NtagFieldIo &io)
{
	io.clear(io.context);
	const int before = io.field(io.context);
	if (before != 0) {
		return before < 0 ? before : -EAGAIN;
	}
	int rc = io.interrupt(io.context, true);
	if (rc == 0) {
		const int after = io.field(io.context);
		rc = after < 0 ? after : (after != 0 ? -EAGAIN : 0);
	}
	if (rc != 0) {
		(void)io.interrupt(io.context, false);
	}
	/* Never clear here: even a completed pulse during enable must remain
	 * visible to the pre-frame edge check. */
	return rc;
}

void erase_sensitive(void *buffer, size_t length)
{
	auto *bytes = static_cast<volatile uint8_t *>(buffer);
	while (length-- != 0) {
		*bytes++ = 0;
	}
}

int Ntag::frame(const uint8_t *tx, uint8_t *rx, size_t length)
{
	int rc = io_.arm(io_.context, true);
	if (rc != 0) {
		return rc;
	}
	const uint32_t guard = io_.fd_latency_us + 20 < 50 ? 50 : io_.fd_latency_us + 20;
	io_.wait_us(io_.context, guard);
	int field = io_.field(io_.context);
	int edges = io_.edges(io_.context);
	if (field < 0 || edges < 0) {
		rc = field < 0 ? field : edges;
	} else if (field != 0 || edges != 0) {
		rc = -EAGAIN;
	}
	const bool transferred = rc == 0;
	if (transferred) {
		rc = tx != nullptr ? io_.write(io_.context, address_, tx, length)
				   : io_.read(io_.context, address_, rx, length);
		last_observed_ = io_.now_us(io_.context);
		io_.wait_us(io_.context, guard);
	}
	const int disarm = io_.arm(io_.context, false);
	field = io_.field(io_.context);
	edges = io_.edges(io_.context);
	if (disarm != 0 || field < 0 || edges < 0) {
		if (transferred) {
			tainted_ = true;
			++rf_events_; /* an unreliable FD guard cannot retain RF proof */
		}
		if (rc == 0) {
			rc = disarm != 0 ? disarm : (field < 0 ? field : edges);
		}
	} else if (field != 0 || edges != 0) {
		if (transferred) {
			tainted_ = true;
			++rf_events_;
		}
		rc = -EAGAIN;
	}
	return rc;
}

int Ntag::read_bytes(const uint8_t *selector, size_t length, uint8_t *output, size_t size)
{
	int rc = io_.lock(io_.context, true);
	if (rc != 0) {
		return rc;
	}
	rc = frame(selector, nullptr, length);
	if (rc == 0) {
		rc = frame(nullptr, output, size);
	}
	const int unlocked = io_.lock(io_.context, false);
	return rc != 0 ? rc : unlocked;
}

int Ntag::read_block(uint8_t block, uint8_t output[16])
{
	if (busy_) {
		return -EBUSY;
	}
	return read_bytes(&block, 1, output, 16);
}

int Ntag::read_register(uint8_t reg, uint8_t &value)
{
	if (reg > 6) {
		return -EINVAL;
	}
	const uint8_t selector[] = {0xfe, reg};
	return read_bytes(selector, sizeof(selector), &value, 1);
}

int Ntag::write_register(uint8_t reg, uint8_t mask, uint8_t value)
{
	if (reg > 6) {
		return -EINVAL;
	}
	const uint8_t data[] = {0xfe, reg, mask, value};
	int rc = io_.lock(io_.context, true);
	if (rc != 0) {
		return rc;
	}
	rc = frame(data, nullptr, sizeof(data));
	const int unlocked = io_.lock(io_.context, false);
	return rc != 0 ? rc : unlocked;
}

int Ntag::wait_ready(uint64_t started)
{
	/* now_us is the hardware-backed cycle counter on Zephyr. The deadline
	 * begins at the write's STOP, before the post-frame RF guard. A status
	 * sampled before expiry may finish its RF guard after expiry. */
	while (io_.now_us(io_.context) - started < 5000) {
		uint8_t status = 0;
		const int rc = read_register(6, status);
		if (rc != 0) {
			return rc;
		}
		if (last_observed_ - started >= 5000) {
			return -ETIMEDOUT;
		}
		if ((status & kBusy) == 0) {
			busy_ = false;
			return (status & kWriteError) != 0 ? -EIO : 0;
		}
	}
	return -ETIMEDOUT;
}

int Ntag::write_block(uint8_t block, const uint8_t input[16], bool credentials)
{
	if (busy_) {
		return -EBUSY;
	}
	if (block > 0x3a || (block == 0x3a && (input[6] & 3) != 0)) {
		return -EPERM;
	}
	uint8_t data[17];
	data[0] = block;
	memcpy(data + 1, input, 16);
	int rc = io_.lock(io_.context, true);
	if (rc != 0) {
		erase_sensitive(data, sizeof(data));
		return rc;
	}
	/* Even an errored/contended write may have started programming. Do not
	 * permit another EEPROM command until completion has been observed. */
	rc = frame(data, nullptr, sizeof(data));
	const uint64_t started = last_observed_;
	busy_ = true;
	if (block == 0 && rc == 0) {
		address_ = input[0] >> 1;
	}
	erase_sensitive(data, sizeof(data));
	const int unlocked = io_.lock(io_.context, false);
	const int ready = wait_ready(started);
	if (rc != 0 || unlocked != 0 || ready != 0) {
		return rc != 0 ? rc : (unlocked != 0 ? unlocked : ready);
	}
	uint8_t verified[16];
	rc = read_block(block, verified);
	if (rc == 0) {
		for (unsigned i = 0; i < 16; ++i) {
			if (block == 0 && i == 0) {
				continue;
			} /* UID0 readback, not address */
			if (credentials && i >= 4 && i < 10) {
				continue;
			} /* masked PWD/PACK */
			if (verified[i] != input[i]) {
				rc = -EIO;
				break;
			}
		}
	}
	erase_sensitive(verified, sizeof(verified));
	return rc;
}

int Ntag::release()
{
	return write_register(6, 0x40, 0); /* release I2C_LOCKED explicitly */
}

int Ntag::initialize()
{
	/* Try only the documented factory and personalized addresses. Never
	 * scan the shared IMU bus for arbitrary targets. */
	const uint8_t addresses[] = {0x02, 0x55};
	for (uint8_t address : addresses) {
		address_ = address;
		busy_ = false;
		uint8_t status;
		int rc = read_register(6, status);
		if (rc == 0 && (status & kBusy) != 0) {
			busy_ = true;
			return -EBUSY;
		}
		if (rc == 0) {
			rc = write_register(0, 0xff, 0x01);
			uint8_t nc = 0;
			if (rc == 0) {
				rc = read_register(0, nc);
			}
			if (rc == 0 && nc != 0x01) {
				rc = -EIO;
			}
			if (rc == 0) {
				return release();
			}
			return rc;
		}
		if (rc == -EAGAIN) {
			return rc;
		}
	}
	return -ENODEV;
}

int Ntag::inspect(uint8_t uid[7], bool protected_policy)
{
	uint8_t metadata[16], auth[16], access[16], configuration[16];
	int rc = read_block(0, metadata);
	if (rc == 0) {
		rc = read_block(0x38, auth);
	}
	if (rc == 0) {
		rc = read_block(0x39, access);
	}
	if (rc == 0) {
		rc = read_block(0x3a, configuration);
	}
	if (rc != 0) {
		return rc;
	}
	if (metadata[0] != 0x04 || configuration[0] != 0x01 || (configuration[6] & ~3) != 0 ||
	    (protected_policy && (memcmp(metadata + 12, kCc, 4) != 0 || (metadata[10] & 9) != 9 ||
				  auth[15] != 4 || access[0] != 0 || access[12] != 4 ||
				  configuration[1] != 2 || configuration[5] != 0))) {
		return -EACCES;
	}
	memcpy(uid, metadata, 7);
	return 0;
}

int Ntag::verify_policy(uint8_t uid[7])
{
	const int rc = inspect(uid, true);
	const int unlocked = release();
	return rc != 0 ? rc : unlocked;
}

int Ntag::write_ndef(const uint8_t data[kNdefStorage])
{
	/* Publish TLV length only after the tail block verifies. This reduces
	 * exposure to mixed records; physical torn writes still need bench tests. */
	uint8_t first[16];
	memcpy(first, data, 16);
	first[1] = 0;
	int rc = write_block(1, first);
	if (rc == 0) {
		rc = write_block(2, data + 16);
	}
	if (rc == 0) {
		rc = write_block(1, data);
	}
	return rc;
}

int Ntag::read_url(char output[kPayloadMax + 1])
{
	uint8_t data[kNdefStorage];
	int rc = read_block(1, data);
	if (rc == 0) {
		rc = read_block(2, data + 16);
	}
	if (rc == 0 && !decode_ndef_uri(data, output)) {
		rc = -EBADMSG;
	}
	return rc;
}

int Ntag::synchronize(const char *url, size_t length)
{
	uint8_t expected[kNdefStorage], actual[kNdefStorage], uid[7];
	if (!encode_ndef_uri(url, length, expected)) {
		return -EINVAL;
	}
	int rc = -EIO;
	for (unsigned attempt = 0; attempt < 3; ++attempt) {
		/* Re-read all metadata and every bounded NDEF block after any
		 * failed/contended read before trusting the tag again. */
		rc = inspect(uid, true);
		if (rc == 0) {
			rc = read_block(1, actual);
		}
		if (rc == 0) {
			rc = read_block(2, actual + 16);
		}
		if (rc == 0) {
			tainted_ = false;
			/* RF lock bits outside AUTH0 can deny RF writes, but must not
			 * accumulate across MCU updates. Keep only the CC lock pair. */
			uint8_t metadata[16], auth[16];
			rc = read_block(0, metadata);
			if (rc == 0 && (metadata[10] != 9 || metadata[11] != 0)) {
				metadata[0] = 4;
				metadata[10] = 9;
				metadata[11] = 0;
				rc = write_block(0, metadata);
			}
			if (rc == 0) {
				rc = read_block(0x38, auth);
			}
			if (rc == 0 && (auth[8] != 0 || auth[9] != 0 || auth[10] != 0)) {
				auth[8] = auth[9] = auth[10] = 0;
				rc = write_block(0x38, auth);
			}
			if (rc == 0 && memcmp(actual, expected, sizeof(actual)) != 0) {
				rc = write_ndef(expected);
			}
		}
		if (rc == 0) {
			rc = read_block(1, actual);
		}
		if (rc == 0) {
			rc = read_block(2, actual + 16);
		}
		if (rc == 0 && memcmp(actual, expected, sizeof(actual)) != 0) {
			rc = -EIO;
		}
		if (rc == 0 || busy_ || rc == -EACCES) {
			break;
		}
	}
	const int unlocked = busy_ ? 0 : release();
	return rc != 0 ? rc : unlocked;
}

int Ntag::begin_provision(const char *url, size_t length, const uint8_t credentials[6],
			  uint8_t uid[7])
{
	uint8_t ndef[kNdefStorage], metadata[16], auth[16], access[16], configuration[16];
	if (!encode_ndef_uri(url, length, ndef) ||
	    (credentials[0] == 0xff && credentials[1] == 0xff && credentials[2] == 0xff &&
	     credentials[3] == 0xff)) {
		return -EINVAL;
	}
	int rc = read_block(0, metadata);
	if (rc == 0) {
		rc = read_block(0x38, auth);
	}
	if (rc == 0) {
		rc = read_block(0x39, access);
	}
	if (rc == 0) {
		rc = read_block(0x3a, configuration);
	}
	if (rc != 0) {
		return rc;
	}
	if (metadata[0] != 4 || (configuration[6] & 3) != 0) {
		return -EPERM;
	}
	memcpy(uid, metadata, 7);
	metadata[0] = 4; /* byte 04 -> 7-bit address 02 */
	metadata[10] = metadata[11] = 0;
	memcpy(metadata + 12, kCc, sizeof(kCc));
	rc = write_block(0, metadata);
	if (rc == 0) {
		rc = write_ndef(ndef);
	}
	configuration[0] = 1; /* plain field detect; no mirroring or pass-through */
	configuration[1] = 2;
	configuration[5] = 0; /* no clock stretching */
	if (rc == 0) {
		rc = write_block(0x3a, configuration);
	}
	access[0] = 0;  /* public reads, unlimited failed auth attempts */
	access[12] = 4; /* SRAM_PROT, unrestricted MCU access */
	memcpy(access + 4, credentials, 6);
	if (rc == 0) {
		rc = write_block(0x39, access, true);
	}
	erase_sensitive(access, sizeof(access));
	auth[8] = auth[9] = auth[10] = 0; /* clear dynamic RF locks */
	auth[15] = 4;                     /* activate AUTH0 only after credentials */
	if (rc == 0) {
		rc = write_block(0x38, auth);
	}
	const int unlocked = busy_ ? 0 : release();
	return rc != 0 ? rc : unlocked;
}

int Ntag::finish_provision(const uint8_t uid[7])
{
	uint8_t metadata[16];
	int rc = read_block(0, metadata);
	if (rc == 0 && memcmp(metadata, uid, 7) != 0) {
		rc = -EACCES;
	}
	if (rc == 0) {
		metadata[0] = 4;
		metadata[10] = 9; /* L_CC bit 3 and BL_CC bit 0; reversible via I2C */
		metadata[11] = 0;
		rc = write_block(0, metadata);
	}
	uint8_t verified_uid[7];
	if (rc == 0) {
		rc = inspect(verified_uid, true);
	}
	if (rc == 0 && memcmp(uid, verified_uid, 7) != 0) {
		rc = -EACCES;
	}
	const int unlocked = busy_ ? 0 : release();
	return rc != 0 ? rc : unlocked;
}
} // namespace badge
