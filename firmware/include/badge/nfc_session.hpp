/* SPDX-License-Identifier: MIT */
#pragma once
#include "badge/ntag.hpp"

namespace badge
{
/* Single NFC worker owner. Readable policy/NDEF checks, not RF password proof.
 * Pin the first accepted UID for this boot; explicit setup can replace it. */
class NfcSession
{
public:
	int synchronize(Ntag &tag, const char *url, size_t length, char previous[kPayloadMax + 1]);
	void bind(const uint8_t uid[7]);
	const uint8_t *uid() const
	{
		return uid_;
	}

private:
	uint8_t uid_[7]{};
	bool bound_ = false;
};
} // namespace badge
