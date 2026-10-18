/* SPDX-License-Identifier: MIT */
#pragma once
#include "badge/settings.hpp"
#include <string.h>
struct OwnedArtwork {
	uint8_t bytes[badge::kLegacyArtworkBytes] = {1, 1, 150, 0};
	uint32_t length = 132;
	operator badge::Artwork() const
	{
		return {bytes, length};
	}
};
struct OwnedSettings: badge::Settings {
	uint8_t storage[badge::kLegacyArtworkBytes]{};
	OwnedSettings() : badge::Settings(badge::kDefaultSettings)
	{
		memcpy(storage, artwork.bytes, artwork.length);
		artwork.bytes = storage;
	}
};
inline uint8_t *mutable_art(badge::Settings &settings)
{
	return const_cast<uint8_t *>(settings.artwork.bytes);
}
