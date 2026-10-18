/* SPDX-License-Identifier: MIT */
#pragma once
#include <psa/crypto.h>
#include <stddef.h>
#include <stdint.h>
namespace badge
{
bool audio_mac(const uint8_t key[32], const uint8_t *data, size_t length, uint8_t out[32]);
bool audio_enrollment(const uint8_t peer[65], const uint8_t nonce[16], uint8_t pub[65],
		      uint8_t key[32]);
bool audio_encrypt(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad,
		   size_t aad_length, const uint8_t *plain, size_t length, uint8_t *cipher);
bool audio_equal(const uint8_t *a, const uint8_t *b, size_t length);
bool audio_authenticate(const uint8_t key[32], const uint8_t nonce[16], const uint8_t challenge[16],
			const uint8_t *command, size_t length, uint32_t &counter, uint16_t &value);
} // namespace badge
