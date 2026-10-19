/* SPDX-License-Identifier: MIT */
#pragma once
#include "badge/test_pattern.hpp"

namespace badge
{
constexpr size_t kAudioFftSize = 256;
constexpr size_t kAudioBlockSamples = 640;
constexpr size_t kAudioBands = 8;
struct AudioAnalysis {
	uint32_t rms = 0;
	uint16_t peak = 0;
	uint16_t clipped = 0;
	uint16_t bands[kAudioBands]{};
};
/* Hardware-independent, bounded Q15 Hann-windowed radix-2 FFT.
 * Input is mono signed PCM16; no audio is retained after analyze(). */
class AudioAnalyzer
{
public:
	bool analyze(const int16_t *samples, size_t count, AudioAnalysis &output);
	void clear();

private:
	int32_t real_[kAudioFftSize]{};
	int32_t imaginary_[kAudioFftSize]{};
};
enum class AudioOverlay {
	kNone,
	kUnavailable,
	kRecording
};
/* Pending, active and draining capture always outrank transient status text. */
constexpr AudioOverlay audio_overlay_kind(bool recording, bool requested, bool closing,
					  bool unavailable)
{
	return recording || requested || closing ? AudioOverlay::kRecording
	       : unavailable                     ? AudioOverlay::kUnavailable
						 : AudioOverlay::kNone;
}
void clear_audio(void *memory, size_t bytes);
void render_recording_audio(Frame &output, const AudioAnalysis &analysis);
void render_audio(Frame &output, const AudioAnalysis &analysis, bool spectrum);
} // namespace badge
