/* SPDX-License-Identifier: MIT */
#include "badge/adpcm.hpp"
namespace badge
{
namespace
{
constexpr int steps[] = {
	7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,   21,    23,
	25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,   73,    80,
	88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,  253,   279,
	307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,  876,   963,
	1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749, 3024,  3327,
	3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487,
	12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
constexpr int changes[] = {-1, -1, -1, -1, 2, 4, 6, 8};
int clamp(int value, int low, int high)
{
	return value < low ? low : value > high ? high : value;
}
int advance(int nibble, int &predictor, int &index)
{
	const int step = steps[index];
	int difference = step >> 3;
	if (nibble & 4) {
		difference += step;
	}
	if (nibble & 2) {
		difference += step >> 1;
	}
	if (nibble & 1) {
		difference += step >> 2;
	}
	predictor = clamp(predictor + ((nibble & 8) ? -difference : difference), -32768, 32767);
	index = clamp(index + changes[nibble & 7], 0, 88);
	return predictor;
}
} // namespace
void encode_voice(const int16_t *samples, uint8_t output[kVoiceBytes])
{
	int predictor = samples[0], index = 0;
	output[0] = predictor;
	output[1] = predictor >> 8;
	output[2] = output[3] = 0;
	for (size_t i = 0; i < kVoiceSamples; ++i) {
		int difference = samples[i] - predictor, nibble = difference < 0 ? 8 : 0;
		if (difference < 0) {
			difference = -difference;
		}
		int step = steps[index];
		if (difference >= step) {
			nibble |= 4;
			difference -= step;
		}
		if (difference >= (step >> 1)) {
			nibble |= 2;
			difference -= step >> 1;
		}
		if (difference >= (step >> 2)) {
			nibble |= 1;
		}
		advance(nibble, predictor, index);
		if ((i & 1) == 0) {
			output[4 + i / 2] = nibble;
		} else {
			output[4 + i / 2] |= nibble << 4;
		}
	}
}
bool decode_voice(const uint8_t *input, size_t length, int16_t output[kVoiceSamples])
{
	if (!input || length != kVoiceBytes || input[2] > 88 || input[3] != 0) {
		return false;
	}
	int predictor = static_cast<int16_t>(input[0] | input[1] << 8), index = input[2];
	for (size_t i = 0; i < kVoiceSamples; ++i) {
		output[i] = advance((input[4 + i / 2] >> ((i & 1) * 4)) & 15, predictor, index);
	}
	return true;
}
} // namespace badge
