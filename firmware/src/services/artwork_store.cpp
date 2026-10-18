/* SPDX-License-Identifier: MIT */
#include "badge/artwork_store.hpp"
#include <errno.h>
#include <string.h>
namespace badge
{
namespace
{
uint32_t get32(const uint8_t *p)
{
	return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
void put32(uint8_t *p, uint32_t value)
{
	for (unsigned i = 0; i < 4; i++) {
		p[i] = value >> (8 * i);
	}
}
} // namespace
uint32_t artwork_crc(const uint8_t *bytes, size_t length)
{
	uint32_t crc = 0xffffffff;
	for (size_t i = 0; i < length; i++) {
		crc ^= bytes[i];
		for (unsigned bit = 0; bit < 8; bit++) {
			crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1)));
		}
	}
	return ~crc;
}
const uint8_t *ArtworkStore::staged_data() const
{
	return staged_ >= 0 && expected_ != 0 && written_ == expected_ && tail_size_ == 0
		       ? io_.mapped + staged_ * kArtworkBankBytes
		       : nullptr;
}
int ArtworkStore::flush(bool final)
{
	const size_t aligned = final ? (tail_size_ + 3) & ~size_t(3) : tail_size_ & ~size_t(3);
	if (aligned == 0) {
		return 0;
	}
	if (aligned > tail_size_) {
		memset(tail_ + tail_size_, 0xff, aligned - tail_size_);
	}
	const size_t base = staged_ * kArtworkBankBytes;
	const size_t needed = (flushed_ + aligned + 4095) & ~size_t(4095);
	while (erased_ < needed) {
		const int rc = io_.erase(io_.context, base + erased_, 4096);
		if (rc != 0) {
			return rc;
		}
		erased_ += 4096;
	}
	const int rc = io_.write(io_.context, base + flushed_, tail_, aligned);
	if (rc != 0) {
		return rc;
	}
	if (memcmp(io_.mapped + base + flushed_, tail_, aligned) != 0) {
		return -EIO;
	}
	const size_t used = aligned > tail_size_ ? tail_size_ : aligned;
	tail_size_ -= used;
	memmove(tail_, tail_ + used, tail_size_);
	flushed_ += aligned;
	return 0;
}
int ArtworkStore::stage(uint32_t offset, uint32_t total, const uint8_t *bytes, size_t length)
{
	if (!io_.mapped || !io_.erase || !io_.write) {
		return -ENODEV;
	}
	if (!bytes || total < 4 || total > kArtworkBytes || length == 0 || length > 235 ||
	    offset > total || length > total - offset) {
		return -EINVAL;
	}
	if (offset == 0) {
		reset();
		staged_ = active_ == 0 ? 1 : 0;
		expected_ = total;
	}
	if (staged_ < 0 || total != expected_ || offset != written_) {
		return -EINVAL;
	}
	memcpy(tail_ + tail_size_, bytes, length);
	tail_size_ += length;
	written_ += length;
	const int rc = flush(written_ == total);
	if (rc != 0) {
		reset();
		return rc;
	}
	if (written_ == total && !valid_artwork(staged_data(), total)) {
		reset();
		return -EINVAL;
	}
	return 0;
}
int ArtworkStore::load(const uint8_t manifest[12], Artwork &art)
{
	const uint32_t length = get32(manifest + 4);
	if (!io_.mapped) {
		return -ENODEV;
	}
	if (manifest[0] != 3 || manifest[1] > 1 || manifest[2] != 0 || manifest[3] != 0 ||
	    length < 4 || length > kArtworkBytes) {
		return -EIO;
	}
	const auto *bytes = io_.mapped + manifest[1] * kArtworkBankBytes;
	if (artwork_crc(bytes, length) != get32(manifest + 8) || !valid_artwork(bytes, length)) {
		return -EIO;
	}
	active_ = manifest[1];
	reset();
	art = {bytes, length};
	return 0;
}
int ArtworkStore::prepare(const uint8_t *bytes, size_t length, uint8_t manifest[12])
{
	if (!io_.mapped) {
		return -ENODEV;
	}
	if (!valid_artwork(bytes, length)) {
		return -EINVAL;
	}
	if (staged_data() != bytes || expected_ != length) {
		// Non-streamed defaults/legacy callers use the same inactive-bank path.
		// Never overwrite a source which aliases the bank being staged.
		if (reinterpret_cast<uintptr_t>(bytes) >= reinterpret_cast<uintptr_t>(io_.mapped) &&
		    reinterpret_cast<uintptr_t>(bytes) <
			    reinterpret_cast<uintptr_t>(io_.mapped) + 2 * kArtworkBankBytes) {
			if (active_ >= 0 && bytes == io_.mapped + active_ * kArtworkBankBytes) {
				memset(manifest, 0, 12);
				manifest[0] = 3;
				manifest[1] = active_;
				put32(manifest + 4, length);
				put32(manifest + 8, artwork_crc(bytes, length));
				return 0;
			}
			return -EINVAL;
		}
		for (size_t offset = 0; offset < length;) {
			const size_t count = length - offset < 232 ? length - offset : 232;
			const int rc = stage(offset, length, bytes + offset, count);
			if (rc != 0) {
				return rc;
			}
			offset += count;
		}
	}
	memset(manifest, 0, 12);
	manifest[0] = 3;
	manifest[1] = staged_;
	put32(manifest + 4, length);
	put32(manifest + 8, artwork_crc(staged_data(), length));
	return 0;
}
} // namespace badge
