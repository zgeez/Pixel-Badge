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
struct ConfigCommand {
	uint32_t generation = 0, id = 0;
	Field field = Field::kCount;
	uint16_t length = 0;
	uint8_t data[kArtworkBytes]{};
};
/* Caller serializes access. One staged field per Apply; no multi-field
 * transaction. A disconnect resets staging and invalidates old completions. */
class BleProtocol
{
public:
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
	uint8_t valid_ = 0, name_total_ = 0, name_used_ = 0;
	uint8_t staged_[7][kArtworkBytes]{};
	uint16_t lengths_[7]{}, expected_[7]{};
};
} // namespace badge
