/* SPDX-License-Identifier: MIT */
#include "badge/audio.hpp"
#include "badge/audio_capture.hpp"
#include <zephyr/ztest.h>
#include <string.h>
using namespace badge;
/* Independent PCM reference fixture: round(12000*sin(2*pi*n/256)). */
constexpr int16_t tone[kAudioFftSize] = {
	0,      294,    589,    883,    1176,   1469,   1761,   2052,   2341,   2629,   2916,
	3201,   3483,   3764,   4043,   4319,   4592,   4863,   5131,   5395,   5657,   5915,
	6169,   6420,   6667,   6910,   7148,   7383,   7613,   7838,   8059,   8274,   8485,
	8691,   8891,   9087,   9276,   9460,   9638,   9811,   9978,   10138,  10293,  10441,
	10583,  10719,  10848,  10971,  11087,  11196,  11299,  11394,  11483,  11565,  11640,
	11708,  11769,  11823,  11870,  11910,  11942,  11967,  11986,  11996,  12000,  11996,
	11986,  11967,  11942,  11910,  11870,  11823,  11769,  11708,  11640,  11565,  11483,
	11394,  11299,  11196,  11087,  10971,  10848,  10719,  10583,  10441,  10293,  10138,
	9978,   9811,   9638,   9460,   9276,   9087,   8891,   8691,   8485,   8274,   8059,
	7838,   7613,   7383,   7148,   6910,   6667,   6420,   6169,   5915,   5657,   5395,
	5131,   4863,   4592,   4319,   4043,   3764,   3483,   3201,   2916,   2629,   2341,
	2052,   1761,   1469,   1176,   883,    589,    294,    0,      -294,   -589,   -883,
	-1176,  -1469,  -1761,  -2052,  -2341,  -2629,  -2916,  -3201,  -3483,  -3764,  -4043,
	-4319,  -4592,  -4863,  -5131,  -5395,  -5657,  -5915,  -6169,  -6420,  -6667,  -6910,
	-7148,  -7383,  -7613,  -7838,  -8059,  -8274,  -8485,  -8691,  -8891,  -9087,  -9276,
	-9460,  -9638,  -9811,  -9978,  -10138, -10293, -10441, -10583, -10719, -10848, -10971,
	-11087, -11196, -11299, -11394, -11483, -11565, -11640, -11708, -11769, -11823, -11870,
	-11910, -11942, -11967, -11986, -11996, -12000, -11996, -11986, -11967, -11942, -11910,
	-11870, -11823, -11769, -11708, -11640, -11565, -11483, -11394, -11299, -11196, -11087,
	-10971, -10848, -10719, -10583, -10441, -10293, -10138, -9978,  -9811,  -9638,  -9460,
	-9276,  -9087,  -8891,  -8691,  -8485,  -8274,  -8059,  -7838,  -7613,  -7383,  -7148,
	-6910,  -6667,  -6420,  -6169,  -5915,  -5657,  -5395,  -5131,  -4863,  -4592,  -4319,
	-4043,  -3764,  -3483,  -3201,  -2916,  -2629,  -2341,  -2052,  -1761,  -1469,  -1176,
	-883,   -589,   -294,
};
ZTEST(audio, test_silence_dc_and_invalid_blocks)
{
	AudioAnalyzer analyzer;
	int16_t samples[kAudioBlockSamples]{};
	AudioAnalysis analysis;
	zassert_true(analyzer.analyze(samples, kAudioBlockSamples, analysis));
	zassert_equal(analysis.rms, 0);
	for (int16_t &value : samples) {
		value = 12000;
	}
	zassert_true(analyzer.analyze(samples, kAudioBlockSamples, analysis));
	zassert_equal(analysis.rms, 0);
	for (uint16_t value : analysis.bands) {
		zassert_equal(value, 0);
	}
	Frame frame;
	render_audio(frame, analysis, true);
	for (uint8_t value : frame.bytes) {
		zassert_equal(value, 0);
	}
	zassert_false(analyzer.analyze(nullptr, kAudioBlockSamples, analysis));
	zassert_false(analyzer.analyze(samples, kAudioFftSize - 1, analysis));
	zassert_false(analyzer.analyze(samples, kAudioBlockSamples + 1, analysis));
}
ZTEST(audio, test_tone_frequency_amplitude_and_clipping)
{
	AudioAnalyzer analyzer;
	int16_t samples[kAudioFftSize];
	AudioAnalysis analysis;
	constexpr unsigned bins[] = {3U, 12U, 24U, 80U};
	for (unsigned bin : bins) {
		for (size_t i = 0; i < kAudioFftSize; ++i) {
			samples[i] = tone[bin * i % kAudioFftSize];
		}
		zassert_true(analyzer.analyze(samples, kAudioFftSize, analysis));
		zassert_within(analysis.rms, 8485, 10);
		const unsigned expected = bin == 3 ? 1 : bin == 12 ? 3 : bin == 24 ? 4 : 6;
		for (size_t band = 0; band < kAudioBands; ++band) {
			if (band != expected) {
				zassert_true(analysis.bands[expected] > analysis.bands[band]);
			}
		}
		zassert_within(analysis.bands[expected], 12000, 200);
		zassert_equal(analysis.clipped, 0);
	}
	for (size_t i = 0; i < kAudioFftSize; ++i) {
		samples[i] = i % 2 == 0 ? -12000 : 12000;
	}
	zassert_true(analyzer.analyze(samples, kAudioFftSize, analysis));
	zassert_within(analysis.bands[kAudioBands - 1], 12000, 200);
	for (size_t i = 0; i < kAudioFftSize; ++i) {
		samples[i] = i % 2 == 0 ? INT16_MIN : INT16_MAX;
	}
	zassert_true(analyzer.analyze(samples, kAudioFftSize, analysis));
	zassert_equal(analysis.clipped, kAudioFftSize);
	zassert_equal(analysis.peak, 32768);
	zassert_within(analysis.rms, 32767, 1);
	Frame frame;
	render_audio(frame, analysis, true);
	zassert_true(frame.get(31, 0));
	zassert_true(frame.get(31, 1));
}
ZTEST(audio, test_newest_window_no_stale_spectrum_and_explicit_clear)
{
	AudioAnalyzer analyzer;
	int16_t samples[kAudioBlockSamples];
	for (size_t i = 0; i < kAudioBlockSamples; ++i) {
		samples[i] = i < kAudioBlockSamples - kAudioFftSize ? 32767 : 5000;
	}
	AudioAnalysis analysis;
	zassert_true(analyzer.analyze(samples, kAudioBlockSamples, analysis));
	for (uint16_t value : analysis.bands) {
		zassert_equal(value, 0);
	}
	analyzer.clear();
	memset(samples, 0, sizeof(samples));
	zassert_true(analyzer.analyze(samples, kAudioBlockSamples, analysis));
	for (uint16_t value : analysis.bands) {
		zassert_equal(value, 0);
	}
	uint8_t buffer[64];
	memset(buffer, 0xa5, sizeof(buffer));
	clear_audio(buffer, sizeof(buffer));
	for (uint8_t value : buffer) {
		zassert_equal(value, 0);
	}
}
ZTEST(audio, test_recording_overlay_overrides_stale_unavailable_message)
{
	// A failed long press left NO REC visible; a request arrives before it expires.
	zassert_equal(audio_overlay_kind(false, false, false, true), AudioOverlay::kUnavailable);
	zassert_equal(audio_overlay_kind(false, true, false, true), AudioOverlay::kRecording);
	zassert_equal(audio_overlay_kind(true, false, false, true), AudioOverlay::kRecording);
	zassert_equal(audio_overlay_kind(false, false, true, true), AudioOverlay::kRecording);
	// Expiring the transient message never changes the pending/active REC frame.
	zassert_equal(audio_overlay_kind(false, true, false, false), AudioOverlay::kRecording);
	zassert_equal(audio_overlay_kind(true, false, false, false), AudioOverlay::kRecording);
	zassert_equal(audio_overlay_kind(false, false, false, false), AudioOverlay::kNone);
}
ZTEST(audio, test_recording_uses_full_spectrum_and_level_view)
{
	AudioAnalysis analysis;
	for (auto &band : analysis.bands) {
		band = 32768;
	}
	analysis.rms = 32768;
	Frame frame;
	render_recording_audio(frame, analysis);
	for (unsigned band = 0; band < 8; band++) {
		zassert_true(frame.get(band * 4, 0));
		zassert_true(frame.get(band * 4, 27));
	}
	for (int x = 0; x < 32; x++) {
		zassert_true(frame.get(x, 30));
		zassert_true(frame.get(x, 31));
	}
	render_recording_audio(frame, AudioAnalysis{});
	zassert_true(frame.get(0, 0));
	zassert_true(frame.get(31, 0));
	zassert_false(frame.get(4, 14));
	zassert_false(frame.get(4, 31));
}
ZTEST(audio, test_rev_a_capture_profile_and_native_voice_block)
{
	constexpr auto reva = audio_capture_profile(true);
	constexpr auto prototype = audio_capture_profile(false);
	zassert_equal(reva.rate, 16000);
	zassert_equal(reva.min_clock, 1280000);
	zassert_equal(reva.max_clock, reva.min_clock);
	zassert_equal(reva.min_clock / 80, reva.rate);
	zassert_equal(prototype.rate, 32000);
	zassert_equal(prototype.min_clock, 1800000);
	// One Rev A 20ms block feeds the FFT and exactly two 160-sample packets.
	int16_t samples[320];
	for (size_t i = 0; i < 320; ++i) {
		samples[i] = i % 16 < 8 ? 10000 : -10000;
	}
	AudioAnalyzer analyzer;
	AudioAnalysis analysis;
	zassert_true(analyzer.analyze(samples, 320, analysis));
	zassert_equal(analysis.rms, 10000);
	// A 1kHz tone at16kHz falls in FFT bin16, in band4.
	zassert_true(analysis.bands[4] > analysis.bands[2]);
}
ZTEST_SUITE(audio, nullptr, nullptr, nullptr, nullptr, nullptr);
