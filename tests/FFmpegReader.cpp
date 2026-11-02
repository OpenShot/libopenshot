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
#include <future>
#include <thread>

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

#if defined(__linux__) && USE_HW_ACCEL
bool VaapiDeviceAvailableForTest()
{
	// These tests select HW_DE_DEVICE_SET=0. A render node can exist on an
	// NVIDIA machine (or be inaccessible) without providing a usable VAAPI
	// device. Probe with the same FFmpeg library and adapter as the reader.
	AVBufferRef* device = nullptr;
	const int result = av_hwdevice_ctx_create(
		&device, AV_HWDEVICE_TYPE_VAAPI, "/dev/dri/renderD128", nullptr, 0);
	av_buffer_unref(&device);
	if (result < 0) {
		char error[AV_ERROR_MAX_STRING_SIZE] = {};
		av_strerror(result, error, sizeof(error));
		WARN("Skipping VAAPI test: cannot initialize /dev/dri/renderD128: " << error);
		return false;
	}
	return true;
}
#endif

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

TEST_CASE("Queued reader lifecycle calls recheck state after locking",
          "[libopenshot][ffmpegreader][lifecycle]")
{
	class LockedReader : public FFmpegReader {
	public:
		using FFmpegReader::FFmpegReader;
		using ReaderBase::getFrameMutex;
	};
	LockedReader reader(std::string(TEST_MEDIA_PATH) + "sintel_trailer-720p.mp4");
	const bool initially_open = GENERATE(false, true);
	const bool closing = GENERATE(false, true);
	CAPTURE(initially_open, closing);
	if (initially_open) reader.Open();

	// Hold the lifecycle lock while starting the worker. Exercise
	// Open/Open, Close/Close, and both mixed call orders.
	std::unique_lock<std::recursive_mutex> lock(reader.getFrameMutex);
	std::promise<void> started, finished;
	auto done = finished.get_future();
	std::exception_ptr error;
	std::thread pending([&] {
		started.set_value();
		try {
			if (closing) reader.Close(); else reader.Open();
		} catch (...) { error = std::current_exception(); }
		finished.set_value();
	});
	started.get_future().wait();
	// Give the worker time to reach the held mutex. This exercises contention,
	// but cannot guarantee scheduling on every platform.
	const bool waited = done.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout;
	std::exception_ptr foreground_error;
	try {
		if (initially_open) reader.Close(); else reader.Open();
	} catch (...) { foreground_error = std::current_exception(); }
	AVFormatContext* context = reader.pFormatCtx;
	lock.unlock();
	pending.join();

	REQUIRE(waited);
	REQUIRE(foreground_error == nullptr);
	REQUIRE(error == nullptr);
	CHECK(reader.IsOpen() == !closing);
	// A queued Open must reuse the existing context, not leak it and reopen.
	if (closing == initially_open) CHECK(reader.pFormatCtx == context);
	CHECK_NOTHROW(reader.Close());
	CHECK_NOTHROW(reader.Close());
	reader.Open();
	CHECK(reader.GetFrame(1)->GetWidth() > 0);
	CHECK(reader.GetFrame(30)->GetWidth() > 0);
	CHECK(reader.GetFrame(1)->GetWidth() > 0);
	reader.Close();
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

TEST_CASE("Reduced decoded frames have aligned rows", "[libopenshot][ffmpegreader][preview-size]")
{
	for (const QSize bounds : {QSize(638, 359), QSize(640, 352), QSize(1280, 359),
		QSize(638, 720), QSize(1, 1)}) {
		CAPTURE(bounds.width(), bounds.height());
		FFmpegReader reader(std::string(TEST_MEDIA_PATH) + "sintel_trailer-720p.mp4");
		reader.SetMaxDecodeSize(bounds.width(), bounds.height());
		reader.Open();
		auto frame = reader.GetFrame(1);
		REQUIRE(frame != nullptr);
		CHECK(frame->GetWidth() % 4 == 0);
		CHECK(frame->GetHeight() % 4 == 0);
		CHECK(frame->GetWidth() >= 4);
		CHECK(frame->GetHeight() >= 4);
		CHECK(frame->GetWidth() <= std::max(4, bounds.width()));
		CHECK(frame->GetHeight() <= std::max(4, bounds.height()));
		CHECK(frame->GetImage()->bytesPerLine() % 16 == 0);
		CHECK(reinterpret_cast<uintptr_t>(frame->GetPixels()) % 16 == 0);
		reader.Close();
	}
}

TEST_CASE("Clip fitting cannot undo preview alignment", "[libopenshot][ffmpegreader][preview-size]")
{
	FFmpegReader reader(std::string(TEST_MEDIA_PATH) + "sintel_trailer-720p.mp4");
	reader.Open();
	Clip clip(&reader);
	// A 16:9 source in a portrait preview is fitted again by the reader.
	Timeline timeline(1080, 1920, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
	timeline.SetMaxSize(333, 591);
	timeline.AddClip(&clip);
	clip.scale_x = Keyframe(1.13);
	clip.scale_y = Keyframe(1.07);
	clip.Open();
	auto frame = reader.GetFrame(1);
	CHECK(frame->GetWidth() < reader.info.width);
	CHECK(frame->GetWidth() % 4 == 0);
	CHECK(frame->GetHeight() % 4 == 0);
	CHECK(frame->GetImage()->bytesPerLine() % 16 == 0);
	clip.Close();
	timeline.Close();
}

TEST_CASE("Quarter-turn previews retain aligned rows", "[libopenshot][ffmpegreader][preview-size]")
{
	FFmpegReader reader(std::string(TEST_MEDIA_PATH) + "sintel_trailer-720p.mp4");
	reader.SetMaxDecodeSize(359, 638);
	reader.ApplyOrientationMetadata(true);
	reader.Open();
	// Model the display orientation discovered from container metadata.
	reader.source_rotation = 90;
	std::swap(reader.info.width, reader.info.height);
	auto frame = reader.GetFrame(1);
	CHECK(frame->GetWidth() <= 359);
	CHECK(frame->GetHeight() <= 638);
	CHECK(frame->GetHeight() > frame->GetWidth());
	CHECK(frame->GetWidth() % 4 == 0);
	CHECK(frame->GetHeight() % 4 == 0);
	CHECK(frame->GetImage()->bytesPerLine() % 16 == 0);
	reader.Close();
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

TEST_CASE("Initial missing pictures wait for audio but not EOF", "[libopenshot][ffmpegreader][initial-gap]")
{
	FFmpegReader r("synthetic-initial-gap", DurationStrategy::VideoPreferred, false);
	r.info.has_video = true;
	r.info.has_audio = true;
	r.info.has_single_image = false;
	r.info.width = 160;
	r.info.height = 90;
	r.info.fps = Fraction(30, 1);
	r.info.sample_rate = 48000;
	r.info.channels = 1;
	r.info.channel_layout = LAYOUT_MONO;
	r.pts_offset_seconds = 0.0;
	r.packet_status.reset(false);
	r.video_pts_seconds = 0.5;
	r.audio_pts_seconds = 1.1;
	r.first_video_frame = 16;
	auto frame = r.CreateFrame(1);
	const float sample = 0.25f;
	frame->AddAudio(true, 0, 0, &sample, 1, 1.0f);

	SECTION("Initial gap becomes black without changing its audio") {
		r.CheckWorkingFrames(1);
		auto result = r.final_cache.GetFrame(1);
		REQUIRE(result != nullptr);
		CHECK(result->CheckPixel(0, 0, 0, 0, 0, 255, 0));
		CHECK(result->GetAudioSamples(0)[0] == sample);
		CHECK_FALSE(r.packet_status.end_of_file);
	}
	SECTION("Earlier audio packets alone do not make the frame ready") {
		r.audio_pts_seconds = -0.02;
		r.CheckWorkingFrames(1);
		CHECK(r.final_cache.GetFrame(1) == nullptr);
		CHECK_FALSE(frame->has_image_data);
	}
	SECTION("Video-only initial gaps need no audio wait") {
		r.info.has_audio = false;
		r.audio_pts_seconds = -0.02;
		r.CheckWorkingFrames(1);
		REQUIRE(r.final_cache.GetFrame(1) != nullptr);
		CHECK(r.final_cache.GetFrame(1)->CheckPixel(0, 0, 0, 0, 0, 255, 0));
	}
	SECTION("Wait for actual decoded output, not a later packet timestamp") {
		r.first_video_frame = 0;
		r.CheckWorkingFrames(1);
		CHECK(r.final_cache.GetFrame(1) == nullptr);
	}
	SECTION("A missing picture after the first decoded picture is not an initial gap") {
		r.first_video_frame = 1;
		r.CheckWorkingFrames(1);
		CHECK(r.final_cache.GetFrame(1) == nullptr);
	}
	SECTION("A completed mid-file seek cannot establish an initial gap") {
		r.first_video_frame = -1;
		r.CheckWorkingFrames(1);
		CHECK(r.final_cache.GetFrame(1) == nullptr);
	}
	SECTION("A seek in progress cannot establish an initial gap") {
		r.is_seeking = true;
		r.CheckWorkingFrames(1);
		CHECK(r.final_cache.GetFrame(1) == nullptr);
	}
}

TEST_CASE("Initial video gap preserves decoded pictures and survives seeking", "[libopenshot][ffmpegreader][initial-gap]")
{
	if (std::system("ffmpeg -hide_banner -version >/dev/null 2>&1") != 0) {
		WARN("Skipping initial-gap fixture: ffmpeg executable not available");
		return;
	}
	const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	TemporaryFileGuard source("initial-gap-source-" + suffix + ".mp4");
	TemporaryFileGuard gap("initial-gap-" + suffix + ".mkv");
	const std::string encode = "ffmpeg -y -hide_banner -loglevel error "
		"-f lavfi -i testsrc2=size=160x90:rate=30 "
		"-f lavfi -i sine=frequency=440:sample_rate=48000 -t 5 "
		"-c:v libx264 -g 30 -keyint_min 30 -sc_threshold 0 -bf 2 -c:a aac " + source.path;
	REQUIRE(std::system(encode.c_str()) == 0);
	// Preserve non-key packets at the start: decoding cannot produce a picture
	// until the next keyframe. Audio starts earlier, and B-frames require reordering.
	const std::string trim = "ffmpeg -y -hide_banner -loglevel error -i " + source.path
		+ " -ss 0.5 -c copy -copyinkf -avoid_negative_ts disabled " + gap.path;
	REQUIRE(std::system(trim.c_str()) == 0);
	HardwareDecoderSettingsGuard guard;
	Settings::Instance()->HARDWARE_DECODER = 0;
	FFmpegReader normal(source.path);
	normal.Open();
	auto normal_first = normal.GetFrame(1);
	CHECK(normal.first_video_frame == 1);
	CHECK(SampleAverageLuma(normal_first) > 10.0);

	FFmpegReader r(gap.path);
	r.Open();
	CHECK(r.aStream->start_time * av_q2d(r.aStream->time_base)
		< r.pStream->start_time * av_q2d(r.pStream->time_base));
	auto first = r.GetFrame(1);
	REQUIRE(first != nullptr);
	CHECK(first->CheckPixel(0, 0, 0, 0, 0, 255, 0));
	CHECK_FALSE(r.packet_status.packets_eof);
	CHECK(r.packet_status.video_decoded < 60);
	const int64_t first_picture = r.first_video_frame;
	REQUIRE(first_picture > 1);
	REQUIRE(first_picture < 30);
	REQUIRE(first->GetAudioSamplesCount() > 0);
	float peak_audio = 0.0f;
	for (int i = 0; i < first->GetAudioSamplesCount(); ++i)
		peak_audio = std::max(peak_audio, std::abs(first->GetAudioSamples(0)[i]));
	CHECK(peak_audio > 0.01f);
	for (int64_t n = 2; n < first_picture; ++n)
		CHECK(r.GetFrame(n)->CheckPixel(0, 0, 0, 0, 0, 255, 0));
	auto picture = r.GetFrame(first_picture);
	CHECK(SampleAverageLuma(picture) > 10.0);
	// Stream copy preserves the original picture at 1 second (frame 31).
	CHECK(*picture->GetImage() == *normal.GetFrame(31)->GetImage());
	r.GetFrame(100);
	CHECK(r.first_video_frame == -1); // A mid-file seek must not define a startup gap.
	r.GetCache()->Clear();
	CHECK(r.GetFrame(1)->CheckPixel(0, 0, 0, 0, 0, 255, 0));
	CHECK_FALSE(r.packet_status.packets_eof);
	CHECK(*r.GetFrame(first_picture)->GetImage() == *picture->GetImage());
	r.Close();
	CHECK(r.first_video_frame == 0);
	r.Open();
	CHECK(r.GetFrame(1)->CheckPixel(0, 0, 0, 0, 0, 255, 0));
	r.Close();
	normal.Close();
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
	WARN("Skipping VAAPI test: requires Linux build with hardware decode support");
	return;
#else
	if (std::system("ffmpeg -hide_banner -version >/dev/null 2>&1") != 0) {
		WARN("Skipping VAAPI test: ffmpeg executable not available");
		return;
	}
	if (!VaapiDeviceAvailableForTest()) {
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
	WARN("Skipping VAAPI test: requires Linux build with hardware decode support");
	return;
#else
	if (std::system("ffmpeg -hide_banner -version >/dev/null 2>&1") != 0) {
		WARN("Skipping VAAPI test: ffmpeg executable not available");
		return;
	}
	if (!VaapiDeviceAvailableForTest()) {
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


TEST_CASE("HDR color metadata is exposed in reader JSON",
          "[libopenshot][ffmpegreader][hdr]")
{
    if (std::system("ffmpeg -hide_banner -version >/dev/null 2>&1") != 0) {
        WARN("Skipping HDR metadata test: ffmpeg executable unavailable");
        return;
    }

    if (std::system(
            "ffmpeg -hide_banner -encoders 2>/dev/null | grep -q libx265"
        ) != 0) {
        WARN("Skipping HDR metadata test: libx265 encoder unavailable");
        return;
    }

    struct ColorFixture {
        const char* name;
        const char* primaries;
        const char* transfer;
        const char* matrix;
        const char* hdr;
    };

    const ColorFixture fixtures[] = {
        {"pq",  "bt2020", "smpte2084",    "bt2020nc", "PQ"},
        {"hlg", "bt2020", "arib-std-b67", "bt2020nc", "HLG"},
        {"sdr", "bt709",  "bt709",        "bt709",    nullptr}
    };

    for (const auto& fixture : fixtures) {
        DYNAMIC_SECTION("Color signaling: " << fixture.name) {
            std::stringstream filename;
            filename << "libopenshot-hdr-"
                     << fixture.name << "-"
                     << std::time(nullptr) << "-"
                     << std::rand() << ".mp4";

            TemporaryFileGuard cleanup(filename.str());

            std::stringstream command;
            command << "ffmpeg -y -hide_banner -loglevel error "
                    << "-f lavfi -i color=c=gray:s=64x64:r=1 "
                    << "-frames:v 1 "
                    << "-c:v libx265 "
                    << "-pix_fmt yuv420p10le "
                    << "-threads 1 "
                    << "-x265-params "
                    << "log-level=error:pools=1:frame-threads=1:"
                    << "colorprim=" << fixture.primaries << ":"
                    << "transfer=" << fixture.transfer << ":"
                    << "colormatrix=" << fixture.matrix << " "
                    << "-color_primaries " << fixture.primaries << " "
                    << "-color_trc " << fixture.transfer << " "
                    << "-colorspace " << fixture.matrix << " "
                    << "\"" << filename.str() << "\" "
                    << ">/dev/null 2>&1";

            REQUIRE(std::system(command.str().c_str()) == 0);

            FFmpegReader reader(filename.str());
            reader.Open();

            const auto& metadata = reader.info.metadata;

            REQUIRE(metadata.at("color_primaries") == fixture.primaries);
            REQUIRE(metadata.at("color_transfer") == fixture.transfer);
            REQUIRE(metadata.at("color_space") == fixture.matrix);
            REQUIRE(metadata.at("bit_depth") == "10");

            if (fixture.hdr) {
                REQUIRE(metadata.at("hdr_transfer") == fixture.hdr);
            } else {
                CHECK(metadata.count("hdr_transfer") == 0);
            }

            const Json::Value json = reader.JsonValue();
            REQUIRE(json["metadata"]["color_primaries"].asString()
                    == fixture.primaries);
            REQUIRE(json["metadata"]["color_transfer"].asString()
                    == fixture.transfer);

            reader.Close();
        }
    }
}
