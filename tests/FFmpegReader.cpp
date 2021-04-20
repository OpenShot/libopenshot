/**
 * @file
 * @brief Unit tests for openshot::FFmpegReader
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <sstream>
#include <memory>

#include <catch2/catch.hpp>

#include "FFmpegReader.h"
#include "Exceptions.h"
#include "Frame.h"
#include "Timeline.h"
#include "Json.h"

using namespace std;
using namespace openshot;

TEST_CASE( "Invalid_Path", "[libopenshot][ffmpegreader]" )
{
	// Check invalid path
	CHECK_THROWS_AS(FFmpegReader(""), InvalidFile);
}

TEST_CASE( "GetFrame_Before_Opening", "[libopenshot][ffmpegreader]" )
{
	// Create a reader
	stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";
	FFmpegReader r(path.str());

	// Check invalid path
	CHECK_THROWS_AS(r.GetFrame(1), ReaderClosed);
}

TEST_CASE( "Check_Audio_File", "[libopenshot][ffmpegreader]" )
{
	// Create a reader
	stringstream path;
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
	stringstream path;
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
	stringstream path;
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
	stringstream path;
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
	stringstream path;
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
	stringstream path;
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
}
