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
#include "Timeline.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <thread>
#include <sstream>


using namespace openshot;

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
