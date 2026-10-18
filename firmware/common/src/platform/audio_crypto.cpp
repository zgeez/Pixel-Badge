/* SPDX-License-Identifier: MIT */
#include "badge/audio_crypto.hpp"
#include "badge/audio.hpp"
#include <string.h>
#include <zephyr/sys/byteorder.h>
namespace badge
{
bool audio_authenticate(const uint8_t key[32], const uint8_t nonce[16], const uint8_t challenge[16],
			const uint8_t *command, size_t length, uint32_t &counter, uint16_t &value)
{
	if (!command || length != 23 || command[0] < 2 || command[0] > 7 ||
	    sys_get_le32(command + 1) <= counter) {
		return false;
	}
	uint8_t message[43]{}, mac[32]{};
	memcpy(message, "PB3C", 4);
	memcpy(message + 4, nonce, 16);
	memcpy(message + 20, command, 7);
	const size_t size = command[0] == 5 ? 43 : 27;
	if (command[0] == 5) {
		memcpy(message + 27, challenge, 16);
	}
	const bool valid = audio_mac(key, message, size, mac) && audio_equal(mac, command + 7, 16);
	clear_audio(message, sizeof(message));
	clear_audio(mac, sizeof(mac));
	if (valid) {
		counter = sys_get_le32(command + 1);
		value = sys_get_le16(command + 5);
	}
	return valid;
}
bool audio_equal(const uint8_t *a, const uint8_t *b, size_t length)
{
	uint8_t difference = 0;
	for (size_t i = 0; i < length; ++i) {
		difference |= a[i] ^ b[i];
	}
	return difference == 0;
}
bool audio_mac(const uint8_t key[32], const uint8_t *data, size_t length, uint8_t out[32])
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
	psa_set_key_bits(&attributes, 256);
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
	psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
	psa_key_id_t id = 0;
	size_t size = 0;
	bool ok = psa_import_key(&attributes, key, 32, &id) == PSA_SUCCESS;
	if (ok) {
		ok = psa_mac_compute(id, PSA_ALG_HMAC(PSA_ALG_SHA_256), data, length, out, 32,
				     &size) == PSA_SUCCESS &&
		     size == 32;
	}
	psa_destroy_key(id);
	psa_reset_key_attributes(&attributes);
	if (!ok) {
		clear_audio(out, 32);
	}
	return ok;
}
bool audio_enrollment(const uint8_t peer[65], const uint8_t nonce[16], uint8_t pub[65],
		      uint8_t key[32])
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
	psa_set_key_bits(&attributes, 256);
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DERIVE);
	psa_set_key_algorithm(&attributes, PSA_ALG_ECDH);
	psa_key_id_t id = 0;
	size_t size = 0;
	uint8_t secret[32]{}, salt[32]{}, prk[32]{}, info[135]{};
	bool ok = psa_generate_key(&attributes, &id) == PSA_SUCCESS;
	if (ok) {
		ok = psa_export_public_key(id, pub, 65, &size) == PSA_SUCCESS && size == 65;
	}
	if (ok) {
		ok = psa_raw_key_agreement(PSA_ALG_ECDH, id, peer, 65, secret, 32, &size) ==
			     PSA_SUCCESS &&
		     size == 32;
	}
	/* RFC5869 SHA256: extract with the 16-byte connection nonce as salt.
	 * Zero padding the HMAC key to 32 bytes is equivalent for SHA256. */
	memcpy(salt, nonce, 16);
	memcpy(info, "PB3E", 4);
	memcpy(info + 4, peer, 65);
	memcpy(info + 69, pub, 65);
	info[134] = 1;
	if (ok) {
		ok = audio_mac(salt, secret, 32, prk) && audio_mac(prk, info, sizeof(info), key);
	}
	psa_destroy_key(id);
	psa_reset_key_attributes(&attributes);
	clear_audio(secret, 32);
	clear_audio(salt, 32);
	clear_audio(prk, 32);
	clear_audio(info, sizeof(info));
	if (!ok) {
		clear_audio(key, 32);
		clear_audio(pub, 65);
	}
	return ok;
}
bool audio_encrypt(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad,
		   size_t aad_length, const uint8_t *plain, size_t length, uint8_t *cipher)
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&attributes, 256);
	psa_set_key_algorithm(&attributes, PSA_ALG_GCM);
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
	psa_key_id_t id = 0;
	size_t size = 0;
	bool ok = psa_import_key(&attributes, key, 32, &id) == PSA_SUCCESS;
	if (ok) {
		ok = psa_aead_encrypt(id, PSA_ALG_GCM, nonce, 12, aad, aad_length, plain, length,
				      cipher, length + 16, &size) == PSA_SUCCESS &&
		     size == length + 16;
	}
	psa_destroy_key(id);
	psa_reset_key_attributes(&attributes);
	if (!ok) {
		clear_audio(cipher, length + 16);
	}
	return ok;
}
} // namespace badge
