/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace badge
{
constexpr size_t kVoiceSamples = 160, kVoiceBytes = 84;
/* Independent 10 ms blocks: LE predictor, index=0, reserved=0, low nibble first.
 * All 160 samples are encoded after the predictor (sample zero is repeated). */
void encode_voice(const int16_t *samples, uint8_t output[kVoiceBytes]);
bool decode_voice(const uint8_t *input, size_t length, int16_t output[kVoiceSamples]);
} // namespace badge
