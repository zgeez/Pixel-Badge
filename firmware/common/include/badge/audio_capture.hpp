/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
namespace badge
{
struct AudioCaptureProfile {
	uint32_t rate, min_clock, max_clock;
};
/* nRF52840 supports 1.28MHz / 80 = 16kHz. The DK keeps its measured
 * 32kHz capture path; recording packets are 16kHz in either case. */
constexpr AudioCaptureProfile audio_capture_profile(bool nrf52840)
{
	return nrf52840 ? AudioCaptureProfile{16000, 1280000, 1280000}
			: AudioCaptureProfile{32000, 1800000, 3250000};
}
} // namespace badge
