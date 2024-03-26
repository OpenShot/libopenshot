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

#include "openshot_catch.h"

#include "FFmpegWriter.h"
#include "Exceptions.h"
#include "FFmpegReader.h"
#include "Fraction.h"
#include "Frame.h"
#include "Timeline.h"

using namespace std;
using namespace openshot;

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
