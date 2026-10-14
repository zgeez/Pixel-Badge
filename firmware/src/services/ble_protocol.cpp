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
	valid_ = name_total_ = name_used_ = 0;
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
	const unsigned index = field == kNfcSyncField ? 6 : static_cast<unsigned>(field) - 1;
	if (field == Field::kText || field == Field::kArtwork) {
		const size_t header = field == Field::kArtwork ? 5 : 4;
		const size_t maximum = field == Field::kArtwork ? kArtworkBytes : kTextBytes;
		if (length <= header || length > 20 || (field == Field::kArtwork && data[0] != 1)) {
			return -EINVAL;
		}
		const uint8_t *fragment = data + (header == 5 ? 1 : 0);
		const uint16_t offset = content_interval(fragment),
			       total = content_interval(fragment + 2);
		if (total == 0 || total > maximum || offset > total ||
		    length - header > static_cast<size_t>(total - offset)) {
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
		memcpy(staged_[index] + offset, data + header, length - header);
		lengths_[index] += length - header;
		if (lengths_[index] == total) {
			const bool valid = field == Field::kArtwork
						   ? valid_artwork(staged_[index], total)
						   : valid_text(staged_[index], total);
			if (!valid) {
				return -EINVAL;
			}
			valid_ |= 1U << index;
		}
		return 0;
	}
	if (field == Field::kName) {
		/* Two header bytes keep all writes within the default ATT payload. */
		if (length < 3 || length > 20 || data[1] == 0 || data[1] > kNameMax) {
			return -EINVAL;
		}
		if (data[0] == 0) {
			valid_ &= ~(1U << index);
			name_used_ = 0;
			name_total_ = data[1];
		}
		if (data[0] != name_used_ || data[1] != name_total_ ||
		    name_used_ + length - 2 > name_total_) {
			return -EINVAL;
		}
		for (size_t i = 2; i < length; ++i) {
			if (data[i] < 0x20 || data[i] > 0x7e) {
				return -EINVAL;
			}
		}
		memcpy(staged_[index] + name_used_, data + 2, length - 2);
		name_used_ += length - 2;
		if (name_used_ == name_total_) {
			lengths_[index] = name_total_;
			valid_ |= 1U << index;
		}
		return 0;
	}
	if ((field == Field::kUrl && !valid_url(reinterpret_cast<const char *>(data), length)) ||
	    (field == Field::kBrightness && (length != 1 || (data[0] != 16 && data[0] != 64 &&
							     data[0] != 128 && data[0] != 255))) ||
	    (field == Field::kDefaultMode && (length != 1 || data[0] > 3)) ||
	    (field == kNfcSyncField && (length != 1 || data[0] != 1))) {
		return -EINVAL;
	}
	memcpy(staged_[index], data, length);
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
	const unsigned index = data[5] == static_cast<unsigned>(kNfcSyncField) ? 6 : data[5] - 1;
	if ((valid_ & (1U << index)) == 0) {
		return -EINVAL;
	}
	output = {};
	output.generation = generation_;
	output.id = id;
	output.field = static_cast<Field>(data[5]);
	output.length = lengths_[index];
	memcpy(output.data, staged_[index], output.length);
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
