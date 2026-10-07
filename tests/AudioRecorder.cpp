/**
 * @file
 * @brief Unit tests for openshot::AudioRecorder helper classes
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <future>
#include <memory>
#include <string>

#include "openshot_catch.h"

#include "AudioRecorder.h"
#include "Exceptions.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "Frame.h"

using namespace openshot;

namespace {
AudioRecorderBlock MakeBlock(int sample_rate, int64_t first_sample, std::vector<float> left, std::vector<float> right = {})
{
	AudioRecorderBlock block;
	block.sample_rate = sample_rate;
	block.first_sample = first_sample;
	block.channels.push_back(std::move(left));
	if (!right.empty()) {
		block.channels.push_back(std::move(right));
	}
	return block;
}
}

TEST_CASE("Audio recorder settings validation", "[libopenshot][audiorecorder]")
{
	AudioRecorderSettings settings;
	settings.path = "settings-validation.wav";
	CHECK_NOTHROW([&settings]() { AudioRecorder recorder(settings); }());

	settings.path = "";
	CHECK_THROWS_AS([&settings]() { AudioRecorder recorder(settings); }(), InvalidOptions);

	settings.path = "settings-validation.wav";
	settings.channels = 0;
	CHECK_THROWS_AS([&settings]() { AudioRecorder recorder(settings); }(), InvalidChannels);

	settings.channels = 1;
	settings.sample_rate = 7999;
	CHECK_THROWS_AS([&settings]() { AudioRecorder recorder(settings); }(), InvalidSampleRate);
}

TEST_CASE("Audio recorder level meter calculates peak and RMS", "[libopenshot][audiorecorder]")
{
	AudioRecorderLevelMeter meter;
	auto level = meter.ProcessBlock(MakeBlock(
		48000,
		24000,
		{0.0f, -0.5f, 0.25f, 1.0f},
		{0.0f, 0.25f, -0.25f, 0.5f}));

	REQUIRE(level.peak.size() == 2);
	REQUIRE(level.rms.size() == 2);
	CHECK(level.timestamp == Approx(0.5));
	CHECK(level.peak[0] == Approx(1.0f));
	CHECK(level.peak[1] == Approx(0.5f));
	CHECK(level.rms[0] == Approx(std::sqrt((0.0 + 0.25 + 0.0625 + 1.0) / 4.0)));
	CHECK(level.rms[1] == Approx(std::sqrt((0.0 + 0.0625 + 0.0625 + 0.25) / 4.0)));
	CHECK(level.clipped);
}

TEST_CASE("Audio recorder waveform accumulator downsamples max and RMS", "[libopenshot][audiorecorder]")
{
	AudioRecorderWaveformAccumulator accumulator(8, 2);
	auto chunks = accumulator.ProcessBlock(MakeBlock(
		8,
		0,
		{0.0f, 0.5f, -1.0f, 0.25f, 0.25f, -0.25f, 0.75f, -0.5f}));

	REQUIRE(chunks.size() == 1);
	CHECK(chunks[0].samples_per_second == 2);
	CHECK(chunks[0].start_time == Approx(0.0));
	CHECK(chunks[0].duration == Approx(1.0));
	REQUIRE(chunks[0].max_samples.size() == 2);
	REQUIRE(chunks[0].rms_samples.size() == 2);
	CHECK(chunks[0].max_samples[0] == Approx(1.0f));
	CHECK(chunks[0].max_samples[1] == Approx(0.75f));
	CHECK(chunks[0].rms_samples[0] == Approx(std::sqrt((0.0 + 0.25 + 1.0 + 0.0625) / 4.0)));
	CHECK(chunks[0].rms_samples[1] == Approx(std::sqrt((0.0625 + 0.0625 + 0.5625 + 0.25) / 4.0)));

	auto snapshot = accumulator.Snapshot();
	CHECK(snapshot.max_samples == chunks[0].max_samples);
	CHECK(snapshot.rms_samples == chunks[0].rms_samples);
}

TEST_CASE("Audio recording frame factory creates audio-only frames", "[libopenshot][audiorecorder]")
{
	auto block = MakeBlock(
		44100,
		0,
		{0.1f, 0.2f, 0.3f},
		{-0.1f, -0.2f, -0.3f});

	auto frame = AudioRecordingFrameFactory::CreateFrame(block, LAYOUT_STEREO, 42);
	REQUIRE(frame);
	CHECK(frame->number == 42);
	CHECK(frame->SampleRate() == 44100);
	CHECK(frame->ChannelsLayout() == LAYOUT_STEREO);
	CHECK(frame->GetAudioChannelsCount() == 2);
	CHECK(frame->GetAudioSamplesCount() == 3);
	CHECK(frame->GetAudioSamples(0)[1] == Approx(0.2f));
	CHECK(frame->GetAudioSamples(1)[2] == Approx(-0.3f));
}

TEST_CASE("Audio recorder frames write through FFmpegWriter", "[libopenshot][audiorecorder][ffmpegwriter]")
{
	const std::string path = "AudioRecorder-output.wav";
	std::remove(path.c_str());

	const int sample_rate = 8000;
	const int samples = sample_rate / 10;
	std::vector<float> tone(samples);
	for (int i = 0; i < samples; ++i) {
		tone[i] = 0.25f * static_cast<float>(std::sin(2.0 * M_PI * 440.0 * static_cast<double>(i) / sample_rate));
	}

	FFmpegWriter writer(path);
	writer.SetAudioOptions(true, "pcm_s16le", sample_rate, 1, LAYOUT_MONO, 128000);
	writer.Open();
	writer.WriteFrame(AudioRecordingFrameFactory::CreateFrame(MakeBlock(sample_rate, 0, tone), LAYOUT_MONO, 1));
	writer.Close();

	FFmpegReader reader(path);
	reader.Open();
	CHECK(reader.info.has_audio);
	CHECK(reader.info.sample_rate == sample_rate);
	CHECK(reader.info.channels == 1);
	CHECK(reader.info.duration > 0.05f);
	auto frame = reader.GetFrame(1);
	CHECK(frame->GetAudioChannelsCount() == 1);
	CHECK(frame->GetAudioSamplesCount() > 0);
	reader.Close();
}

TEST_CASE("Audio recorder silent frames produce readable zero waveform", "[libopenshot][audiorecorder][ffmpegwriter][audiowaveformer]")
{
	const std::string path = "AudioRecorder-silent-output.wav";
	std::remove(path.c_str());

	const int sample_rate = 8000;
	const int samples = sample_rate / 10;
	std::vector<float> silence(samples, 0.0f);

	FFmpegWriter writer(path);
	writer.SetAudioOptions(true, "pcm_s16le", sample_rate, 1, LAYOUT_MONO, 128000);
	writer.Open();
	writer.WriteFrame(AudioRecordingFrameFactory::CreateFrame(MakeBlock(sample_rate, 0, silence), LAYOUT_MONO, 1));
	writer.Close();

	FFmpegReader reader(path);
	reader.Open();
	REQUIRE(reader.info.has_audio);
	CHECK(reader.info.duration > 0.05f);

	AudioWaveformer waveformer(&reader);
	AudioWaveformData waveform = waveformer.ExtractSamples(-1, 20, true);

	REQUIRE_FALSE(waveform.max_samples.empty());
	CHECK(waveform.max_samples.size() == waveform.rms_samples.size());
	CHECK(*std::max_element(waveform.max_samples.begin(), waveform.max_samples.end()) == Approx(0.0f));
	CHECK(*std::max_element(waveform.rms_samples.begin(), waveform.rms_samples.end()) == Approx(0.0f));

	reader.Close();
	std::remove(path.c_str());
}

TEST_CASE("Audio recorder empty writer output has no waveform samples", "[libopenshot][audiorecorder][ffmpegwriter][audiowaveformer]")
{
	const std::string path = "AudioRecorder-empty-output.wav";
	std::remove(path.c_str());

	FFmpegWriter writer(path);
	writer.SetAudioOptions(true, "pcm_s16le", 8000, 1, LAYOUT_MONO, 128000);
	writer.Open();
	writer.Close();

	FFmpegReader reader(path);
	reader.Open();

	AudioWaveformer waveformer(&reader);
	auto future_waveform = std::async(std::launch::async, [&]() {
		return waveformer.ExtractSamples(-1, 20, true);
	});

	REQUIRE(future_waveform.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
	AudioWaveformData waveform = future_waveform.get();
	CHECK(waveform.max_samples.empty());
	CHECK(waveform.rms_samples.empty());

	reader.Close();
	std::remove(path.c_str());
}
