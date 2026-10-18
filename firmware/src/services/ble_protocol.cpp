/* SPDX-License-Identifier: MIT */
#include "badge/ble_protocol.hpp"
#include <errno.h>
#include <string.h>

namespace badge
{
void BleProtocol::reset()
{
	if (++generation_ == 0) {
		++generation_;
	}
	id_ = 0;
	field_ = Field::kCount;
	result_ = 0;
	pending_ = false;
	valid_ = 0;
	staged_art_ = nullptr;
	memset(staged_, 0, sizeof(staged_));
	memset(lengths_, 0, sizeof(lengths_));
	memset(expected_, 0, sizeof(expected_));
}

int BleProtocol::stage(Field field, const uint8_t *data, size_t length, bool authorized)
{
	if (!authorized) {
		return -EACCES;
	}
	if (pending_) {
		return -EBUSY;
	}
	if (field <= Field::kSchema || (field >= Field::kCount && field != kNfcSyncField) ||
	    data == nullptr || length == 0) {
		return -EINVAL;
	}
	const unsigned index = field == kNfcSyncField ? 7 : static_cast<unsigned>(field) - 1;
	if (field == Field::kArtwork) {
		const bool wide = data[0] == 2;
		const size_t header = wide ? 9 : 5;
		if (!stage_art_ || length <= header || length > (wide ? 244 : 20) ||
		    (!wide && data[0] != 1)) {
			return -EINVAL;
		}
		auto get32 = [](const uint8_t *p) {
			return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
			       uint32_t(p[3]) << 24;
		};
		const uint32_t offset = wide ? get32(data + 1) : content_interval(data + 1);
		const uint32_t total = wide ? get32(data + 5) : content_interval(data + 3);
		if (total < 4 || total > kArtworkBytes || offset > total ||
		    length - header > total - offset) {
			return -EINVAL;
		}
		if (offset == 0) {
			valid_ &= ~(1U << index);
			lengths_[index] = 0;
			expected_[index] = total;
			staged_art_ = nullptr;
		}
		if (offset != lengths_[index] || total != expected_[index]) {
			return -EINVAL;
		}
		const uint8_t *view = nullptr;
		const int rc = stage_art_(art_context_, offset, total, data + header,
					  length - header, &view);
		if (rc != 0) {
			valid_ &= ~(1U << index);
			lengths_[index] = expected_[index] = 0;
			staged_art_ = nullptr;
			return rc;
		}
		lengths_[index] += length - header;
		if (lengths_[index] == total) {
			if (!valid_artwork(view, total)) {
				return -EINVAL;
			}
			staged_art_ = view;
			valid_ |= 1U << index;
		}
		return 0;
	}
	if (field == Field::kText) {
		if (length <= 4 || length > 20) {
			return -EINVAL;
		}
		const uint16_t offset = content_interval(data), total = content_interval(data + 2);
		if (total == 0 || total > kTextBytes || offset > total ||
		    length - 4 > static_cast<size_t>(total - offset)) {
			return -EINVAL;
		}
		if (offset == 0) {
			valid_ &= ~(1U << index);
			lengths_[index] = 0;
			expected_[index] = total;
		}
		if (offset != lengths_[index] || total != expected_[index]) {
			return -EINVAL;
		}
		memcpy(buffer(index) + offset, data + 4, length - 4);
		lengths_[index] += length - 4;
		if (lengths_[index] == total) {
			if (!valid_text(buffer(index), total)) {
				return -EINVAL;
			}
			valid_ |= 1U << index;
		}
		return 0;
	}
	if (field == Field::kName || (field == Field::kUrl && data[0] < 32)) {
		/* URL fragments use the same offset/total envelope as names. Raw
		 * HTTP(S) writes remain compatible; no valid URL begins with a control byte. */
		const size_t maximum = field == Field::kName ? kNameMax : kPayloadMax;
		if (length < 3 || length > 20 || data[1] == 0 || data[1] > maximum) {
			return -EINVAL;
		}
		if (data[0] == 0) {
			valid_ &= ~(1U << index);
			lengths_[index] = 0;
			expected_[index] = data[1];
		}
		if (data[0] != lengths_[index] || data[1] != expected_[index] ||
		    lengths_[index] + length - 2 > expected_[index]) {
			return -EINVAL;
		}
		for (size_t i = 2; i < length; ++i) {
			if (data[i] < 0x20 || data[i] > 0x7e) {
				return -EINVAL;
			}
		}
		memcpy(buffer(index) + lengths_[index], data + 2, length - 2);
		lengths_[index] += length - 2;
		if (lengths_[index] == expected_[index]) {
			if (field == Field::kUrl &&
			    !valid_url(reinterpret_cast<const char *>(buffer(index)),
				       lengths_[index])) {
				return -EINVAL;
			}
			valid_ |= 1U << index;
		}
		return 0;
	}
	if ((field == Field::kUrl && !valid_url(reinterpret_cast<const char *>(data), length)) ||
	    (field == Field::kBrightness &&
	     (length != 1 || (data[0] != 13 && data[0] != 16 && data[0] != 64 && data[0] != 128 &&
			      data[0] != 255))) ||
	    (field == Field::kDefaultMode &&
	     (length != 1 || data[0] > static_cast<uint8_t>(Mode::kFluid))) ||
	    (field == Field::kMotion && !valid_motion(data, length)) ||
	    (field == kNfcSyncField && (length != 1 || data[0] != 1))) {
		return -EINVAL;
	}
	memcpy(buffer(index), data, length);
	lengths_[index] = length;
	valid_ |= 1U << index;
	return 0;
}

int BleProtocol::accept(const uint8_t *data, size_t length, bool authorized, ConfigCommand &output)
{
	if (!authorized) {
		return -EACCES;
	}
	if (pending_) {
		return -EBUSY;
	}
	if (data == nullptr || length != kBleApplySize || data[0] != kBleVersion || data[5] == 0 ||
	    (data[5] >= static_cast<unsigned>(Field::kCount) &&
	     data[5] != static_cast<unsigned>(kNfcSyncField))) {
		return -EINVAL;
	}
	const uint32_t id = uint32_t(data[1]) | (uint32_t(data[2]) << 8) |
			    (uint32_t(data[3]) << 16) | (uint32_t(data[4]) << 24);
	if (id == 0 || id <= id_) {
		return -EALREADY;
	}
	const unsigned index = data[5] == static_cast<unsigned>(kNfcSyncField) ? 7 : data[5] - 1;
	if ((valid_ & (1U << index)) == 0) {
		return -EINVAL;
	}
	output = {};
	output.generation = generation_;
	output.id = id;
	output.field = static_cast<Field>(data[5]);
	output.length = lengths_[index];
	if (output.field == Field::kArtwork) {
		output.artwork = {staged_art_, output.length};
	} else {
		memcpy(output.data, buffer(index), output.length);
	}
	id_ = id;
	field_ = output.field;
	result_ = kBlePendingResult;
	pending_ = true;
	valid_ &= ~(1U << index);
	return 0;
}

bool BleProtocol::current(const ConfigCommand &command) const
{
	return pending_ && command.generation == generation_ && command.id == id_;
}
void BleProtocol::complete(const ConfigCommand &command, int result)
{
	if (current(command)) {
		pending_ = false;
		result_ = result;
	}
}
void BleProtocol::status(uint8_t output[kBleStatusSize], uint8_t flags) const
{
	output[0] = kBleVersion;
	for (unsigned i = 0; i < 4; ++i) {
		output[1 + i] = id_ >> (i * 8);
		output[7 + i] = static_cast<uint32_t>(result_) >> (i * 8);
	}
	output[5] = id_ == 0 ? 0 : static_cast<uint8_t>(field_);
	output[6] = id_ == 0 ? 0 : (pending_ ? 1 : 2);
	output[11] = flags;
}
} // namespace badge
