/* SPDX-License-Identifier: MIT */
#include "badge/adpcm.hpp"
#include "badge/recording.hpp"
#include <zephyr/ztest.h>
#include <string.h>
using namespace badge;
ZTEST(voice, test_silence_independent_blocks)
{
	int16_t pcm[kVoiceSamples]{}, decoded[kVoiceSamples]{};
	uint8_t packet[kVoiceBytes];
	encode_voice(pcm, packet);
	for (uint8_t byte : packet) {
		zassert_equal(byte, 0);
	}
	zassert_true(decode_voice(packet, sizeof(packet), decoded));
	zassert_mem_equal(pcm, decoded, sizeof(pcm));
	for (unsigned i = 0; i < kVoiceSamples; ++i) {
		pcm[i] = i % 2 ? -32768 : 32767;
	}
	encode_voice(pcm, packet);
	zassert_true(decode_voice(packet, sizeof(packet), decoded));
	memset(pcm, 0, sizeof(pcm));
	encode_voice(pcm, packet);
	for (uint8_t byte : packet) {
		zassert_equal(byte, 0); // No codec history crosses blocks.
	}
}
ZTEST(voice, test_reference_nibble_order_and_invalid_header)
{
	uint8_t packet[kVoiceBytes]{};
	int16_t pcm[kVoiceSamples]{};
	// IMA reference steps: nibble7 -> predictor11,index8; nibble15 -> -19,index16.
	packet[4] = 0xf7;
	zassert_true(decode_voice(packet, sizeof(packet), pcm));
	zassert_equal(pcm[0], 11);
	zassert_equal(pcm[1], -19);
	packet[2] = 89;
	zassert_false(decode_voice(packet, sizeof(packet), pcm));
	packet[2] = 0;
	packet[3] = 1;
	zassert_false(decode_voice(packet, sizeof(packet), pcm));
	zassert_false(decode_voice(packet, sizeof(packet) - 1, pcm));
	zassert_false(decode_voice(nullptr, sizeof(packet), pcm));
}
namespace
{
constexpr int16_t sine16[16] = {0, 77,  141,  185,  200,  185,  141,  77,
				0, -77, -141, -185, -200, -185, -141, -77};
uint64_t power(const int16_t *samples)
{
	uint64_t result = 0;
	for (size_t i = 0; i < kRecordingSamples; ++i) {
		result += static_cast<int64_t>(samples[i]) * samples[i];
	}
	return result;
}
} // namespace
ZTEST(voice, test_recording_quiet_audio_gain_and_reset)
{
	RecordingProcessor processor;
	int16_t samples[kRecordingSamples], first[kRecordingSamples];
	for (size_t i = 0; i < kRecordingSamples; ++i) {
		samples[i] = sine16[i % 16];
	}
	const uint64_t original = power(samples);
	zassert_true(processor.process(samples, kRecordingSamples));
	memcpy(first, samples, sizeof(first));
	zassert_true(power(samples) > original * 16);
	for (int block = 0; block < 150; ++block) {
		for (size_t i = 0; i < kRecordingSamples; ++i) {
			samples[i] = sine16[i % 16];
		}
		zassert_true(processor.process(samples, kRecordingSamples));
	}
	zassert_true(power(samples) > original * 100);
	processor.clear();
	for (size_t i = 0; i < kRecordingSamples; ++i) {
		samples[i] = sine16[i % 16];
	}
	zassert_true(processor.process(samples, kRecordingSamples));
	zassert_mem_equal(first, samples, sizeof(first));
}
ZTEST(voice, test_recording_dc_silence_and_ambient_not_gated)
{
	RecordingProcessor processor;
	int16_t samples[kRecordingSamples];
	for (int block = 0; block < 20; ++block) {
		for (auto &sample : samples) {
			sample = 12000;
		}
		zassert_true(processor.process(samples, kRecordingSamples));
	}
	zassert_equal(power(samples), 0);
	processor.clear();
	memset(samples, 0, sizeof(samples));
	zassert_true(processor.process(samples, kRecordingSamples));
	zassert_equal(power(samples), 0);
	for (size_t i = 0; i < kRecordingSamples; ++i) {
		samples[i] = i % 2 ? -2 : 2;
	}
	zassert_true(processor.process(samples, kRecordingSamples));
	zassert_true(power(samples) > 0); // Near-silence is not muted.
}
ZTEST(voice, test_recording_limiter_and_invalid_input)
{
	RecordingProcessor processor;
	int16_t samples[kRecordingSamples];
	zassert_false(processor.process(nullptr, kRecordingSamples));
	zassert_false(processor.process(samples, kRecordingSamples - 1));
	for (int block = 0; block < 20; ++block) {
		for (size_t i = 0; i < kRecordingSamples; ++i) {
			samples[i] = i % 2 ? -32768 : 32767;
		}
		zassert_true(processor.process(samples, kRecordingSamples));
		for (auto sample : samples) {
			zassert_within(sample, 0, 30000);
		}
	}
}
ZTEST_SUITE(voice, nullptr, nullptr, nullptr, nullptr, nullptr);
