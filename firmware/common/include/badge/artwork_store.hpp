/* SPDX-License-Identifier: MIT */
#pragma once
#include "badge/content.hpp"
namespace badge
{
constexpr size_t kArtworkBankBytes = 0x21000;
struct ArtworkFlash {
	void *context;
	const uint8_t *mapped;
	int (*erase)(void *, size_t, size_t);
	int (*write)(void *, size_t, const void *, size_t);
};
/* Serialized by the platform owner. Staging never changes the active bank.
 * All published views remain immutable until a later successful commit. */
class ArtworkStore
{
public:
	void initialize(ArtworkFlash io)
	{
		io_ = io;
		active_ = -1;
		reset();
	}
	void reset()
	{
		expected_ = written_ = flushed_ = erased_ = tail_size_ = 0;
		staged_ = -1;
	}
	int stage(uint32_t offset, uint32_t total, const uint8_t *bytes, size_t length);
	int load(const uint8_t manifest[12], Artwork &art);
	int prepare(const uint8_t *bytes, size_t length, uint8_t manifest[12]);
	void committed(const uint8_t manifest[12])
	{
		active_ = manifest[1];
		reset();
	}
	const uint8_t *staged_data() const;

private:
	int flush(bool final);
	ArtworkFlash io_{};
	int active_ = -1, staged_ = -1;
	uint32_t expected_ = 0, written_ = 0, flushed_ = 0, erased_ = 0;
	alignas(4) uint8_t tail_[248]{};
	size_t tail_size_ = 0;
};
uint32_t artwork_crc(const uint8_t *bytes, size_t length);
} // namespace badge
