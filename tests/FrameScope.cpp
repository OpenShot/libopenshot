/**
 * @file
 * @brief Unit tests for FrameScope class
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <memory>
#include <sstream>
#include <vector>

#include <QColor>
#include <QImage>

#include "Frame.h"
#include "FrameScope.h"
#include "openshot_catch.h"

using namespace openshot;

static std::shared_ptr<Frame> makeVideoScopeFrame()
{
	// Must use Format_RGBA8888_Premultiplied — the format libopenshot always
	// stores frames in. FrameScope reads raw bytes as [R=0, G=1, B=2, A=3].
	QImage img(4, 1, QImage::Format_RGBA8888_Premultiplied);
	img.setPixelColor(0, 0, QColor(0, 0, 0, 255));
	img.setPixelColor(1, 0, QColor(255, 0, 0, 255));
	img.setPixelColor(2, 0, QColor(0, 255, 0, 255));
	img.setPixelColor(3, 0, QColor(0, 0, 255, 255));
	auto frame = std::make_shared<Frame>();
	*frame->GetImage() = img;
	return frame;
}

static std::shared_ptr<Frame> makeAudioScopeFrame()
{
	auto frame = std::make_shared<Frame>(1, 8, 2);
	frame->SampleRate(48000);

	const float left[] = {0.0f, 0.5f, -0.25f, 1.0f, -1.0f, 0.25f, -0.5f, 0.0f};
	const float right[] = {0.1f, -0.2f, 0.3f, -0.4f, 0.5f, -0.6f, 0.7f, -0.8f};
	frame->AddAudio(true, 0, 0, left, 8, 1.0f);
	frame->AddAudio(true, 1, 0, right, 8, 1.0f);
	return frame;
}

TEST_CASE("FrameScope returns empty-present flags for missing frame", "[framescope]")
{
	FrameScope scope;
	Json::Value root = scope.JsonValue();

	CHECK(root["version"].asInt() == 1);
	CHECK(root["video"]["present"].asBool() == false);
	CHECK(root["audio"]["present"].asBool() == false);
}

TEST_CASE("FrameScope analyzes video histogram and waveform data", "[framescope][video]")
{
	FrameScope scope(makeVideoScopeFrame(), 4, 4);
	Json::Value root = scope.JsonValue();

	REQUIRE(root["video"]["present"].asBool() == true);
	CHECK(root["video"]["width"].asInt() == 4);
	CHECK(root["video"]["height"].asInt() == 1);
	CHECK(root["video"]["histogram"]["red"].size() == 256);
	CHECK(root["video"]["histogram"]["green"].size() == 256);
	CHECK(root["video"]["histogram"]["blue"].size() == 256);
	CHECK(root["video"]["waveform"]["columns"].asInt() == 4);
	CHECK(root["video"]["waveform"]["bins"].asInt() == 256);
	CHECK(root["video"]["waveform"]["luma"].size() == 4 * 256);
	CHECK(root["video"]["waveform"]["red"].size() == 4 * 256);
	CHECK(root["video"]["waveform"]["green"].size() == 4 * 256);
	CHECK(root["video"]["waveform"]["blue"].size() == 4 * 256);
	CHECK(root["video"]["vectorscope"]["size"].asInt() == 256);
	CHECK(root["video"]["vectorscope"]["density"].size() == 256 * 256);
	CHECK(root["video"]["histogram"]["red"][255].asInt() == 1);
	CHECK(root["video"]["histogram"]["green"][255].asInt() == 1);
	CHECK(root["video"]["histogram"]["blue"][255].asInt() == 1);
	CHECK(root["video"]["histogram"]["luma"][0].asInt() == 1);
	CHECK(root["video"]["waveform"]["red"][1 * 256 + 255].asInt() == 1);
	CHECK(root["video"]["waveform"]["green"][2 * 256 + 255].asInt() == 1);
	CHECK(root["video"]["waveform"]["blue"][3 * 256 + 255].asInt() == 1);
	CHECK(root["video"]["summary"]["avg_luma"].asDouble() > 0.2);
}

TEST_CASE("FrameScope exposes typed video getters", "[framescope][video][getters]")
{
	FrameScope scope(makeVideoScopeFrame(), 4, 4);

	REQUIRE(scope.HasVideo() == true);
	CHECK(scope.GetVideoWidth() == 4);
	CHECK(scope.GetVideoHeight() == 1);
	CHECK(scope.GetWaveformColumns() == 4);
	CHECK(scope.GetWaveformBins() == 256);
	CHECK(scope.GetVideoHistogramRed().size() == 256);
	CHECK(scope.GetVideoHistogramGreen()[255] == 1);
	CHECK(scope.GetVideoWaveformRed().size() == 4 * 256);
	CHECK(scope.GetVideoWaveformRed()[1 * 256 + 255] == 1);
	CHECK(scope.GetVideoWaveformBlue()[3 * 256 + 255] == 1);
	CHECK(scope.GetVideoHistogramRedBins()[255] == 1u);
	CHECK(scope.GetVideoWaveformGreenBins()[2 * 256 + 255] == 1u);
	CHECK(scope.GetVideoVectorscopeBins().size() == 256u * 256u);
	CHECK(scope.GetVideoVectorscope().size() == 256 * 256);
	CHECK(scope.GetVideoAverageLuma() > 0.2);
}

TEST_CASE("FrameScope exposes vectorscope bins and configurable size", "[framescope][video][vectorscope]")
{
	FrameScope scope(makeVideoScopeFrame(), 4, 4, 64);

	REQUIRE(scope.HasVideo() == true);
	CHECK(scope.GetVectorscopeSize() == 64);
	CHECK(scope.GetVideoVectorscopeBins().size() == 64u * 64u);

	uint32_t vectorscope_sum = 0;
	for (uint32_t value : scope.GetVideoVectorscopeBins())
		vectorscope_sum += value;

	CHECK(vectorscope_sum == 4u);

	scope.SetVectorscopeSize(32);
	CHECK(scope.GetVectorscopeSize() == 32);
	CHECK(scope.GetVideoVectorscopeBins().size() == 32u * 32u);
}

TEST_CASE("FrameScope analyzes audio waveform and summary data", "[framescope][audio]")
{
	FrameScope scope(makeAudioScopeFrame(), 4, 4);
	Json::Value root = scope.JsonValue();

	REQUIRE(root["audio"]["present"].asBool() == true);
	CHECK(root["audio"]["channels"].asInt() == 2);
	CHECK(root["audio"]["samples"].asInt() == 8);
	CHECK(root["audio"]["sample_rate"].asInt() == 48000);
	CHECK(root["audio"]["waveform"]["buckets"].asInt() == 4);
	CHECK(root["audio"]["waveform"]["min"].size() == 2);
	CHECK(root["audio"]["waveform"]["max"].size() == 2);
	CHECK(root["audio"]["waveform"]["min"][0].size() == 4);
	CHECK(root["audio"]["waveform"]["max"][1].size() == 4);
	CHECK(root["audio"]["summary"]["peak"][0].asFloat() == Approx(1.0f));
	CHECK(root["audio"]["summary"]["peak"][1].asFloat() == Approx(0.8f));
	CHECK(root["audio"]["summary"]["clipped_samples"][0].asInt() == 2);
	CHECK(root["audio"]["summary"]["clipped_samples"][1].asInt() == 0);
	CHECK(root["audio"]["summary"]["rms"][0].asFloat() > 0.0f);
}

TEST_CASE("FrameScope exposes typed audio getters", "[framescope][audio][getters]")
{
	FrameScope scope(makeAudioScopeFrame(), 4, 4);

	REQUIRE(scope.HasAudio() == true);
	CHECK(scope.GetAudioChannels() == 2);
	CHECK(scope.GetAudioSamples() == 8);
	CHECK(scope.GetAudioSampleRate() == 48000);
	CHECK(scope.GetAudioBuckets() == 4);
	CHECK(scope.GetAudioPeakLevels().size() == 2);
	CHECK(scope.GetAudioPeakLevels()[0] == Approx(1.0f));
	CHECK(scope.GetAudioPeakLevelsRef()[1] == Approx(0.8f));
	CHECK(scope.GetAudioRmsLevels()[1] > 0.0f);
	CHECK(scope.GetAudioClippedSamples()[0] == 2);
	CHECK(scope.GetAudioClippedSamplesRef()[0] == 2u);
	CHECK(scope.GetAudioWaveformMin(0).size() == 4);
	CHECK(scope.GetAudioWaveformMax(1).size() == 4);
	CHECK(scope.GetAudioWaveformMax(99).empty());
}

TEST_CASE("FrameScope setting changes only affect matching scope families", "[framescope][settings]")
{
	FrameScope scope(makeAudioScopeFrame(), 4, 4, 64);

	REQUIRE(scope.HasAudio() == true);
	const std::vector<float> original_peak = scope.GetAudioPeakLevels();
	scope.SetVectorscopeSize(32);
	CHECK(scope.GetAudioPeakLevels() == original_peak);

	auto video_frame = makeVideoScopeFrame();
	scope.SetFrame(video_frame);
	REQUIRE(scope.HasVideo() == true);
	const std::vector<uint32_t> original_vectorscope = scope.GetVideoVectorscopeBins();
	scope.SetAudioBuckets(8);
	CHECK(scope.GetVideoVectorscopeBins() == original_vectorscope);
}

TEST_CASE("FrameScope Json string parses cleanly", "[framescope][json]")
{
	FrameScope scope(makeAudioScopeFrame(), 8, 4);
	std::istringstream json_stream(scope.Json());
	Json::CharReaderBuilder rb;
	Json::Value root;
	std::string errs;

	REQUIRE(Json::parseFromStream(rb, json_stream, &root, &errs));
	CHECK(root["audio"]["present"].asBool() == true);
}

TEST_CASE("FrameScope reads RGBA8888 pixel byte order correctly", "[framescope][video][rgba]")
{
	// Regression test: FrameScope must treat pixel bytes as [R=0, G=1, B=2, A=3]
	// (QImage::Format_RGBA8888_Premultiplied — the canonical libopenshot frame
	// format). A previous bug read them as BGRA (Format_ARGB32 order), causing
	// the red and blue channels to report each other's data.

	// Pure-red pixel: only histogram_red[255] should be non-zero.
	{
		QImage img(1, 1, QImage::Format_RGBA8888_Premultiplied);
		img.setPixelColor(0, 0, QColor(255, 0, 0, 255));
		auto frame = std::make_shared<Frame>();
		*frame->GetImage() = img;
		FrameScope scope(frame, 1, 1);

		CHECK(scope.GetVideoHistogramRed()[255]  == 1);
		CHECK(scope.GetVideoHistogramGreen()[255] == 0);
		CHECK(scope.GetVideoHistogramBlue()[255]  == 0);
	}

	// Pure-blue pixel: only histogram_blue[255] should be non-zero.
	{
		QImage img(1, 1, QImage::Format_RGBA8888_Premultiplied);
		img.setPixelColor(0, 0, QColor(0, 0, 255, 255));
		auto frame = std::make_shared<Frame>();
		*frame->GetImage() = img;
		FrameScope scope(frame, 1, 1);

		CHECK(scope.GetVideoHistogramRed()[255]  == 0);
		CHECK(scope.GetVideoHistogramGreen()[255] == 0);
		CHECK(scope.GetVideoHistogramBlue()[255]  == 1);
	}

	// Pure-green pixel: only histogram_green[255] should be non-zero.
	{
		QImage img(1, 1, QImage::Format_RGBA8888_Premultiplied);
		img.setPixelColor(0, 0, QColor(0, 255, 0, 255));
		auto frame = std::make_shared<Frame>();
		*frame->GetImage() = img;
		FrameScope scope(frame, 1, 1);

		CHECK(scope.GetVideoHistogramRed()[255]  == 0);
		CHECK(scope.GetVideoHistogramGreen()[255] == 1);
		CHECK(scope.GetVideoHistogramBlue()[255]  == 0);
	}
}
