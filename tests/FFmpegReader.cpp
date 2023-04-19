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

#include "openshot_catch.h"

#include "FFmpegReader.h"
#include "Exceptions.h"
#include "Frame.h"
#include "Timeline.h"
#include "Json.h"

using namespace openshot;

TEST_CASE( "Invalid_Path", "[libopenshot][ffmpegreader]" )
{
	// Check invalid path
	CHECK_THROWS_AS(FFmpegReader(""), InvalidFile);
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
	CHECK(f->GetAudioSamplesCount() == 332);

	// Check actual sample values (to be sure the waveform is correct)
	CHECK(samples[0] == Approx(0.0f).margin(0.00001));
	CHECK(samples[50] == Approx(0.0f).margin(0.00001));
	CHECK(samples[100] == Approx(0.0f).margin(0.00001));
	CHECK(samples[200] == Approx(0.0f).margin(0.00001));
	CHECK(samples[230] == Approx(0.16406f).margin(0.00001));
	CHECK(samples[300] == Approx(-0.06250f).margin(0.00001));

	// Close reader
	r.Close();
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
--> Duration: 51.95 Seconds
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