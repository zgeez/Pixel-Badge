/* SPDX-License-Identifier: MIT */
#include "badge/recording.hpp"
#include "badge/audio.hpp"
#include <limits.h>
namespace badge
{
namespace
{
constexpr int32_t kOne = 4096, kMinimumGain = kOne / 4, kMaximumGain = 16 * kOne;
constexpr int32_t kTargetRms = 2600, kNoiseFloor = 24, kLimit = 30000;
/* exp(-2*pi*30/16000) in Q15: a gentle first-order DC/rumble filter. */
constexpr int32_t kHighPass = 32384;
uint32_t root(uint64_t value)
{
	uint64_t result = 0, bit = uint64_t(1) << 62;
	while (bit > value) {
		bit >>= 2;
	}
	while (bit) {
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
int32_t clamp(int64_t value, int32_t low, int32_t high)
{
	return value < low ? low : value > high ? high : static_cast<int32_t>(value);
}
} // namespace
void RecordingProcessor::clear()
{
	clear_audio(filtered_, sizeof(filtered_));
	previous_input_ = previous_output_q14_ = 0;
	gain_q12_ = 8 * kOne;
	primed_ = false;
}
bool RecordingProcessor::process(int16_t *samples, size_t count)
{
	if (!samples || count != kRecordingSamples) {
		return false;
	}
	uint64_t power = 0;
	uint32_t peak = 0;
	for (size_t i = 0; i < count; ++i) {
		const int32_t input = samples[i];
		if (!primed_) {
			previous_input_ = input;
			primed_ = true;
		}
		const int64_t state =
			static_cast<int64_t>(input - previous_input_) * 16384 +
			static_cast<int64_t>(previous_output_q14_) * kHighPass / 32768;
		previous_input_ = input;
		previous_output_q14_ = clamp(state, INT32_MIN, INT32_MAX);
		const int32_t value = previous_output_q14_ / 16384;
		filtered_[i] = value;
		power += static_cast<uint64_t>(static_cast<int64_t>(value) * value);
		const uint32_t magnitude = value < 0 ? -value : value;
		if (magnitude > peak) {
			peak = magnitude;
		}
	}
	const uint32_t rms = root(power / count);
	int32_t target = gain_q12_;
	/* Freeze gain in near-silence instead of winding up the noise floor.
	 * Samples still pass through; there is no gate or voice-only suppression. */
	if (rms >= kNoiseFloor) {
		target = clamp(static_cast<int64_t>(kTargetRms) * kOne / rms, kMinimumGain,
			       kMaximumGain);
	}
	const int32_t delta = target - gain_q12_;
	const int32_t next_gain = gain_q12_ + delta / (delta < 0 ? 2 : 50);
	const int32_t ceiling =
		peak ? static_cast<int32_t>(static_cast<int64_t>(kLimit) * kOne / peak)
		     : kMaximumGain;
	for (size_t i = 0; i < count; ++i) {
		int32_t gain = gain_q12_ + static_cast<int64_t>(next_gain - gain_q12_) *
						   static_cast<int32_t>(i + 1) /
						   static_cast<int64_t>(count);
		if (gain > ceiling) {
			gain = ceiling;
		}
		const int64_t value = static_cast<int64_t>(filtered_[i]) * gain / kOne;
		samples[i] = static_cast<int16_t>(clamp(value, -kLimit, kLimit));
	}
	gain_q12_ = next_gain;
	clear_audio(filtered_, sizeof(filtered_));
	return true;
}
} // namespace badge
