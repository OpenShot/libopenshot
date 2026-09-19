/**
 * @file
 * @brief Unit tests for openshot::FFmpegWriter
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <sstream>
#include <memory>
#include <fstream>
#include <QTemporaryDir>

#include "openshot_catch.h"

#include "FFmpegWriter.h"
#include "Exceptions.h"
#include "DummyReader.h"
#include "FFmpegReader.h"
#include "Fraction.h"
#include "Frame.h"
#include "Timeline.h"

extern "C" {
	#include <libavformat/avformat.h>
}

using namespace std;
using namespace openshot;

namespace {
AVStream* first_video_stream(AVFormatContext* format_context)
{
	for (unsigned int index = 0; index < format_context->nb_streams; ++index) {
		AVStream* stream = format_context->streams[index];
		if (stream && stream->codecpar && stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
			return stream;
	}
	return nullptr;
}
}

TEST_CASE("Raw video export preserves all color planes and frame ownership",
          "[libopenshot][ffmpegwriter][rawvideo]")
{
	QTemporaryDir directory;
	REQUIRE(directory.isValid());
	// NUT uses a different stream time base from the codec's frame rate.
	const auto filename = GENERATE("raw.avi", "raw.nut");
	const std::string path = directory.filePath(filename).toStdString();
	// Multiple frames and repeated exports exercise packet/frame cleanup.
	for (int pass = 0; pass < 2; ++pass) {
		FFmpegWriter writer(path);
		writer.SetVideoOptions(true, "rawvideo", Fraction(30, 1), 64, 64,
		                       Fraction(1, 1), false, false, 1000000);
		writer.Open();
		for (int number = 1; number <= 3; ++number) {
			auto frame = std::make_shared<Frame>(number, 64, 64, number == 2 ? "blue" : "red");
			writer.WriteFrame(frame);
		}
		writer.Close();

		AVFormatContext* input = nullptr;
		REQUIRE(avformat_open_input(&input, path.c_str(), nullptr, nullptr) == 0);
		std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)> input_guard(
			input, [](AVFormatContext* context) { avformat_close_input(&context); });
		REQUIRE(avformat_find_stream_info(input, nullptr) >= 0);
		AVStream* stream = first_video_stream(input);
		REQUIRE(stream != nullptr);
		AVPacket packet = {};
		int packets = 0;
		while (av_read_frame(input, &packet) >= 0) {
			if (packet.stream_index == stream->index) {
				CHECK(packet.size == 64 * 64 * 3 / 2); // Complete YUV420P image
				CHECK(packet.pts * av_q2d(stream->time_base) == Approx(packets / 30.0).margin(0.00001));
				++packets;
			}
			av_packet_unref(&packet);
		}
		CHECK(packets == 3);
		input_guard.reset();

		// NUT's reported duration omits the final frame interval in this FFmpeg
		// version; verify its packets above and use AVI for reader round trips.
		if (std::string(filename) == "raw.nut") continue;
		FFmpegReader reader(path);
		reader.Open();
		CHECK(reader.info.video_length == 3);
		for (int number = 1; number <= 3; ++number) {
			auto frame = reader.GetFrame(number);
			REQUIRE(frame->GetWidth() == 64);
			REQUIRE(frame->GetHeight() == 64);
			const QColor color = frame->GetImage()->pixelColor(32, 32);
			CHECK(color.green() < 10);
			CHECK(color.red() == Approx(number == 2 ? 0 : 255).margin(10));
			CHECK(color.blue() == Approx(number == 2 ? 255 : 0).margin(10));
		}
		reader.Close();
	}
}

TEST_CASE( "Webm", "[libopenshot][ffmpegwriter]" )
{
	// Reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	/* WRITER ---------------- */
	FFmpegWriter w("Webm-output1.webm");

	// Set options
	w.SetAudioOptions(true, "libvorbis", 44100, 2, LAYOUT_STEREO, 188000);
	w.SetVideoOptions(true, "libvpx", Fraction(24,1), 1280, 720, Fraction(1,1), false, false, 30000000);

	// Open writer
	w.Open();

	// Write some frames
	w.WriteFrame(&r, 24, 50);

	// Close writer & reader
	w.Close();
	r.Close();

	FFmpegReader r1("Webm-output1.webm");
	r1.Open();

	// Verify various settings on new MP4
	CHECK(r1.GetFrame(1)->GetAudioChannelsCount() == 2);
	CHECK(r1.info.fps.num == 24);
	CHECK(r1.info.fps.den == 1);

	// Get a specific frame
	std::shared_ptr<Frame> f = r1.GetFrame(8);

	// Get the image data for row 500
	const unsigned char* pixels = f->GetPixels(500);
	int pixel_index = 112 * 4; // pixel 112 (4 bytes per pixel)

	// Check image properties on scanline 10, pixel 112
	CHECK((int)pixels[pixel_index] == Approx(23).margin(7));
	CHECK((int)pixels[pixel_index + 1] == Approx(23).margin(7));
	CHECK((int)pixels[pixel_index + 2] == Approx(23).margin(7));
	CHECK((int)pixels[pixel_index + 3] == Approx(255).margin(7));
}

TEST_CASE( "Options_Overloads", "[libopenshot][ffmpegwriter]" )
{
	// Reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	/* WRITER ---------------- */
	FFmpegWriter w("Options_Overloads-output1.mp4");

	// Set options
	w.SetAudioOptions("aac", 48000, 192000);
	w.SetVideoOptions("libx264", 1280, 720, Fraction(30,1), 5000000);

	// Open writer
	w.Open();

	// Write some frames
	w.WriteFrame(&r, 24, 50);

	// Close writer & reader
	w.Close();
	r.Close();

	FFmpegReader r1("Options_Overloads-output1.mp4");
	r1.Open();

	// Verify implied settings
	CHECK(r1.info.has_audio == true);
	CHECK(r1.info.has_video == true);

	CHECK(r1.GetFrame(1)->GetAudioChannelsCount() == 2);
	CHECK(r1.info.channel_layout == LAYOUT_STEREO);

	CHECK(r1.info.pixel_ratio.num == 1);
	CHECK(r1.info.pixel_ratio.den == 1);
	CHECK_FALSE(r1.info.interlaced_frame);
	CHECK(r1.info.top_field_first == true);
}


TEST_CASE( "DisplayInfo", "[libopenshot][ffmpegwriter]" )
{
	// Reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	/* WRITER ---------------- */
	FFmpegWriter w("DisplayInfo-output1.webm");

	// Set options
	w.SetAudioOptions(true, "libvorbis", 44100, 2, LAYOUT_STEREO, 188000);
	w.SetVideoOptions(
		true, "libvpx",
		Fraction(24,1),
		1280, 720,
		Fraction(1,1),
		false, false,
		30000000);

	// Open writer
	w.Open();

	std::string expected(
		R"(----------------------------
----- File Information -----
----------------------------
--> Has Video: true
--> Has Audio: true
--> Has Single Image: false
--> Duration: 0.00 Seconds
--> File Size: 0.00 MB
----------------------------
----- Video Attributes -----
----------------------------
--> Width: 1280
--> Height: 720
--> Pixel Format: -1
--> Frames Per Second: 24.00 (24/1)
--> Video Bit Rate: 30000 kb/s
--> Pixel Ratio: 1.00 (1/1)
--> Display Aspect Ratio: 1.78 (16/9)
--> Video Codec: libvpx
--> Video Length: 0 Frames
--> Video Stream Index: -1
--> Video Timebase: 0.04 (1/24)
--> Interlaced: false
--> Interlaced: Top Field First: false
----------------------------
----- Audio Attributes -----
----------------------------
--> Audio Codec: libvorbis
--> Audio Bit Rate: 188 kb/s
--> Sample Rate: 44100 Hz
--> # of Channels: 2
--> Channel Layout: 3
--> Audio Stream Index: -1
--> Audio Timebase: 1.00 (1/1)
----------------------------)");

	// Store the DisplayInfo() text in 'output'
	std::stringstream output;
	w.DisplayInfo(&output);

	w.Close();

	// Compare a [0, expected.size()) substring of output to expected
	CHECK(output.str().substr(0, expected.size()) == expected);
}

TEST_CASE( "Gif", "[libopenshot][ffmpegwriter]" )
{
    // Reader
    std::stringstream path;
    path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";

    // Create Gif Clip
    Clip clip_video(path.str());
    clip_video.Layer(0);
    clip_video.Position(0.0);
    clip_video.Open();

    // Create Timeline w/ 1 Gif Clip (with 0 sample rate, and 0 channels)
    openshot::Timeline t(1280, 720, Fraction(30,1), 0, 0, LAYOUT_MONO);
    t.AddClip(&clip_video);
    t.Open();

    /* WRITER ---------------- */
    FFmpegWriter w("Gif-output1.gif");

    // Set options (no audio options are set)
    w.SetVideoOptions(true, "gif", Fraction(24,1), 1280, 720, Fraction(1,1), false, false, 15000000);

    // Create streams
    w.PrepareStreams();

    // Open writer
    w.Open();

    // Write some frames
    w.WriteFrame(&t, 1, 60);

    // Close writer & reader
    w.Close();
    t.Close();

    FFmpegReader r1("Gif-output1.gif");
    r1.Open();

    // Verify various settings on new Gif
    CHECK(r1.GetFrame(1)->GetAudioChannelsCount() == 0);
    CHECK(r1.GetFrame(1)->GetAudioSamplesCount() == 0);
    CHECK(r1.info.fps.num == 24);
    CHECK(r1.info.fps.den == 1);

    // Close reader
    r1.Close();
}

TEST_CASE( "MP4_30fps_duration_exact_with_b_frames", "[libopenshot][ffmpegwriter][fps]" )
{
	const std::string out_name = "MP4_30fps_duration_exact_with_b_frames.mp4";
	DummyReader reader(Fraction(30, 1), 1280, 720, 48000, 2, 1.0f);
	reader.Open();

	FFmpegWriter writer(out_name);
	writer.SetVideoOptions(true, "mpeg4", Fraction(30, 1), 1280, 720, Fraction(1, 1), false, false, 15000000);
	writer.Open();
	writer.WriteFrame(&reader, 1, 30);
	writer.Close();
	reader.Close();

	AVFormatContext* format_context = nullptr;
	REQUIRE(avformat_open_input(&format_context, out_name.c_str(), nullptr, nullptr) == 0);
	REQUIRE(avformat_find_stream_info(format_context, nullptr) >= 0);
	AVStream* stream = first_video_stream(format_context);
	REQUIRE(stream != nullptr);

	CHECK(stream->r_frame_rate.num == 30);
	CHECK(stream->r_frame_rate.den == 1);
	CHECK(stream->avg_frame_rate.num == 30);
	CHECK(stream->avg_frame_rate.den == 1);
	CHECK(stream->duration == av_rescale_q(30, av_make_q(1, 30), stream->time_base));

	avformat_close_input(&format_context);
}

TEST_CASE( "WriteFrameAt_preserves_sparse_video_timestamps", "[libopenshot][ffmpegwriter][fps]" )
{
	const std::string out_name = "WriteFrameAt_sparse_timestamps.mp4";
	DummyReader reader(Fraction(30, 1), 1280, 720, 0, 0, 1.0f);
	reader.Open();

	FFmpegWriter writer(out_name);
	writer.SetVideoOptions(true, "libx264", Fraction(30, 1), 1280, 720, Fraction(1, 1), false, false, 15000000);
	writer.Open();
	writer.WriteFrameAt(reader.GetFrame(1), 1);
	writer.WriteFrameAt(reader.GetFrame(2), 2);
	writer.WriteFrameAt(reader.GetFrame(3), 10);
	writer.Close();
	reader.Close();

	AVFormatContext* format_context = nullptr;
	REQUIRE(avformat_open_input(&format_context, out_name.c_str(), nullptr, nullptr) == 0);
	REQUIRE(avformat_find_stream_info(format_context, nullptr) >= 0);
	AVStream* stream = first_video_stream(format_context);
	REQUIRE(stream != nullptr);

	CHECK(stream->duration == av_rescale_q(10, av_make_q(1, 30), stream->time_base));

	avformat_close_input(&format_context);
}

TEST_CASE( "SizeOrdering_x264_CRF", "[libopenshot][ffmpegwriter][filesize]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader reader(path.str());
	reader.Open();

	auto file_size = [](const std::string& file_path) -> std::streamoff {
		std::ifstream in(file_path, std::ios::binary | std::ios::ate);
		if (!in)
			return -1;
		return in.tellg();
	};

	auto encode = [&](const std::string& out_name, int crf, int audio_bitrate) -> std::streamoff {
		FFmpegWriter w(out_name);
		w.SetAudioOptions(true, "aac", 48000, 2, LAYOUT_STEREO, audio_bitrate);
		w.SetVideoOptions(true, "libx264", Fraction(24,1), 1280, 720, Fraction(1,1), false, false, crf);
		w.PrepareStreams();
		w.SetOption(VIDEO_STREAM, "crf", std::to_string(crf));
		w.Open();
		w.WriteFrame(&reader, 1, 120);
		w.Close();
		return file_size(out_name);
	};

	const auto low_size = encode("SizeOrdering_x264_CRF_low.mp4", 30, 96000);
	const auto med_size = encode("SizeOrdering_x264_CRF_med.mp4", 23, 128000);
	const auto high_size = encode("SizeOrdering_x264_CRF_high.mp4", 20, 160000);

	CHECK(low_size < med_size);
	CHECK(med_size < high_size);

	reader.Close();
}

TEST_CASE( "SizeOrdering_vp9_CRF", "[libopenshot][ffmpegwriter][filesize]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader reader(path.str());
	reader.Open();

	auto file_size = [](const std::string& file_path) -> std::streamoff {
		std::ifstream in(file_path, std::ios::binary | std::ios::ate);
		if (!in)
			return -1;
		return in.tellg();
	};

	auto encode = [&](const std::string& out_name, int crf, int audio_bitrate) -> std::streamoff {
		FFmpegWriter w(out_name);
		w.SetAudioOptions(true, "libvorbis", 48000, 2, LAYOUT_STEREO, audio_bitrate);
		w.SetVideoOptions(true, "libvpx-vp9", Fraction(24,1), 1280, 720, Fraction(1,1), false, false, crf);
		w.PrepareStreams();
		w.SetOption(VIDEO_STREAM, "crf", std::to_string(crf));
		w.Open();
		w.WriteFrame(&reader, 1, 120);
		w.Close();
		return file_size(out_name);
	};

	const auto low_size = encode("SizeOrdering_vp9_CRF_low.webm", 46, 96000);
	const auto med_size = encode("SizeOrdering_vp9_CRF_med.webm", 34, 128000);
	const auto high_size = encode("SizeOrdering_vp9_CRF_high.webm", 26, 160000);

	CHECK(low_size < med_size);
	CHECK(med_size < high_size);

	reader.Close();
}
