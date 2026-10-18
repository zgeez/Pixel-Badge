/* SPDX-License-Identifier: MIT */
#pragma once
#include "badge/settings.hpp"

namespace badge
{
constexpr uint8_t kBleVersion = 2;
constexpr size_t kBleApplySize = 6;
constexpr size_t kBleStatusSize = 12;
/* Protocol v1 wire value; target libc EINPROGRESS is not necessarily 115. */
constexpr int32_t kBlePendingResult = -115;
/* One short ATT response per page, so clients never need Read Blob. */
struct ArtworkReadCursor {
	uint32_t offset = 0;
	bool wide = false, metadata = false, sequential = false;
	bool select(const uint8_t *data, size_t length, uint32_t total)
	{
		if (length == 1 && (data[0] == 3 || data[0] == 4)) {
			offset = 0;
			wide = true;
			metadata = data[0] == 3;
			sequential = data[0] == 4;
			return true;
		}
		if (!((length == 3 && data[0] == 0) || (length == 5 && data[0] == 2))) {
			return false;
		}
		const bool next_wide = length == 5;
		uint32_t next = 0;
		for (unsigned i = 1; i < length; ++i) {
			next |= uint32_t(data[i]) << (8 * (i - 1));
		}
		if (next > total || (!next_wide && total > 65535)) {
			return false;
		}
		offset = next;
		wide = next_wide;
		metadata = sequential = false;
		return true;
	}
	size_t capacity(size_t response_bytes) const
	{
		const size_t header = wide ? 8 : 4;
		if (!wide) {
			return 16;
		}
		const size_t limit = response_bytes > 244 ? 244 : response_bytes;
		const size_t short_limit =
			sequential && limit == response_bytes && limit > 0 ? limit - 1 : limit;
		return short_limit > header ? short_limit - header : 0;
	}
	void complete(uint16_t att_offset, int result, size_t packet, size_t payload)
	{
		if (sequential && att_offset == 0 && result >= 0 &&
		    static_cast<size_t>(result) == packet) {
			offset += payload;
		}
	}
};
struct ConfigCommand {
	uint32_t generation = 0, id = 0;
	Field field = Field::kCount;
	uint32_t length = 0;
	uint8_t data[kTextBytes]{};
	Artwork artwork{};
	const uint8_t *value() const
	{
		return field == Field::kArtwork ? artwork.bytes : data;
	}
};
/* Caller serializes access. One staged field per Apply; no multi-field
 * transaction. A disconnect resets staging and invalidates old completions. */
class BleProtocol
{
public:
	using StageArtwork = int (*)(void *, uint32_t, uint32_t, const uint8_t *, size_t,
				     const uint8_t **);
	void artwork_staging(StageArtwork writer, void *context)
	{
		stage_art_ = writer;
		art_context_ = context;
	}
	void reset();
	int stage(Field field, const uint8_t *data, size_t length, bool authorized);
	int accept(const uint8_t *data, size_t length, bool authorized, ConfigCommand &output);
	bool current(const ConfigCommand &command) const;
	void complete(const ConfigCommand &command, int result);
	void status(uint8_t output[kBleStatusSize], uint8_t flags) const;
	bool busy() const
	{
		return pending_;
	}

private:
	uint32_t generation_ = 1, id_ = 0;
	Field field_ = Field::kCount;
	int result_ = 0;
	bool pending_ = false;
	uint8_t valid_ = 0;
	static constexpr size_t offsets_[8] = {0,
					       kPayloadMax,
					       kPayloadMax + kNameMax,
					       kPayloadMax + kNameMax + 1,
					       kPayloadMax + kNameMax + 2,
					       kPayloadMax + kNameMax + 2 + kTextBytes,
					       kPayloadMax + kNameMax + 2 + kTextBytes,
					       kPayloadMax + kNameMax + 2 + kTextBytes +
						       kMotionBytes};
	uint8_t staged_[kPayloadMax + kNameMax + 3 + kTextBytes + kMotionBytes]{};
	uint8_t *buffer(unsigned index)
	{
		return staged_ + offsets_[index];
	}
	uint32_t lengths_[8]{}, expected_[8]{};
	StageArtwork stage_art_ = nullptr;
	void *art_context_ = nullptr;
	const uint8_t *staged_art_ = nullptr;
};
} // namespace badge
