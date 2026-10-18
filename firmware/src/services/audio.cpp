/* SPDX-License-Identifier: MIT */
#include "badge/audio.hpp"
#include <string.h>

namespace badge
{
namespace
{
/* Rounded Q15 sin(2*pi*n/256) and Hann(n/255); generated mathematically. */
constexpr int16_t sine[kAudioFftSize] = {
	0,      804,    1608,   2410,   3212,   4011,   4808,   5602,   6393,   7179,   7962,
	8739,   9512,   10278,  11039,  11793,  12539,  13279,  14010,  14732,  15446,  16151,
	16846,  17530,  18204,  18868,  19519,  20159,  20787,  21403,  22005,  22594,  23170,
	23731,  24279,  24811,  25329,  25832,  26319,  26790,  27245,  27683,  28105,  28510,
	28898,  29268,  29621,  29956,  30273,  30571,  30852,  31113,  31356,  31580,  31785,
	31971,  32137,  32285,  32412,  32521,  32609,  32678,  32728,  32757,  32767,  32757,
	32728,  32678,  32609,  32521,  32412,  32285,  32137,  31971,  31785,  31580,  31356,
	31113,  30852,  30571,  30273,  29956,  29621,  29268,  28898,  28510,  28105,  27683,
	27245,  26790,  26319,  25832,  25329,  24811,  24279,  23731,  23170,  22594,  22005,
	21403,  20787,  20159,  19519,  18868,  18204,  17530,  16846,  16151,  15446,  14732,
	14010,  13279,  12539,  11793,  11039,  10278,  9512,   8739,   7962,   7179,   6393,
	5602,   4808,   4011,   3212,   2410,   1608,   804,    0,      -804,   -1608,  -2410,
	-3212,  -4011,  -4808,  -5602,  -6393,  -7179,  -7962,  -8739,  -9512,  -10278, -11039,
	-11793, -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868,
	-19519, -20159, -20787, -21403, -22005, -22594, -23170, -23731, -24279, -24811, -25329,
	-25832, -26319, -26790, -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
	-30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971, -32137, -32285, -32412,
	-32521, -32609, -32678, -32728, -32757, -32767, -32757, -32728, -32678, -32609, -32521,
	-32412, -32285, -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571, -30273,
	-29956, -29621, -29268, -28898, -28510, -28105, -27683, -27245, -26790, -26319, -25832,
	-25329, -24811, -24279, -23731, -23170, -22594, -22005, -21403, -20787, -20159, -19519,
	-18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279, -12539, -11793,
	-11039, -10278, -9512,  -8739,  -7962,  -7179,  -6393,  -5602,  -4808,  -4011,  -3212,
	-2410,  -1608,  -804,
};
constexpr int16_t window[kAudioFftSize] = {
	0,     5,     20,    45,    80,    124,   179,   243,   317,   401,   495,   598,   711,
	833,   965,   1106,  1257,  1416,  1585,  1763,  1949,  2145,  2349,  2561,  2782,  3011,
	3249,  3494,  3747,  4008,  4276,  4552,  4834,  5124,  5421,  5724,  6034,  6350,  6672,
	7000,  7334,  7673,  8018,  8367,  8722,  9081,  9444,  9812,  10184, 10559, 10938, 11321,
	11706, 12094, 12485, 12879, 13274, 13671, 14070, 14470, 14872, 15274, 15677, 16081, 16484,
	16888, 17291, 17694, 18096, 18497, 18897, 19295, 19691, 20085, 20477, 20867, 21254, 21638,
	22019, 22396, 22770, 23139, 23505, 23866, 24223, 24575, 24922, 25264, 25601, 25932, 26257,
	26576, 26889, 27195, 27495, 27789, 28075, 28354, 28626, 28891, 29148, 29397, 29638, 29871,
	30096, 30313, 30521, 30721, 30912, 31094, 31267, 31432, 31587, 31732, 31869, 31996, 32114,
	32222, 32320, 32409, 32488, 32557, 32617, 32666, 32706, 32736, 32756, 32766, 32766, 32756,
	32736, 32706, 32666, 32617, 32557, 32488, 32409, 32320, 32222, 32114, 31996, 31869, 31732,
	31587, 31432, 31267, 31094, 30912, 30721, 30521, 30313, 30096, 29871, 29638, 29397, 29148,
	28891, 28626, 28354, 28075, 27789, 27495, 27195, 26889, 26576, 26257, 25932, 25601, 25264,
	24922, 24575, 24223, 23866, 23505, 23139, 22770, 22396, 22019, 21638, 21254, 20867, 20477,
	20085, 19691, 19295, 18897, 18497, 18096, 17694, 17291, 16888, 16484, 16081, 15677, 15274,
	14872, 14470, 14070, 13671, 13274, 12879, 12485, 12094, 11706, 11321, 10938, 10559, 10184,
	9812,  9444,  9081,  8722,  8367,  8018,  7673,  7334,  7000,  6672,  6350,  6034,  5724,
	5421,  5124,  4834,  4552,  4276,  4008,  3747,  3494,  3249,  3011,  2782,  2561,  2349,
	2145,  1949,  1763,  1585,  1416,  1257,  1106,  965,   833,   711,   598,   495,   401,
	317,   243,   179,   124,   80,    45,    20,    5,     0,
};
/* Visual amplitude scale from -60 to 0 dBFS, not acoustic SPL calibration. */
constexpr uint32_t thresholds[32] = {
	33,   41,   52,   64,   80,    100,   125,   156,   195,   244,   305,
	381,  476,  594,  742,  927,   1159,  1448,  1809,  2261,  2825,  3530,
	4411, 5512, 6888, 8607, 10755, 13439, 16793, 20985, 26223, 32768,
};
uint32_t root(uint64_t value)
{
	uint64_t result = 0, bit = uint64_t(1) << 62;
	while (bit > value) {
		bit >>= 2;
	}
	while (bit != 0) {
		if (value >= result + bit) {
			value -= result + bit;
			result = (result >> 1) + bit;
		} else {
			result >>= 1;
		}
		bit >>= 2;
	}
	return static_cast<uint32_t>(result);
}
unsigned height(uint32_t magnitude)
{
	unsigned result = 0;
	while (result < 32 && magnitude >= thresholds[result]) {
		++result;
	}
	return result;
}
} // namespace
void clear_audio(void *memory, size_t bytes)
{
	auto *destination = static_cast<volatile uint8_t *>(memory);
	while (bytes-- != 0) {
		*destination++ = 0;
	}
}
void AudioAnalyzer::clear()
{
	clear_audio(real_, sizeof(real_));
	clear_audio(imaginary_, sizeof(imaginary_));
}
bool AudioAnalyzer::analyze(const int16_t *samples, size_t count, AudioAnalysis &output)
{
	output = AudioAnalysis{};
	clear();
	if (samples == nullptr || count < kAudioFftSize || count > kAudioBlockSamples) {
		return false;
	}
	int64_t sum = 0;
	uint64_t squares = 0;
	for (size_t i = 0; i < count; ++i) {
		const int32_t value = samples[i];
		sum += value;
		squares += static_cast<uint64_t>(int64_t(value) * value);
		const uint32_t magnitude = value < 0 ? -value : value;
		if (magnitude > output.peak) {
			output.peak = static_cast<uint16_t>(magnitude);
		}
		if (value == INT16_MIN || value == INT16_MAX) {
			++output.clipped;
		}
	}
	const int64_t mean = sum / static_cast<int64_t>(count);
	const uint64_t dc = static_cast<uint64_t>(mean * mean);
	output.rms = root(squares / count > dc ? squares / count - dc : 0);
	/* Use the newest complete FFT window. DC estimate from that same window
	 * avoids leakage when the preceding samples had a different offset. */
	int64_t fft_sum = 0;
	for (size_t i = count - kAudioFftSize; i < count; ++i) {
		fft_sum += samples[i];
	}
	const int32_t fft_mean = fft_sum / static_cast<int64_t>(kAudioFftSize);
	for (size_t i = 0; i < kAudioFftSize; ++i) {
		real_[i] = (int64_t(samples[count - kAudioFftSize + i] - fft_mean) * window[i]) /
			   32768;
	}
	for (size_t i = 1, reversed = 0; i < kAudioFftSize; ++i) {
		size_t bit = kAudioFftSize >> 1;
		while ((reversed & bit) != 0) {
			reversed ^= bit;
			bit >>= 1;
		}
		reversed ^= bit;
		if (i < reversed) {
			const int32_t value = real_[i];
			real_[i] = real_[reversed];
			real_[reversed] = value;
		}
	}
	for (size_t span = 2; span <= kAudioFftSize; span <<= 1) {
		for (size_t begin = 0; begin < kAudioFftSize; begin += span) {
			for (size_t i = 0; i < span / 2; ++i) {
				const size_t angle = i * kAudioFftSize / span;
				const int32_t cosine =
					sine[(angle + kAudioFftSize / 4) % kAudioFftSize];
				const int32_t negative_sine = -sine[angle];
				const size_t even = begin + i, odd = even + span / 2;
				const int32_t real = (int64_t(cosine) * real_[odd] -
						      int64_t(negative_sine) * imaginary_[odd]) /
						     32768;
				const int32_t imaginary = (int64_t(cosine) * imaginary_[odd] +
							   int64_t(negative_sine) * real_[odd]) /
							  32768;
				real_[odd] = (real_[even] - real) / 2;
				imaginary_[odd] = (imaginary_[even] - imaginary) / 2;
				real_[even] = (real_[even] + real) / 2;
				imaginary_[even] = (imaginary_[even] + imaginary) / 2;
			}
		}
	}
	constexpr size_t limits[kAudioBands + 1] = {1, 2, 4, 8, 16, 32, 64, 96, 129};
	for (size_t band = 0; band < kAudioBands; ++band) {
		for (size_t bin = limits[band]; bin < limits[band + 1]; ++bin) {
			const uint64_t power =
				static_cast<uint64_t>(int64_t(real_[bin]) * real_[bin] +
						      int64_t(imaginary_[bin]) * imaginary_[bin]);
			const uint32_t normalization = bin == kAudioFftSize / 2 ? 2 : 4;
			uint32_t magnitude = normalization * root(power);
			if (magnitude > 32768) {
				magnitude = 32768;
			}
			if (magnitude > output.bands[band]) {
				output.bands[band] = static_cast<uint16_t>(magnitude);
			}
		}
	}
	clear();
	return true;
}
void render_audio(Frame &output, const AudioAnalysis &analysis, bool spectrum)
{
	output.clear();
	if (spectrum) {
		for (size_t band = 0; band < kAudioBands; ++band) {
			const unsigned rows = height(analysis.bands[band]);
			for (unsigned y = 0; y < rows; ++y) {
				for (int x = 0; x < 3; ++x) {
					output.set(static_cast<int>(band) * 4 + x,
						   31 - static_cast<int>(y));
				}
			}
		}
	} else {
		const unsigned rows = height(analysis.rms);
		for (unsigned y = 0; y < rows; ++y) {
			for (int x = 4; x < 28; ++x) {
				output.set(x, 31 - static_cast<int>(y));
			}
		}
	}
	if (analysis.clipped != 0) {
		output.set(31, 0);
		output.set(31, 1);
	}
}
void render_recording_audio(Frame &output, const AudioAnalysis &analysis)
{
	output.clear();
	for (unsigned band = 0; band < kAudioBands; ++band) {
		const unsigned rows = height(analysis.bands[band]) * 28 / 32;
		for (unsigned y = 0; y < rows; ++y) {
			for (unsigned x = 0; x < 3; ++x) {
				output.set(band * 4 + x, 27 - y);
			}
		}
	}
	const unsigned columns = height(analysis.rms);
	for (unsigned x = 0; x < columns; ++x) {
		output.set(x, 30);
		output.set(x, 31);
	}
	// Fixed frame corners distinguish the active recording view even in silence.
	output.set(0, 0);
	output.set(31, 0);
	if (analysis.clipped) {
		output.set(31, 1);
	}
}
} // namespace badge
