/**
 * @file
 * @brief Unit tests for openshot::FFmpegReader
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <sstream>
#include <memory>
#include <set>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <chrono>

#include "openshot_catch.h"

#define private public
#include "FFmpegReader.h"
#undef private
#include "Exceptions.h"
#include "Frame.h"
#include "Timeline.h"
#include "Json.h"

using namespace openshot;

namespace {

double SampleAverageLuma(const std::shared_ptr<Frame>& frame, int sample_grid = 4) {
	const int width = frame->GetWidth();
	const int height = frame->GetHeight();
	if (width <= 0 || height <= 0) {
		return 0.0;
	}

	int64_t luma_sum = 0;
	int64_t sample_count = 0;
	for (int y = 0; y < sample_grid; ++y) {
		const int row = std::min(height - 1, (y * height) / sample_grid);
		const unsigned char* pixels = frame->GetPixels(row);
		for (int x = 0; x < sample_grid; ++x) {
			const int col = std::min(width - 1, (x * width) / sample_grid);
			const int pixel_index = col * 4;
			luma_sum += (pixels[pixel_index] + pixels[pixel_index + 1] + pixels[pixel_index + 2]) / 3;
			++sample_count;
		}
	}

	return sample_count > 0
		? static_cast<double>(luma_sum) / static_cast<double>(sample_count)
		: 0.0;
}

struct HardwareDecoderSettingsGuard {
	int decoder = 0;
	int device = 0;

	HardwareDecoderSettingsGuard()
		: decoder(Settings::Instance()->HARDWARE_DECODER),
		  device(Settings::Instance()->HW_DE_DEVICE_SET) {}

	~HardwareDecoderSettingsGuard() {
		Settings::Instance()->HARDWARE_DECODER = decoder;
		Settings::Instance()->HW_DE_DEVICE_SET = device;
	}
};

struct TemporaryFileGuard {
	std::string path;

	explicit TemporaryFileGuard(std::string temp_path)
		: path(std::move(temp_path)) {}

	~TemporaryFileGuard() {
		if (!path.empty()) {
			std::remove(path.c_str());
		}
	}
};

}

TEST_CASE( "Invalid_Path", "[libopenshot][ffmpegreader]" )
{
	// Check invalid path and error details
	const std::string invalid_path = "/tmp/__openshot_missing_test_file__.mp4";
	try {
		FFmpegReader r(invalid_path);
		FAIL("Expected InvalidFile for missing media path");
	} catch (const InvalidFile& e) {
		const std::string message = e.what();
		CHECK(message.find("FFmpegReader could not open media file.") != std::string::npos);
		CHECK(message.find(invalid_path) != std::string::npos);
	}
}

TEST_CASE( "GetFrame_Before_Opening", "[libopenshot][ffmpegreader]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";
	FFmpegReader r(path.str());

	// Check invalid path
	CHECK_THROWS_AS(r.GetFrame(1), ReaderClosed);
}

TEST_CASE( "Check_Audio_File", "[libopenshot][ffmpegreader]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";
	FFmpegReader r(path.str());
	r.Open();

	// Get frame 1
	std::shared_ptr<Frame> f = r.GetFrame(1);

	// Get the number of channels and samples
	float *samples = f->GetAudioSamples(0);

	// Check audio properties
	CHECK(f->GetAudioChannelsCount() == 2);
	CHECK(f->GetAudioSamplesCount() == 266);

	// Check actual sample values (to be sure the waveform is correct)
	CHECK(samples[0] == Approx(0.0f).margin(0.00001));
	CHECK(samples[50] == Approx(0.0f).margin(0.00001));
	CHECK(samples[100] == Approx(0.0f).margin(0.00001));
	CHECK(samples[200] == Approx(0.0f).margin(0.00001));
	CHECK(samples[230] == Approx(0.16406f).margin(0.00001));
	CHECK(samples[265] == Approx(-0.06250f).margin(0.00001));

	// Close reader
	r.Close();
}

TEST_CASE( "Audio_Only_SetJson_Preserves_Stored_Dimensions", "[libopenshot][ffmpegreader][json]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";

	FFmpegReader r(path.str());
	std::stringstream json;
	json << "{\"type\":\"FFmpegReader\","
		<< "\"path\":\"" << path.str() << "\","
		<< "\"has_audio\":true,"
		<< "\"has_video\":false,"
		<< "\"width\":1280,"
		<< "\"height\":720,"
		<< "\"fps\":{\"num\":30,\"den\":1},"
		<< "\"sample_rate\":44100,"
		<< "\"channels\":2,"
		<< "\"channel_layout\":3,"
		<< "\"duration\":1.0}";

	r.SetJson(json.str());
	r.Open();

	CHECK(r.info.has_audio);
	CHECK_FALSE(r.info.has_video);
	CHECK(r.info.width == 1280);
	CHECK(r.info.height == 720);
	CHECK(r.GetFrame(1)->GetWidth() == 1280);
	CHECK(r.GetFrame(1)->GetHeight() == 720);
}

TEST_CASE( "Check_Video_File", "[libopenshot][ffmpegreader]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "test.mp4";
	FFmpegReader r(path.str());
	r.Open();

	// Get frame 1
	std::shared_ptr<Frame> f = r.GetFrame(1);

	// Get the image data
	const unsigned char* pixels = f->GetPixels(10);
	int pixel_index = 112 * 4; // pixel 112 (4 bytes per pixel)

	// Check image properties on scanline 10, pixel 112
	CHECK((int)pixels[pixel_index] == Approx(21).margin(5));
	CHECK((int)pixels[pixel_index + 1] == Approx(191).margin(5));
	CHECK((int)pixels[pixel_index + 2] == Approx(0).margin(5));
	CHECK((int)pixels[pixel_index + 3] == Approx(255).margin(5));

	// Check pixel function
	CHECK(f->CheckPixel(10, 112, 21, 191, 0, 255, 5) == true);
	CHECK_FALSE(f->CheckPixel(10, 112, 0, 0, 0, 0, 5));

	// Get frame 1
	f = r.GetFrame(2);

	// Get the next frame
	pixels = f->GetPixels(10);
	pixel_index = 112 * 4; // pixel 112 (4 bytes per pixel)

	// Check image properties on scanline 10, pixel 112
	CHECK((int)pixels[pixel_index] == Approx(0).margin(5));
	CHECK((int)pixels[pixel_index + 1] == Approx(96).margin(5));
	CHECK((int)pixels[pixel_index + 2] == Approx(188).margin(5));
	CHECK((int)pixels[pixel_index + 3] == Approx(255).margin(5));

	// Check pixel function
	CHECK(f->CheckPixel(10, 112, 0, 96, 188, 255, 5) == true);
	CHECK_FALSE(f->CheckPixel(10, 112, 0, 0, 0, 0, 5));

	// Close reader
	r.Close();
}

TEST_CASE( "Max_Decode_Size_FFmpegReader", "[libopenshot][ffmpegreader]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "test.mp4";

	FFmpegReader r(path.str());
	r.SetMaxDecodeSize(64, 64);
	r.Open();

	std::shared_ptr<Frame> f = r.GetFrame(1);
	REQUIRE(f != nullptr);
	CHECK(f->GetWidth() <= 64);
	CHECK(f->GetHeight() <= 64);
	CHECK(f->GetWidth() < r.info.width);
	CHECK(f->GetHeight() < r.info.height);

	r.Close();
}

TEST_CASE( "Seek", "[libopenshot][ffmpegreader]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	// Get frame
	std::shared_ptr<Frame> f = r.GetFrame(1);
	CHECK(f->number == 1);

	// Get frame
	f = r.GetFrame(300);
	CHECK(f->number == 300);

	// Get frame
	f = r.GetFrame(301);
	CHECK(f->number == 301);

	// Get frame
	f = r.GetFrame(315);
	CHECK(f->number == 315);

	// Get frame
	f = r.GetFrame(275);
	CHECK(f->number == 275);

	// Get frame
	f = r.GetFrame(270);
	CHECK(f->number == 270);

	// Get frame
	f = r.GetFrame(500);
	CHECK(f->number == 500);

	// Get frame
	f = r.GetFrame(100);
	CHECK(f->number == 100);

	// Get frame
	f = r.GetFrame(600);
	CHECK(f->number == 600);

	// Get frame
	f = r.GetFrame(1);
	CHECK(f->number == 1);

	// Get frame
	f = r.GetFrame(700);
	CHECK(f->number == 700);

	// Close reader
	r.Close();

}

TEST_CASE( "Frame_Rate", "[libopenshot][ffmpegreader]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	// Verify detected frame rate
	openshot::Fraction rate = r.info.fps;
	CHECK(rate.num == 24);
	CHECK(rate.den == 1);

	r.Close();
}

TEST_CASE( "Duration_And_Length", "[libopenshot][ffmpegreader]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	// Duration and frame count should match (length derived from default Video_Preferred duration strategy)
	CHECK(r.info.video_length == 1253);
	CHECK(r.info.duration == Approx(52.208333f).margin(0.0005f));

	r.Close();
}

TEST_CASE( "Duration_Strategy_Video_Preferred", "[libopenshot][ffmpegreader]" )
{
	// Create a reader preferring video duration (then falling back to audio/format)
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str(), DurationStrategy::VideoPreferred);
	r.Open();

	// Video stream duration should win, but still fall back to others if missing
	CHECK(r.info.video_length == 1253);
	CHECK(r.info.duration == Approx(52.208333).margin(0.0005f));

	r.Close();

	// Audio-only file should fallback to its audio duration
	std::stringstream audio_path;
	audio_path << TEST_MEDIA_PATH << "piano.wav";
	FFmpegReader audio_reader(audio_path.str(), DurationStrategy::VideoPreferred);
	audio_reader.Open();

	CHECK(audio_reader.info.video_length == 132);
	CHECK(audio_reader.info.duration == Approx(4.4f).margin(0.001f));

	audio_reader.Close();
}

TEST_CASE( "Duration_Strategy_Longest_Stream", "[libopenshot][ffmpegreader]" )
{
	// Create a reader preferring the longest duration among streams/format
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str(), DurationStrategy::LongestStream);
	r.Open();

	CHECK(r.info.video_length == 1253);
	CHECK(r.info.duration == Approx(52.208333).margin(0.0005f));

	r.Close();

	// Audio-only file should resolve to the audio duration
	std::stringstream audio_path;
	audio_path << TEST_MEDIA_PATH << "piano.wav";
	FFmpegReader audio_reader(audio_path.str(), DurationStrategy::LongestStream);
	audio_reader.Open();

	CHECK(audio_reader.info.video_length == 132);
	CHECK(audio_reader.info.duration == Approx(4.4f).margin(0.001f));

	audio_reader.Close();
}


TEST_CASE( "Duration_Strategy_Audio_Preferred", "[libopenshot][ffmpegreader]" )
{
	// Create a reader preferring audio duration (then falling back to audio/format)
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str(), DurationStrategy::AudioPreferred);
	r.Open();

	// Audio stream duration should win, but still fall back to others if missing
	CHECK(r.info.video_length == 1247);
	CHECK(r.info.duration == Approx(51.958333).margin(0.0005f));

	r.Close();

	// Audio-only file should still resolve to the audio duration
	std::stringstream audio_path;
	audio_path << TEST_MEDIA_PATH << "piano.wav";
	FFmpegReader audio_reader(audio_path.str(), DurationStrategy::AudioPreferred);
	audio_reader.Open();

	CHECK(audio_reader.info.video_length == 132);
	CHECK(audio_reader.info.duration == Approx(4.4f).margin(0.001f));

	audio_reader.Close();
}

TEST_CASE( "GIF_TimeBase", "[libopenshot][ffmpegreader]" )
{
        // Create a reader
        std::stringstream path;
        path << TEST_MEDIA_PATH << "animation.gif";
        FFmpegReader r(path.str());
        r.Open();

        // Verify basic info
        CHECK(r.info.fps.num == 5);
        CHECK(r.info.fps.den == 1);
        CHECK(r.info.video_length == 20);
        CHECK(r.info.duration == Approx(4.0f).margin(0.01));

        auto frame_color = [](std::shared_ptr<Frame> f) {
                const unsigned char* row = f->GetPixels(25);
                return row[25 * 4];
        };
        auto expected_color = [](int frame) {
                return (frame - 1) * 10;
        };

        for (int i = 1; i <= r.info.video_length; ++i) {
                CHECK(frame_color(r.GetFrame(i)) == expected_color(i));
        }

        r.Close();
}

TEST_CASE("Close discards buffered decoder output", "[libopenshot][ffmpegreader]")
{
	FFmpegReader reader(std::string(TEST_MEDIA_PATH) + "sintel_trailer-720p.mp4");
	reader.Open();
	reader.GetFrame(1);
	const auto decoded = reader.packet_status.packets_decoded();
	REQUIRE(reader.packet_status.packets_read() > decoded);
	CHECK_NOTHROW(reader.Close());
	CHECK(reader.packet_status.packets_decoded() == decoded);
	CHECK(reader.pFormatCtx == nullptr);
	CHECK(reader.pCodecCtx == nullptr);
	CHECK(reader.aCodecCtx == nullptr);
	CHECK_NOTHROW(reader.Close());
	reader.Open();
	CHECK(reader.GetFrame(1)->number == 1);
	reader.Close();
}

TEST_CASE( "Multiple_Open_and_Close", "[libopenshot][ffmpegreader]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	// Get frame that requires a seek
	std::shared_ptr<Frame> f = r.GetFrame(1200);
	CHECK(f->number == 1200);

	// Close and Re-open the reader
	r.Close();
	r.Open();

	// Get frame
	f = r.GetFrame(1);
	CHECK(f->number == 1);
	f = r.GetFrame(250);
	CHECK(f->number == 250);

	// Close and Re-open the reader
	r.Close();
	r.Open();

	// Get frame
	f = r.GetFrame(750);
	CHECK(f->number == 750);
	f = r.GetFrame(1000);
	CHECK(f->number == 1000);

	// Close reader
	r.Close();
}

TEST_CASE( "Static_Image_PNG_Reports_Single_Image", "[libopenshot][ffmpegreader]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "front.png";
	FFmpegReader r(path.str(), DurationStrategy::VideoPreferred);
	r.Open();

	CHECK(r.info.has_video);
	CHECK_FALSE(r.info.has_audio);
	CHECK(r.info.has_single_image);
	CHECK(r.info.video_length > 1);
	CHECK(r.info.duration > 1000.0f);

	auto f1 = r.GetFrame(1);
	auto f2 = r.GetFrame(std::min(2, static_cast<int>(r.info.video_length)));
	CHECK(f1->CheckPixel(50, 50,
		f2->GetPixels(50)[50 * 4 + 0],
		f2->GetPixels(50)[50 * 4 + 1],
		f2->GetPixels(50)[50 * 4 + 2],
		f2->GetPixels(50)[50 * 4 + 3],
		0));

	r.Close();
}

TEST_CASE( "Static_Image_JPG_Reports_Single_Image", "[libopenshot][ffmpegreader]" )
{
	// Generate a JPG fixture at runtime from a known PNG frame.
	std::stringstream png_path;
	png_path << TEST_MEDIA_PATH << "front.png";
	FFmpegReader png_reader(png_path.str());
	png_reader.Open();

	auto png_frame = png_reader.GetFrame(1);
	std::srand(static_cast<unsigned int>(std::time(nullptr)));
	std::stringstream jpg_path;
	jpg_path << "libopenshot-static-image-test-" << std::rand() << ".jpg";
	REQUIRE(png_frame->GetImage()->save(jpg_path.str().c_str(), "JPG"));
	png_reader.Close();

	FFmpegReader jpg_reader(jpg_path.str(), DurationStrategy::VideoPreferred);
	jpg_reader.Open();

	CHECK(jpg_reader.info.has_video);
	CHECK_FALSE(jpg_reader.info.has_audio);
	CHECK(jpg_reader.info.has_single_image);
	CHECK(jpg_reader.info.video_length > 1);
	CHECK(jpg_reader.info.duration > 1000.0f);

	auto f1 = jpg_reader.GetFrame(1);
	auto f2 = jpg_reader.GetFrame(std::min(2, static_cast<int>(jpg_reader.info.video_length)));
	CHECK(f1->CheckPixel(50, 50,
		f2->GetPixels(50)[50 * 4 + 0],
		f2->GetPixels(50)[50 * 4 + 1],
		f2->GetPixels(50)[50 * 4 + 2],
		f2->GetPixels(50)[50 * 4 + 3],
		2));

	jpg_reader.Close();
	std::remove(jpg_path.str().c_str());
}

TEST_CASE( "Attached_Picture_Audio_Does_Not_Stall_Early_Frames", "[libopenshot][ffmpegreader]" )
{
	// Build a temporary fixture with audio + attached cover art at runtime.
	// This avoids adding another binary media file to the repository.
	if (std::system("ffmpeg -hide_banner -version >/dev/null 2>&1") != 0) {
		WARN("Skipping attached-picture test: ffmpeg executable not available");
		return;
	}

	std::srand(static_cast<unsigned int>(std::time(nullptr)));
	std::stringstream fixture_path;
	fixture_path << "libopenshot-attached-art-test-" << std::rand() << ".m4a";

	std::stringstream command;
	command << "ffmpeg -y -hide_banner -loglevel error "
	        << "-i \"" << TEST_MEDIA_PATH << "front.png\" "
	        << "-f lavfi -i \"anullsrc=r=44100:cl=stereo\" "
	        << "-t 2 "
	        << "-map 1:a:0 -map 0:v:0 "
	        << "-c:a aac -b:a 128k "
	        << "-c:v mjpeg -disposition:v:0 attached_pic "
	        << "\"" << fixture_path.str() << "\"";
	const int command_result = std::system(command.str().c_str());
	REQUIRE(command_result == 0);

	FFmpegReader r(fixture_path.str(), DurationStrategy::VideoPreferred);
	r.Open();

	CHECK(r.info.has_video);
	CHECK(r.info.has_audio);
	CHECK(r.info.has_single_image);

	auto f1 = r.GetFrame(1);
	CHECK(f1->has_image_data);
	CHECK(f1->GetAudioSamplesCount() > 0);

	const auto frame2_start = std::chrono::steady_clock::now();
	auto f2 = r.GetFrame(2);
	const auto frame2_end = std::chrono::steady_clock::now();
	const auto frame2_ms = std::chrono::duration_cast<std::chrono::milliseconds>(frame2_end - frame2_start).count();
	CHECK(frame2_ms < 1500);
	CHECK(f2->has_image_data);
	CHECK(f2->GetAudioSamplesCount() > 0);

	r.Close();
	std::remove(fixture_path.str().c_str());
}

TEST_CASE( "Missing_Image_Frame_Finalizes_Using_Previous_Image", "[libopenshot][ffmpegreader]" )
{
	FFmpegReader r("synthetic-missing-image", DurationStrategy::VideoPreferred, false);

	r.info.has_video = true;
	r.info.has_audio = true;
	r.info.has_single_image = false;
	r.info.width = 320;
	r.info.height = 240;
	r.info.fps = Fraction(30, 1);
	r.info.sample_rate = 48000;
	r.info.channels = 2;
	r.info.channel_layout = LAYOUT_STEREO;
	r.info.video_length = 120;
	r.info.video_timebase = Fraction(1, 30);
	r.info.audio_timebase = Fraction(1, 48000);

	r.pts_offset_seconds = 0.0;
	r.last_frame = 58;
	r.video_pts_seconds = 2.233333;
	r.audio_pts_seconds = 3.100000;
	r.packet_status.video_eof = false;
	r.packet_status.audio_eof = false;
	r.packet_status.packets_eof = false;
	r.packet_status.end_of_file = false;

	const int samples_per_frame = Frame::GetSamplesPerFrame(
		58, r.info.fps, r.info.sample_rate, r.info.channels);
	auto previous = std::make_shared<Frame>(
		58, r.info.width, r.info.height, "#112233", samples_per_frame, r.info.channels);
	previous->AddColor(r.info.width, r.info.height, "#112233");
	r.final_cache.Add(previous);
	r.last_final_video_frame = previous;

	auto missing = r.CreateFrame(59);
	r.working_cache.Add(missing);
	REQUIRE(missing != nullptr);
	REQUIRE_FALSE(missing->has_image_data);

	r.CheckWorkingFrames(59);

	auto finalized = r.final_cache.GetFrame(59);
	REQUIRE(finalized != nullptr);
	CHECK(finalized->has_image_data);
	CHECK(finalized->CheckPixel(0, 0, 17, 34, 51, 255, 0));
	CHECK(r.final_cache.GetFrame(58) != nullptr);
}

TEST_CASE( "HardwareDecodeSuccessful_IsFalse_WhenHardwareDecodeIsDisabled", "[libopenshot][ffmpegreader][hardware]" )
{
	HardwareDecoderSettingsGuard guard;
	Settings::Instance()->HARDWARE_DECODER = 0;
	Settings::Instance()->HW_DE_DEVICE_SET = 0;

	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str(), DurationStrategy::VideoPreferred);
	r.Open();

	REQUIRE(r.info.has_video);
	auto frame = r.GetFrame(1);
	REQUIRE(frame->has_image_data);
	CHECK_FALSE(r.HardwareDecodeSuccessful());

	r.Close();
}

TEST_CASE( "VAAPI_H264_420_Reports_HardwareDecodeSuccess", "[libopenshot][ffmpegreader][hardware]" )
{
#if !defined(__linux__) || !USE_HW_ACCEL
	WARN("Skipping hardware decode success test: requires Linux build with hardware decode support");
	return;
#else
	if (std::system("ffmpeg -hide_banner -version >/dev/null 2>&1") != 0) {
		WARN("Skipping hardware decode success test: ffmpeg executable not available");
		return;
	}
	if (std::system("ffmpeg -hide_banner -hwaccels 2>/dev/null | grep -q '\\<vaapi\\>'") != 0) {
		WARN("Skipping hardware decode success test: ffmpeg does not report VAAPI support");
		return;
	}
	if (std::system("sh -c 'test -e /dev/dri/renderD128 -o -e /dev/dri/renderD129 -o -e /dev/dri/renderD130' >/dev/null 2>&1") != 0) {
		WARN("Skipping hardware decode success test: no render node available under /dev/dri");
		return;
	}

	std::srand(static_cast<unsigned int>(std::time(nullptr)));
	std::stringstream fixture_path;
	fixture_path << "libopenshot-vaapi-420-test-" << std::rand() << ".mp4";
	TemporaryFileGuard fixture_cleanup(fixture_path.str());

	std::stringstream command;
	command << "ffmpeg -y -hide_banner -loglevel error "
	        << "-f lavfi -i \"testsrc2=size=128x72:rate=30\" "
	        << "-t 1 "
	        << "-c:v libx264 "
	        << "-pix_fmt yuv420p "
	        << "-profile:v high "
	        << "\"" << fixture_path.str() << "\"";
	const int command_result = std::system(command.str().c_str());
	REQUIRE(command_result == 0);

	HardwareDecoderSettingsGuard hw_guard;
	Settings::Instance()->HARDWARE_DECODER = 1;
	Settings::Instance()->HW_DE_DEVICE_SET = 0;

	FFmpegReader r(fixture_path.str(), DurationStrategy::VideoPreferred);
	r.Open();

	REQUIRE(r.info.has_video);
	auto frame = r.GetFrame(1);
	REQUIRE(frame->has_image_data);
	CHECK(r.HardwareDecodeSuccessful());

	r.Close();
#endif
}

TEST_CASE( "VAAPI_H264_422_Does_Not_Return_Black_Frames", "[libopenshot][ffmpegreader][hardware]" )
{
#if !defined(__linux__) || !USE_HW_ACCEL
	WARN("Skipping VAAPI regression test: requires Linux build with hardware decode support");
	return;
#else
	if (std::system("ffmpeg -hide_banner -version >/dev/null 2>&1") != 0) {
		WARN("Skipping VAAPI regression test: ffmpeg executable not available");
		return;
	}
	if (std::system("ffmpeg -hide_banner -hwaccels 2>/dev/null | grep -q '\\<vaapi\\>'") != 0) {
		WARN("Skipping VAAPI regression test: ffmpeg does not report VAAPI support");
		return;
	}
	if (std::system("sh -c 'test -e /dev/dri/renderD128 -o -e /dev/dri/renderD129 -o -e /dev/dri/renderD130' >/dev/null 2>&1") != 0) {
		WARN("Skipping VAAPI regression test: no render node available under /dev/dri");
		return;
	}

	std::srand(static_cast<unsigned int>(std::time(nullptr)));
	std::stringstream fixture_path;
	fixture_path << "libopenshot-vaapi-422-test-" << std::rand() << ".mp4";
	TemporaryFileGuard fixture_cleanup(fixture_path.str());

	std::stringstream command;
	command << "ffmpeg -y -hide_banner -loglevel error "
	        << "-f lavfi -i \"testsrc2=size=128x72:rate=30\" "
	        << "-t 1 "
	        << "-c:v libx264 "
	        << "-pix_fmt yuvj422p "
	        << "-profile:v high422 "
	        << "-color_range pc "
	        << "\"" << fixture_path.str() << "\"";
	const int command_result = std::system(command.str().c_str());
	REQUIRE(command_result == 0);

	HardwareDecoderSettingsGuard guard;
	Settings::Instance()->HARDWARE_DECODER = 1;
	Settings::Instance()->HW_DE_DEVICE_SET = 0;

	FFmpegReader r(fixture_path.str(), DurationStrategy::VideoPreferred);
	r.Open();

	REQUIRE(r.info.has_video);
	REQUIRE(r.info.video_length >= 3);

	const std::array<int64_t, 3> frames_to_check = {1, r.info.video_length / 2, r.info.video_length};
	for (const int64_t frame_number : frames_to_check) {
		auto frame = r.GetFrame(frame_number);
		REQUIRE(frame->has_image_data);
		INFO("frame=" << frame_number << ", avg_luma=" << SampleAverageLuma(frame));
		CHECK(SampleAverageLuma(frame) > 8.0);
	}

	r.Close();
#endif
}

TEST_CASE( "verify parent Timeline", "[libopenshot][ffmpegreader]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	// Check size of frame image
	CHECK(r.GetFrame(1)->GetImage()->width() == 1280);
	CHECK(r.GetFrame(1)->GetImage()->height() == 720);
	r.GetFrame(1)->GetImage()->save("reader-1.png", "PNG");

	// Create a Clip associated with this reader
	Clip c1(&r);
	c1.Open();

	// Check size of frame image (should still be the same)
	CHECK(r.GetFrame(1)->GetImage()->width() == 1280);
	CHECK(r.GetFrame(1)->GetImage()->height() == 720);

	// Create Timeline
	Timeline t1(640, 480, Fraction(30,1), 44100, 2, LAYOUT_STEREO);
	t1.AddClip(&c1);

	// Check size of frame image (it should now match the parent timeline)
	CHECK(r.GetFrame(1)->GetImage()->width() == 640);
	CHECK(r.GetFrame(1)->GetImage()->height() == 360);

	c1.Close();
	t1.Close();
}

TEST_CASE( "DisplayInfo", "[libopenshot][ffmpegreader]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	std::string expected(R"(----------------------------
----- File Information -----
----------------------------
--> Has Video: true
--> Has Audio: true
--> Has Single Image: false
--> Duration: 52.21 Seconds
--> File Size: 7.26 MB
----------------------------
----- Video Attributes -----
----------------------------
--> Width: 1280
--> Height: 720)");

	// Store the DisplayInfo() text in 'output'
	std::stringstream output;
	r.DisplayInfo(&output);

	// Compare a [0, expected.size()) substring of output to expected
	CHECK(output.str().substr(0, expected.size()) == expected);
}

TEST_CASE( "Decoding AV1 Video", "[libopenshot][ffmpegreader]" )
{
	try {
		// Create a reader
		std::stringstream path;
		path << TEST_MEDIA_PATH << "test_video_sync.mp4";
		FFmpegReader r(path.str());
		r.Open();

		std::shared_ptr<Frame> f = r.GetFrame(1);

		// Get the image data
		const unsigned char *pixels = f->GetPixels(10);
		int pixel_index = 112 * 4;

		// Check image properties on scanline 10, pixel 112
		CHECK((int) pixels[pixel_index] == Approx(0).margin(5));
		CHECK((int) pixels[pixel_index + 1] == Approx(0).margin(5));
		CHECK((int) pixels[pixel_index + 2] == Approx(0).margin(5));
		CHECK((int) pixels[pixel_index + 3] == Approx(255).margin(5));

		f = r.GetFrame(90);

		// Get the image data
		pixels = f->GetPixels(820);
		pixel_index = 930 * 4;

		// Check image properties on scanline 820, pixel 930
		CHECK((int) pixels[pixel_index] == Approx(255).margin(5));
		CHECK((int) pixels[pixel_index + 1] == Approx(255).margin(5));
		CHECK((int) pixels[pixel_index + 2] == Approx(255).margin(5));
		CHECK((int) pixels[pixel_index + 3] == Approx(255).margin(5));

		f = r.GetFrame(160);

		// Get the image data
		pixels = f->GetPixels(420);
		pixel_index = 930 * 4;

		// Check image properties on scanline 820, pixel 930
		CHECK((int) pixels[pixel_index] == Approx(255).margin(5));
		CHECK((int) pixels[pixel_index + 1] == Approx(255).margin(5));
		CHECK((int) pixels[pixel_index + 2] == Approx(255).margin(5));
		CHECK((int) pixels[pixel_index + 3] == Approx(255).margin(5));

		f = r.GetFrame(240);

		// Get the image data
		pixels = f->GetPixels(624);
		pixel_index = 930 * 4;

		// Check image properties on scanline 820, pixel 930
		CHECK((int) pixels[pixel_index] == Approx(255).margin(5));
		CHECK((int) pixels[pixel_index + 1] == Approx(255).margin(5));
		CHECK((int) pixels[pixel_index + 2] == Approx(255).margin(5));
		CHECK((int) pixels[pixel_index + 3] == Approx(255).margin(5));

		// Close reader
		r.Close();

	} catch (const InvalidCodec & e) {
		// Ignore older FFmpeg versions which don't support AV1
	} catch (const InvalidFile & e) {
		// Ignore older FFmpeg versions which don't support AV1
	}
}
