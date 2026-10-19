/* SPDX-License-Identifier: MIT */
#include "badge/audio_crypto.hpp"
#include "badge/audio.hpp"
#include <zephyr/ztest.h>
#include <string.h>
#include <zephyr/sys/byteorder.h>
using namespace badge;
ZTEST(audio_crypto, test_hmac_reference_and_constant_time_equality)
{
	zassert_equal(psa_crypto_init(), PSA_SUCCESS);
	uint8_t key[32]{}, mac[32]{};
	// RFC4231 case1: 20 bytes 0x0b (zero padding is HMAC equivalent).
	memset(key, 0x0b, 20);
	constexpr uint8_t expected[] = {0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53,
					0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
					0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7,
					0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7};
	zassert_true(audio_mac(key, reinterpret_cast<const uint8_t *>("Hi There"), 8, mac));
	zassert_mem_equal(mac, expected, 32);
	zassert_true(audio_equal(mac, expected, 32));
	mac[31] ^= 1;
	zassert_false(audio_equal(mac, expected, 32));
	clear_audio(key, 32);
	clear_audio(mac, 32);
}
ZTEST(audio_crypto, test_aes256_gcm_reference)
{
	zassert_equal(psa_crypto_init(), PSA_SUCCESS);
	uint8_t key[32]{}, nonce[12]{}, plain[16]{}, encrypted[32]{};
	constexpr uint8_t expected[] = {0xce, 0xa7, 0x40, 0x3d, 0x4d, 0x60, 0x6b, 0x6e,
					0x07, 0x4e, 0xc5, 0xd3, 0xba, 0xf3, 0x9d, 0x18,
					0xd0, 0xd1, 0xc8, 0xa7, 0x99, 0x99, 0x6b, 0xf0,
					0x26, 0x5b, 0x98, 0xb5, 0xd4, 0x8a, 0xb9, 0x19};
	zassert_true(audio_encrypt(key, nonce, nullptr, 0, plain, 16, encrypted));
	zassert_mem_equal(encrypted, expected, 32);
	nonce[0] = 1;
	uint8_t other[32]{};
	zassert_true(audio_encrypt(key, nonce, nullptr, 0, plain, 16, other));
	zassert_false(audio_equal(encrypted, other, 32));
}
ZTEST(audio_crypto, test_enrollment_invalid_point_erases_output)
{
	zassert_equal(psa_crypto_init(), PSA_SUCCESS);
	uint8_t bad[65]{}, nonce[16]{}, pub[65], key[32];
	memset(pub, 0xaa, 65);
	memset(key, 0xaa, 32);
	zassert_false(audio_enrollment(bad, nonce, pub, key));
	for (uint8_t byte : pub) {
		zassert_equal(byte, 0);
	}
	for (uint8_t byte : key) {
		zassert_equal(byte, 0);
	}
}
ZTEST_SUITE(audio_crypto, nullptr, nullptr, nullptr, nullptr, nullptr);

ZTEST(audio_crypto, test_enrollment_derives_same_key_on_peer)
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
	psa_set_key_bits(&attributes, 256);
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DERIVE);
	psa_set_key_algorithm(&attributes, PSA_ALG_ECDH);
	psa_key_id_t peer = 0;
	uint8_t phone[65]{}, badge[65]{}, nonce[16]{}, key[32]{}, secret[32]{};
	uint8_t salt[32]{}, prk[32]{}, info[135]{}, expected[32]{};
	size_t size = 0;
	zassert_equal(psa_crypto_init(), PSA_SUCCESS);
	zassert_equal(psa_generate_key(&attributes, &peer), PSA_SUCCESS);
	zassert_equal(psa_export_public_key(peer, phone, 65, &size), PSA_SUCCESS);
	zassert_equal(size, 65);
	nonce[0] = 23;
	zassert_true(audio_enrollment(phone, nonce, badge, key));
	zassert_equal(psa_raw_key_agreement(PSA_ALG_ECDH, peer, badge, 65, secret, 32, &size),
		      PSA_SUCCESS);
	zassert_equal(size, 32);
	memcpy(salt, nonce, 16);
	memcpy(info, "PB3E", 4);
	memcpy(info + 4, phone, 65);
	memcpy(info + 69, badge, 65);
	info[134] = 1;
	zassert_true(audio_mac(salt, secret, 32, prk));
	zassert_true(audio_mac(prk, info, sizeof(info), expected));
	zassert_mem_equal(key, expected, 32);
	psa_destroy_key(peer);
	clear_audio(key, 32);
	clear_audio(secret, 32);
	clear_audio(prk, 32);
	clear_audio(expected, 32);
}

ZTEST(audio_crypto, test_controls_reject_replay_and_cross_connection)
{
	uint8_t key[32]{}, nonce[16]{}, challenge[16]{}, packet[23]{}, message[43]{}, digest[32]{};
	packet[0] = 3;
	sys_put_le32(1, packet + 1);
	sys_put_le16(60, packet + 5);
	memcpy(message, "PB3C", 4);
	memcpy(message + 4, nonce, 16);
	memcpy(message + 20, packet, 7);
	zassert_true(audio_mac(key, message, 27, digest));
	memcpy(packet + 7, digest, 16);
	uint32_t counter = 0;
	uint16_t value = 0;
	zassert_true(audio_authenticate(key, nonce, challenge, packet, 23, counter, value));
	zassert_equal(counter, 1);
	zassert_equal(value, 60);
	zassert_false(audio_authenticate(key, nonce, challenge, packet, 23, counter, value));
	counter = 0;
	nonce[0] = 1;
	zassert_false(audio_authenticate(key, nonce, challenge, packet, 23, counter, value));
	zassert_equal(counter, 0);
	nonce[0] = 0;
	packet[0] = 4;
	zassert_false(audio_authenticate(key, nonce, challenge, packet, 23, counter, value));
	packet[0] = 5;
	memcpy(message + 20, packet, 7);
	memcpy(message + 27, challenge, 16);
	zassert_true(audio_mac(key, message, 43, digest));
	memcpy(packet + 7, digest, 16);
	challenge[15] = 1;
	zassert_false(audio_authenticate(key, nonce, challenge, packet, 23, counter, value));
	challenge[15] = 0;
	zassert_true(audio_authenticate(key, nonce, challenge, packet, 23, counter, value));
}
