/* SPDX-License-Identifier: MIT */
#pragma once
#include "badge/adpcm.hpp"
namespace badge
{
constexpr size_t kRecordingSamples = 2 * kVoiceSamples;
/* Fixed-size 16 kHz recording processor. No noise gate or retained audio. */
class RecordingProcessor
{
public:
	bool process(int16_t *samples, size_t count);
	void clear();

private:
	int32_t filtered_[kRecordingSamples]{};
	int32_t previous_input_ = 0, previous_output_q14_ = 0;
	int32_t gain_q12_ = 8 * 4096;
	bool primed_ = false;
};
} // namespace badge
