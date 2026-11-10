/**
 * @file
 * @brief Unit tests for openshot::AudioWaveformer
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2022 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"
#include "AudioWaveformer.h"
#include "Clip.h"
#include "Exceptions.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "Frame.h"
#include "Timeline.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <thread>
#include <sstream>


using namespace openshot;

namespace {
std::shared_ptr<Frame> MakeWaveformFrame(int frame_number, int sample_rate, const std::vector<float>& samples)
{
	auto frame = std::make_shared<Frame>(frame_number, static_cast<int>(samples.size()), 1);
	frame->SampleRate(sample_rate);
	frame->ChannelsLayout(LAYOUT_MONO);
	frame->AddAudio(true, 0, 0, samples.data(), static_cast<int>(samples.size()), 1.0f);
	return frame;
}

// Write a stereo PCM WAV, where each channel's samples come from a generator(channel, sample_index)
template <typename Generator>
void WriteStereoWav(const std::string& path, int sample_rate, double seconds, Generator generator)
{
	std::remove(path.c_str());
	FFmpegWriter writer(path);
	writer.SetAudioOptions(true, "pcm_s16le", sample_rate, 2, LAYOUT_STEREO, sample_rate * 2 * 16);
	writer.Open();

	const int64_t total = static_cast<int64_t>(std::llround(seconds * sample_rate));
	const int chunk = sample_rate / 10;
	int frame_number = 1;
	for (int64_t start = 0; start < total; start += chunk) {
		const int count = static_cast<int>(std::min<int64_t>(chunk, total - start));
		auto frame = std::make_shared<Frame>(frame_number++, count, 2);
		frame->SampleRate(sample_rate);
		frame->ChannelsLayout(LAYOUT_STEREO);
		for (int channel = 0; channel < 2; channel++) {
			std::vector<float> samples(count);
			for (int i = 0; i < count; i++) {
				samples[i] = generator(channel, start + i);
			}
			frame->AddAudio(true, channel, 0, samples.data(), count, 1.0f);
		}
		writer.WriteFrame(frame);
	}
	writer.Close();
}

int ArgMax(const std::vector<float>& values)
{
	return static_cast<int>(std::distance(values.begin(), std::max_element(values.begin(), values.end())));
}

Keyframe LinearKeyframe(std::initializer_list<std::pair<double, double>> points)
{
	Keyframe keyframe;
	for (const auto& point : points) {
		keyframe.AddPoint(point.first, point.second, LINEAR);
	}
	return keyframe;
}
}

TEST_CASE( "Extract waveform data piano.wav", "[libopenshot][audiowaveformer]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";
	FFmpegReader r(path.str());
	r.Open();

	// Create AudioWaveformer and extract a smaller "average" sample set of audio data
	const int samples_per_second = 20;
	const int expected_total = static_cast<int>(std::ceil(r.info.duration * samples_per_second));
	REQUIRE(expected_total > 1);

	AudioWaveformer waveformer(&r);
	for (auto channel = 0; channel < r.info.channels; channel++) {
		AudioWaveformData waveform = waveformer.ExtractSamples(channel, samples_per_second, false);

		CHECK(waveform.rms_samples.size() == expected_total);
		CHECK(waveform.rms_samples[0] >= 0.0f);
		CHECK(waveform.rms_samples.back() >= 0.0f);
		CHECK(*std::max_element(waveform.rms_samples.begin(), waveform.rms_samples.end()) > 0.0f);

		waveform.clear();
	}

	// Clean up
	r.Close();
}

TEST_CASE( "Extract waveform data sintel", "[libopenshot][audiowaveformer]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());

	// Create AudioWaveformer and extract a smaller "average" sample set of audio data
	const int samples_per_second = 20;
	const int expected_total = static_cast<int>(std::ceil(r.info.duration * samples_per_second));
	REQUIRE(expected_total > 1);

	AudioWaveformer waveformer(&r);
	for (auto channel = 0; channel < r.info.channels; channel++) {
		AudioWaveformData waveform = waveformer.ExtractSamples(channel, samples_per_second, false);

		CHECK(waveform.rms_samples.size() == expected_total);
		CHECK(waveform.rms_samples[0] >= 0.0f);
		CHECK(waveform.rms_samples.back() >= 0.0f);

		waveform.clear();
	}

	// Clean up
	r.Close();
}


TEST_CASE( "Extract waveform data sintel (all channels)", "[libopenshot][audiowaveformer]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());

	// Create AudioWaveformer and extract a smaller "average" sample set of audio data
	const int samples_per_second = 20;
	const int expected_total = static_cast<int>(std::ceil(r.info.duration * samples_per_second));
	REQUIRE(expected_total > 1);

	AudioWaveformer waveformer(&r);
	AudioWaveformData waveform = waveformer.ExtractSamples(-1, samples_per_second, false);

	CHECK(waveform.rms_samples.size() == expected_total);
	CHECK(waveform.rms_samples[0] >= 0.0f);
	CHECK(waveform.rms_samples.back() >= 0.0f);

	waveform.clear();

	// Clean up
	r.Close();
}

TEST_CASE( "Channel selection returns data and rejects invalid channel", "[libopenshot][audiowaveformer][channels]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	AudioWaveformer waveformer(&r);
	const int samples_per_second = 20;
	const int expected_total = static_cast<int>(std::ceil(r.info.duration * samples_per_second));

	AudioWaveformData ch0 = waveformer.ExtractSamples(0, samples_per_second, false);
	AudioWaveformData ch1 = waveformer.ExtractSamples(1, samples_per_second, false);
	AudioWaveformData all = waveformer.ExtractSamples(-1, samples_per_second, false);

	CHECK(ch0.rms_samples.size() == expected_total);
	CHECK(ch1.rms_samples.size() == expected_total);
	CHECK(all.rms_samples.size() == expected_total);

	// All-channels max should be at least as large as per-channel max
	CHECK(*std::max_element(all.max_samples.begin(), all.max_samples.end()) >= *std::max_element(ch0.max_samples.begin(), ch0.max_samples.end()));
	CHECK(*std::max_element(all.max_samples.begin(), all.max_samples.end()) >= *std::max_element(ch1.max_samples.begin(), ch1.max_samples.end()));

	// Out of range channel returns empty data
	AudioWaveformData invalid = waveformer.ExtractSamples(10, samples_per_second, false);
	CHECK(invalid.rms_samples.empty());

	r.Close();
}

TEST_CASE( "FFmpegWriter FLAC includes duration metadata for waveform extraction", "[libopenshot][ffmpegwriter][audiowaveformer][flac]" )
{
	const std::string path = "AudioWaveformer-duration-unknown.flac";
	std::remove(path.c_str());

	const int sample_rate = 48000;
	const int samples = sample_rate / 2;
	std::vector<float> tone(samples);
	for (int i = 0; i < samples; ++i) {
		tone[i] = 0.25f * static_cast<float>(std::sin(2.0 * M_PI * 440.0 * static_cast<double>(i) / sample_rate));
	}

	FFmpegWriter writer(path);
	writer.SetAudioOptions(true, "flac", sample_rate, 1, LAYOUT_MONO, 192000);
	writer.Open();
	writer.WriteFrame(MakeWaveformFrame(1, sample_rate, tone));
	writer.WriteFrame(MakeWaveformFrame(2, sample_rate, tone));
	writer.Close();

	FFmpegReader reader(path);
	reader.Open();
	REQUIRE(reader.info.has_audio);
	CHECK(reader.info.duration > 0.0f);
	CHECK(reader.info.video_length > 0);

	AudioWaveformer waveformer(&reader);
	AudioWaveformData waveform = waveformer.ExtractSamples(-1, 20, true);

	CHECK_FALSE(waveform.max_samples.empty());
	CHECK(waveform.max_samples.size() == waveform.rms_samples.size());
	CHECK(*std::max_element(waveform.max_samples.begin(), waveform.max_samples.end()) > 0.0f);
	CHECK(*std::max_element(waveform.max_samples.begin(), waveform.max_samples.end()) <= Approx(1.0f).margin(0.0001f));

	reader.Close();
	std::remove(path.c_str());
}

TEST_CASE( "Waveform extraction does not mutate source reader video flag", "[libopenshot][audiowaveformer][mutation]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader reader(path.str());
	Clip clip(&reader);
	clip.Open();

	const bool original_has_video_clip = clip.Reader()->info.has_video;
	const bool original_has_video_reader = reader.info.has_video;
	REQUIRE(original_has_video_clip == original_has_video_reader);
	REQUIRE(original_has_video_reader);

	AudioWaveformer waveformer(&clip);
	AudioWaveformData waveform = waveformer.ExtractSamples(-1, 5, false);

	// Extraction should not flip has_video on the live reader/clip
	CHECK_FALSE(waveform.rms_samples.empty());
	CHECK(clip.Reader()->info.has_video == original_has_video_clip);
	CHECK(reader.info.has_video == original_has_video_reader);

	clip.Close();
	reader.Close();
}


TEST_CASE( "Extract waveform waits for reader reopen", "[libopenshot][audiowaveformer][stability]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader reader(path.str());
	reader.Open();

	AudioWaveformer waveformer(&reader);
	const int samples_per_second = 20;

	auto future_waveform = std::async(std::launch::async, [&]() {
		return waveformer.ExtractSamples(-1, samples_per_second, false);
	});

	reader.Close();
	reader.Open();
	reader.Close();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	reader.Open();

	AudioWaveformData waveform;
	REQUIRE_NOTHROW(waveform = future_waveform.get());
	CHECK_FALSE(waveform.rms_samples.empty());

	reader.Close();
}

TEST_CASE( "Extract waveform continues if caller closes original reader", "[libopenshot][audiowaveformer][stability]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader reader(path.str());
	reader.Open();

	AudioWaveformer waveformer(&reader);
	const int samples_per_second = 20;

	auto future_waveform = std::async(std::launch::async, [&]() {
		return waveformer.ExtractSamples(-1, samples_per_second, false);
	});

	// Closing the caller's reader should not affect a detached clone used for waveform extraction.
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	reader.Close();

	AudioWaveformData waveform;
	REQUIRE_NOTHROW(waveform = future_waveform.get());
	CHECK_FALSE(waveform.rms_samples.empty());

	reader.Close();
}

TEST_CASE( "Normalize & scale waveform data piano.wav", "[libopenshot][audiowaveformer]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";
	FFmpegReader r(path.str());

	// Create AudioWaveformer and extract a smaller "average" sample set of audio data
	const int samples_per_second = 20;
	const int expected_total = static_cast<int>(std::ceil(r.info.duration * samples_per_second));
	REQUIRE(expected_total > 1);

	AudioWaveformer waveformer(&r);
	for (auto channel = 0; channel < r.info.channels; channel++) {
		// Normalize values and scale them between -1 and +1
		AudioWaveformData waveform = waveformer.ExtractSamples(channel, samples_per_second, true);

		CHECK(waveform.rms_samples.size() == expected_total);
		CHECK(waveform.rms_samples[0] >= 0.0f);
		CHECK(waveform.rms_samples.back() >= 0.0f);
		CHECK(*std::max_element(waveform.max_samples.begin(), waveform.max_samples.end()) <= Approx(1.0f).margin(0.0001f));

		waveform.clear();
	}

	// Clean up
	r.Close();
}

TEST_CASE( "Extract waveform data clip slowed by time curve", "[libopenshot][audiowaveformer][clip][time]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";

	FFmpegReader reader(path.str());
	Clip clip(&reader);
	clip.Open();

	const int64_t original_video_length = clip.Reader()->info.video_length;
	const double fps_value = clip.Reader()->info.fps.ToDouble();
	REQUIRE(original_video_length > 0);
	REQUIRE(fps_value > 0.0);

	clip.time = Keyframe();
	clip.time.AddPoint(1.0, 1.0, LINEAR);
	clip.time.AddPoint(static_cast<double>(original_video_length) * 2.0,
					   static_cast<double>(original_video_length), LINEAR);

	AudioWaveformer waveformer(&clip);
	const int samples_per_second = 20;
	AudioWaveformData waveform = waveformer.ExtractSamples(-1, samples_per_second, false);

	const double expected_duration = (static_cast<double>(original_video_length) * 2.0) / fps_value;
	const int expected_total = static_cast<int>(std::ceil(expected_duration * samples_per_second));
	CHECK(waveform.rms_samples.size() == expected_total);
	CHECK(clip.time.GetLength() == original_video_length * 2);
	CHECK(clip.time.GetLength() == static_cast<int64_t>(std::llround(expected_duration * fps_value)));

	clip.Close();
	reader.Close();
}

TEST_CASE( "Extract waveform data clip reversed by time curve", "[libopenshot][audiowaveformer][clip][time]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";

	FFmpegReader reader(path.str());
	Clip clip(&reader);
	clip.Open();

	const int samples_per_second = 20;
	const int base_total = static_cast<int>(std::ceil(clip.Reader()->info.duration * samples_per_second));
	const int64_t original_video_length = clip.Reader()->info.video_length;
	const double fps_value = clip.Reader()->info.fps.ToDouble();
	REQUIRE(original_video_length > 0);
	REQUIRE(fps_value > 0.0);

	clip.time = Keyframe();
	clip.time.AddPoint(1.0, static_cast<double>(original_video_length), LINEAR);
	clip.time.AddPoint(static_cast<double>(original_video_length), 1.0, LINEAR);

	AudioWaveformer waveformer(&clip);
	AudioWaveformData waveform = waveformer.ExtractSamples(-1, samples_per_second, false);

	const double expected_duration = static_cast<double>(original_video_length) / fps_value;
	const int expected_total = static_cast<int>(std::ceil(expected_duration * samples_per_second));
	CHECK(waveform.rms_samples.size() == expected_total);
	CHECK(expected_total == base_total);
	CHECK(clip.time.GetLength() == original_video_length);
	CHECK(clip.time.GetLength() == static_cast<int64_t>(std::llround(expected_duration * fps_value)));

	clip.Close();
	reader.Close();
}

TEST_CASE( "Extract waveform data clip reversed and slowed", "[libopenshot][audiowaveformer][clip][time]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";

	FFmpegReader reader(path.str());
	Clip clip(&reader);
	clip.Open();

	const int samples_per_second = 20;
	const int base_total = static_cast<int>(std::ceil(clip.Reader()->info.duration * samples_per_second));
	const int64_t original_video_length = clip.Reader()->info.video_length;
	const double fps_value = clip.Reader()->info.fps.ToDouble();
	REQUIRE(original_video_length > 0);
	REQUIRE(fps_value > 0.0);

	clip.time = Keyframe();
	clip.time.AddPoint(1.0, static_cast<double>(original_video_length), LINEAR);
	clip.time.AddPoint(static_cast<double>(original_video_length) * 2.0, 1.0, LINEAR);

	AudioWaveformer waveformer(&clip);
	AudioWaveformData waveform = waveformer.ExtractSamples(-1, samples_per_second, false);

	const double expected_duration = (static_cast<double>(original_video_length) * 2.0) / fps_value;
	const int expected_total = static_cast<int>(std::ceil(expected_duration * samples_per_second));
	CHECK(waveform.rms_samples.size() == expected_total);
	CHECK(expected_total > base_total);
	CHECK(clip.time.GetLength() == original_video_length * 2);
	CHECK(clip.time.GetLength() == static_cast<int64_t>(std::llround(expected_duration * fps_value)));

	clip.Close();
	reader.Close();
}

TEST_CASE( "Clip duration uses parent timeline FPS when time-mapped", "[libopenshot][audiowaveformer][clip][time][timeline]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";

	FFmpegReader reader(path.str());
	Clip clip(&reader);
	clip.Open();

	const int64_t original_video_length = clip.Reader()->info.video_length;
	const double reader_fps = clip.Reader()->info.fps.ToDouble();
	REQUIRE(original_video_length > 0);
	REQUIRE(reader_fps > 0.0);

	Timeline timeline(
		640,
		480,
		Fraction(60, 1),
		clip.Reader()->info.sample_rate,
		clip.Reader()->info.channels,
		clip.Reader()->info.channel_layout);

	clip.ParentTimeline(&timeline);

	clip.time = Keyframe();
	clip.time.AddPoint(1.0, 1.0, LINEAR);
	clip.time.AddPoint(static_cast<double>(original_video_length) * 2.0,
					   static_cast<double>(original_video_length), LINEAR);

	const double timeline_fps = timeline.info.fps.ToDouble();
	REQUIRE(timeline_fps > 0.0);

	const double expected_duration = (static_cast<double>(original_video_length) * 2.0) / timeline_fps;
	CHECK(clip.time.GetLength() == static_cast<int64_t>(std::llround(expected_duration * timeline_fps)));

	clip.Close();
	reader.Close();
}

TEST_CASE( "Extract waveform from image (no audio)", "[libopenshot][audiowaveformer]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "front.png";
	FFmpegReader r(path.str());

	// Create AudioWaveformer and extract a smaller "average" sample set of audio data
	AudioWaveformer waveformer(&r);
	AudioWaveformData waveform = waveformer.ExtractSamples(-1, 20, false);

	CHECK(waveform.rms_samples.size() == 0);
	CHECK(waveform.max_samples.size() == 0);

	// Clean up
	r.Close();
}

TEST_CASE( "AudioWaveformData struct methods", "[libopenshot][audiowaveformer]" )
{
	// Create a reader
	AudioWaveformData waveform;

	// Resize data to 10 elements
	waveform.resize(10);
	CHECK(waveform.rms_samples.size() == 10);
	CHECK(waveform.max_samples.size() == 10);

	// Set all values = 1.0
	for (auto s = 0; s < waveform.rms_samples.size(); s++) {
		waveform.rms_samples[s] = 1.0;
		waveform.max_samples[s] = 1.0;
	}
	CHECK(waveform.rms_samples[0] == Approx(1.0f).margin(0.00001));
	CHECK(waveform.rms_samples[9] == Approx(1.0f).margin(0.00001));
	CHECK(waveform.max_samples[0] == Approx(1.0f).margin(0.00001));
	CHECK(waveform.max_samples[9] == Approx(1.0f).margin(0.00001));

	// Scale all values by 2
	waveform.scale(10, 2.0);
	CHECK(waveform.rms_samples.size() == 10);
	CHECK(waveform.max_samples.size() == 10);
	CHECK(waveform.rms_samples[0] == Approx(2.0f).margin(0.00001));
	CHECK(waveform.rms_samples[9] == Approx(2.0f).margin(0.00001));
	CHECK(waveform.max_samples[0] == Approx(2.0f).margin(0.00001));
	CHECK(waveform.max_samples[9] == Approx(2.0f).margin(0.00001));

	// Zero out all values
	waveform.zero(10);
	CHECK(waveform.rms_samples.size() == 10);
	CHECK(waveform.max_samples.size() == 10);
	CHECK(waveform.rms_samples[0] == Approx(0.0f).margin(0.00001));
	CHECK(waveform.rms_samples[9] == Approx(0.0f).margin(0.00001));
	CHECK(waveform.max_samples[0] == Approx(0.0f).margin(0.00001));
	CHECK(waveform.max_samples[9] == Approx(0.0f).margin(0.00001));

	// Access vectors and verify size
	std::vector<std::vector<float>> vectors = waveform.vectors();
	CHECK(vectors.size() == 2);
	CHECK(vectors[0].size() == 10);
	CHECK(vectors[0].size() == 10);

	// Clear and verify internal data is empty
	waveform.clear();
	CHECK(waveform.rms_samples.size() == 0);
	CHECK(waveform.max_samples.size() == 0);
	vectors = waveform.vectors();
	CHECK(vectors.size() == 2);
	CHECK(vectors[0].size() == 0);
	CHECK(vectors[0].size() == 0);
}

TEST_CASE( "Waveform chunks do not drift when sample rate is not a multiple of the density", "[libopenshot][audiowaveformer][timing]" )
{
	// 44100 / 200 = 220.5 samples per chunk. Integer chunking used 220 samples, which put
	// this click 11 samples late at 25 seconds (and 16 seconds late at the end of 2 hours).
	const std::string path = "AudioWaveformer-drift-44100.wav";
	const int sample_rate = 44100;
	const double click_seconds = 25.0;
	const int64_t click_start = static_cast<int64_t>(click_seconds * sample_rate);
	WriteStereoWav(path, sample_rate, 30.0, [&](int, int64_t index) {
		return (index >= click_start && index < click_start + 200) ? 0.9f : 0.001f;
	});

	FFmpegReader reader(path);
	reader.Open();
	AudioWaveformer waveformer(&reader);
	const int samples_per_second = 200;
	AudioWaveformData waveform = waveformer.ExtractSamples(-1, samples_per_second, false);

	REQUIRE(waveform.max_samples.size() == static_cast<size_t>(std::ceil(reader.info.duration * samples_per_second)));
	CHECK(ArgMax(waveform.max_samples) == static_cast<int>(click_seconds * samples_per_second));

	// The end of the media is covered (no truncated tail from accumulated rounding)
	CHECK(waveform.max_samples.back() > 0.0f);
	CHECK(waveform.rms_samples.back() > 0.0f);

	reader.Close();
	std::remove(path.c_str());
}

TEST_CASE( "Single-pass channel extraction matches per-channel extraction", "[libopenshot][audiowaveformer][channels]" )
{
	const std::string path = "AudioWaveformer-channels.wav";
	const int sample_rate = 48000;
	WriteStereoWav(path, sample_rate, 3.0, [&](int channel, int64_t index) {
		const double amplitude = channel == 0 ? 0.8 : 0.2;
		return static_cast<float>(amplitude * std::sin(2.0 * M_PI * 440.0 * index / sample_rate));
	});

	FFmpegReader reader(path);
	reader.Open();
	REQUIRE(reader.info.channels == 2);
	AudioWaveformer waveformer(&reader);
	const int samples_per_second = 200;

	AudioWaveformData all = waveformer.ExtractChannelSamples(samples_per_second, false);
	AudioWaveformData mixed = waveformer.ExtractSamples(-1, samples_per_second, false);
	REQUIRE(all.channel_count() == 2);
	REQUIRE(all.channel_rms_samples.size() == 2);
	REQUIRE(all.max_samples.size() == mixed.max_samples.size());
	for (size_t i = 0; i < mixed.max_samples.size(); i++) {
		CHECK(all.max_samples[i] == Approx(mixed.max_samples[i]).margin(1e-6));
		CHECK(all.rms_samples[i] == Approx(mixed.rms_samples[i]).margin(1e-6));
	}

	for (int channel = 0; channel < 2; channel++) {
		AudioWaveformData single = waveformer.ExtractSamples(channel, samples_per_second, false);
		const auto& channel_max = all.channel_max_samples[channel];
		const auto& channel_rms = all.channel_rms_samples[channel];
		REQUIRE(channel_max.size() == single.max_samples.size());
		REQUIRE(channel_rms.size() == single.rms_samples.size());
		for (size_t i = 0; i < single.max_samples.size(); i++) {
			CHECK(channel_max[i] == Approx(single.max_samples[i]).margin(1e-6));
			CHECK(channel_rms[i] == Approx(single.rms_samples[i]).margin(1e-6));
		}
		std::vector<std::vector<float>> vectors = all.channel_vectors(channel);
		CHECK(vectors[0] == channel_max);
		CHECK(vectors[1] == channel_rms);
	}

	// Channels keep their own levels: left peaks near 0.8, right near 0.2, mix follows the loudest
	const size_t middle = all.max_samples.size() / 2;
	CHECK(all.channel_max_samples[0][middle] == Approx(0.8f).margin(0.01f));
	CHECK(all.channel_max_samples[1][middle] == Approx(0.2f).margin(0.01f));
	CHECK(all.max_samples[middle] == Approx(0.8f).margin(0.01f));
	CHECK(all.channel_rms_samples[0][middle] == Approx(0.8f / std::sqrt(2.0f)).margin(0.01f));
	CHECK(all.rms_samples[middle] == Approx(std::sqrt((0.32f + 0.02f) / 2.0f)).margin(0.01f));

	// Out-of-range channel requests return empty envelopes
	CHECK(all.channel_vectors(2)[0].empty());
	CHECK(all.channel_vectors(-1)[1].empty());

	// Normalizing scales every envelope by the loudest mixed peak
	AudioWaveformData normalized = waveformer.ExtractChannelSamples(samples_per_second, true);
	const float scale = 1.0f / *std::max_element(all.max_samples.begin(), all.max_samples.end());
	CHECK(*std::max_element(normalized.max_samples.begin(), normalized.max_samples.end()) == Approx(1.0f).margin(1e-5));
	CHECK(normalized.channel_max_samples[1][middle] == Approx(all.channel_max_samples[1][middle] * scale).margin(1e-5));

	reader.Close();
	std::remove(path.c_str());
}

TEST_CASE( "Single-pass channel extraction of media without audio is empty", "[libopenshot][audiowaveformer][channels]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "front.png";
	FFmpegReader reader(path.str());
	AudioWaveformer waveformer(&reader);
	AudioWaveformData waveform = waveformer.ExtractChannelSamples(200, false);
	CHECK(waveform.max_samples.empty());
	CHECK(waveform.channel_count() == 0);
	reader.Close();
}

TEST_CASE( "Single-pass channel extraction of a video file", "[libopenshot][audiowaveformer][channels]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader reader(path.str());
	reader.Open();
	AudioWaveformer waveformer(&reader);
	const int samples_per_second = 20;
	AudioWaveformData waveform = waveformer.ExtractChannelSamples(samples_per_second, false);
	const size_t expected_total = static_cast<size_t>(std::ceil(reader.info.duration * samples_per_second));
	CHECK(waveform.max_samples.size() == expected_total);
	REQUIRE(waveform.channel_count() == reader.info.channels);
	for (int channel = 0; channel < waveform.channel_count(); channel++) {
		CHECK(waveform.channel_max_samples[channel].size() == expected_total);
		CHECK(waveform.channel_rms_samples[channel].size() == expected_total);
	}
	// Live reader keeps decoding video for preview
	CHECK(reader.info.has_video);
	reader.Close();
}

TEST_CASE( "Audio-only reader cache stays bounded on long sequential reads", "[libopenshot][ffmpegreader][audiowaveformer][cache]" )
{
	// An audio-only cache budget sized from width x height never evicted, so every frame of
	// the file stayed cached and each insert rescanned the whole cache (quadratic reads).
	const std::string path = "AudioWaveformer-cache.wav";
	WriteStereoWav(path, 48000, 60.0, [](int, int64_t index) {
		return static_cast<float>(0.5 * std::sin(index * 0.01));
	});

	FFmpegReader reader(path);
	reader.Open();
	REQUIRE_FALSE(reader.info.has_video);
	REQUIRE(reader.info.video_length > 1000);
	for (int64_t frame = 1; frame <= reader.info.video_length; frame++) {
		reader.GetFrame(frame);
	}
	CHECK(reader.GetCache()->Count() < 300);
	CHECK(reader.GetCache()->Count() > 0);
	reader.Close();
	std::remove(path.c_str());
}

TEST_CASE( "ApplyKeyframes maps time and volume curves onto envelopes", "[libopenshot][audiowaveformer][keyframes]" )
{
	AudioWaveformer waveformer(nullptr);
	const Fraction fps(30, 1);
	const int samples_per_second = 30; // one envelope sample per frame keeps the math readable
	std::vector<float> ramp(60);
	std::vector<float> ramp_rms(60);
	for (size_t i = 0; i < ramp.size(); i++) {
		ramp[i] = static_cast<float>(i) / 100.0f;
		ramp_rms[i] = ramp[i] / 2.0f;
	}

	SECTION("no keyframes keeps the envelope") {
		AudioWaveformData out = waveformer.ApplyKeyframes(ramp, ramp_rms, nullptr, nullptr, fps, samples_per_second);
		CHECK(out.max_samples == ramp);
		CHECK(out.rms_samples == ramp_rms);
	}

	SECTION("identity time curve and unity volume keep the envelope") {
		Keyframe time = LinearKeyframe({{1, 1}, {60, 60}});
		Keyframe volume(1.0);
		AudioWaveformData out = waveformer.ApplyKeyframes(ramp, ramp_rms, &time, &volume, fps, samples_per_second);
		REQUIRE(out.max_samples.size() == ramp.size());
		for (size_t i = 0; i < ramp.size(); i++) {
			CHECK(out.max_samples[i] == Approx(ramp[i]).margin(1e-6));
			CHECK(out.rms_samples[i] == Approx(ramp_rms[i]).margin(1e-6));
		}
	}

	SECTION("single-point time curve and empty volume curve are ignored") {
		Keyframe time(1.0);
		Keyframe volume;
		AudioWaveformData out = waveformer.ApplyKeyframes(ramp, ramp_rms, &time, &volume, fps, samples_per_second);
		CHECK(out.max_samples == ramp);
	}

	SECTION("volume curve scales max and RMS") {
		Keyframe volume = LinearKeyframe({{1, 0.0}, {60, 1.0}});
		AudioWaveformData out = waveformer.ApplyKeyframes(ramp, ramp_rms, nullptr, &volume, fps, samples_per_second);
		REQUIRE(out.max_samples.size() == ramp.size());
		CHECK(out.max_samples.front() == Approx(0.0f).margin(1e-6));
		CHECK(out.max_samples[30] == Approx(ramp[30] * volume.GetValue(31)).margin(1e-5));
		CHECK(out.rms_samples[30] == Approx(ramp_rms[30] * volume.GetValue(31)).margin(1e-5));
		CHECK(out.max_samples.back() == Approx(ramp.back()).margin(1e-5));
	}

	SECTION("reversed time curve reverses the envelope") {
		Keyframe time = LinearKeyframe({{1, 60}, {60, 1}});
		AudioWaveformData out = waveformer.ApplyKeyframes(ramp, ramp_rms, &time, nullptr, fps, samples_per_second);
		REQUIRE(out.max_samples.size() == ramp.size());
		CHECK(out.max_samples.front() == Approx(ramp.back()).margin(1e-6));
		CHECK(out.max_samples.back() == Approx(ramp.front()).margin(1e-6));
	}

	SECTION("slowed time curve doubles the length and interpolates") {
		Keyframe time = LinearKeyframe({{1, 1}, {120, 60}});
		AudioWaveformData out = waveformer.ApplyKeyframes(ramp, ramp_rms, &time, nullptr, fps, samples_per_second);
		REQUIRE(out.max_samples.size() == 120);
		CHECK(out.max_samples.front() == Approx(ramp.front()).margin(1e-6));
		CHECK(out.max_samples[119] == Approx(ramp.back()).margin(1e-6));
		for (size_t i = 1; i < out.max_samples.size(); i++) {
			CHECK(out.max_samples[i] >= out.max_samples[i - 1]);
		}
	}

	SECTION("short or empty RMS envelopes do not read out of bounds") {
		std::vector<float> short_rms(10, 0.5f);
		AudioWaveformData out = waveformer.ApplyKeyframes(ramp, short_rms, nullptr, nullptr, fps, samples_per_second);
		REQUIRE(out.rms_samples.size() == ramp.size());
		CHECK(out.rms_samples[5] == Approx(0.5f));
		CHECK(out.rms_samples[50] == Approx(0.0f));
		AudioWaveformData empty = waveformer.ApplyKeyframes(std::vector<float>(), ramp_rms, nullptr, nullptr, fps, samples_per_second);
		CHECK(empty.max_samples.empty());
	}

	SECTION("invalid density or fps returns empty data") {
		CHECK(waveformer.ApplyKeyframes(ramp, ramp_rms, nullptr, nullptr, fps, 0).max_samples.empty());
		CHECK(waveformer.ApplyKeyframes(ramp, ramp_rms, nullptr, nullptr, Fraction(0, 1), samples_per_second).max_samples.empty());
	}
}
