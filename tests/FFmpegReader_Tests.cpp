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

#include "UnitTest++.h"
// Prevent name clashes with juce::UnitTest
#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "FFmpegReader.h"
#include "Frame.h"
#include "Timeline.h"
#include "Json.h"

using namespace std;
using namespace openshot;

SUITE(FFmpegReader)
{

TEST(Invalid_Path)
{
	// Check invalid path
	CHECK_THROW(FFmpegReader(""), InvalidFile);
}

TEST(GetFrame_Before_Opening)
{
	// Create a reader
	stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";
	FFmpegReader r(path.str());

	// Check invalid path
	CHECK_THROW(r.GetFrame(1), ReaderClosed);
}

TEST(Check_Audio_File)
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
	CHECK_EQUAL(2, f->GetAudioChannelsCount());
	CHECK_EQUAL(332, f->GetAudioSamplesCount());

	// Check actual sample values (to be sure the waveform is correct)
	CHECK_CLOSE(0.0f, samples[0], 0.00001);
	CHECK_CLOSE(0.0f, samples[50], 0.00001);
	CHECK_CLOSE(0.0f, samples[100], 0.00001);
	CHECK_CLOSE(0.0f, samples[200], 0.00001);
	CHECK_CLOSE(0.16406f, samples[230], 0.00001);
	CHECK_CLOSE(-0.06250f, samples[300], 0.00001);

	// Close reader
	r.Close();
}

TEST(Check_Video_File)
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
	CHECK_CLOSE(21, (int)pixels[pixel_index], 5);
	CHECK_CLOSE(191, (int)pixels[pixel_index + 1], 5);
	CHECK_CLOSE(0, (int)pixels[pixel_index + 2], 5);
	CHECK_CLOSE(255, (int)pixels[pixel_index + 3], 5);

	// Check pixel function
	CHECK_EQUAL(true, f->CheckPixel(10, 112, 21, 191, 0, 255, 5));
	CHECK_EQUAL(false, f->CheckPixel(10, 112, 0, 0, 0, 0, 5));

	// Get frame 1
	f = r.GetFrame(2);

	// Get the next frame
	pixels = f->GetPixels(10);
	pixel_index = 112 * 4; // pixel 112 (4 bytes per pixel)

	// Check image properties on scanline 10, pixel 112
	CHECK_CLOSE(0, (int)pixels[pixel_index], 5);
	CHECK_CLOSE(96, (int)pixels[pixel_index + 1], 5);
	CHECK_CLOSE(188, (int)pixels[pixel_index + 2], 5);
	CHECK_CLOSE(255, (int)pixels[pixel_index + 3], 5);

	// Check pixel function
	CHECK_EQUAL(true, f->CheckPixel(10, 112, 0, 96, 188, 255, 5));
	CHECK_EQUAL(false, f->CheckPixel(10, 112, 0, 0, 0, 0, 5));

	// Close reader
	r.Close();
}

TEST(Seek)
{
	// Create a reader
	stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	// Get frame
	std::shared_ptr<Frame> f = r.GetFrame(1);
	CHECK_EQUAL(1, f->number);

	// Get frame
	f = r.GetFrame(300);
	CHECK_EQUAL(300, f->number);

	// Get frame
	f = r.GetFrame(301);
	CHECK_EQUAL(301, f->number);

	// Get frame
	f = r.GetFrame(315);
	CHECK_EQUAL(315, f->number);

	// Get frame
	f = r.GetFrame(275);
	CHECK_EQUAL(275, f->number);

	// Get frame
	f = r.GetFrame(270);
	CHECK_EQUAL(270, f->number);

	// Get frame
	f = r.GetFrame(500);
	CHECK_EQUAL(500, f->number);

	// Get frame
	f = r.GetFrame(100);
	CHECK_EQUAL(100, f->number);

	// Get frame
	f = r.GetFrame(600);
	CHECK_EQUAL(600, f->number);

	// Get frame
	f = r.GetFrame(1);
	CHECK_EQUAL(1, f->number);

	// Get frame
	f = r.GetFrame(700);
	CHECK_EQUAL(700, f->number);

	// Close reader
	r.Close();

}

TEST(Frame_Rate)
{
	// Create a reader
	stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	// Verify detected frame rate
	openshot::Fraction rate = r.info.fps;
	CHECK_EQUAL(24, rate.num);
	CHECK_EQUAL(1, rate.den);

	r.Close();
}

TEST(Multiple_Open_and_Close)
{
	// Create a reader
	stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	// Get frame that requires a seek
	std::shared_ptr<Frame> f = r.GetFrame(1200);
	CHECK_EQUAL(1200, f->number);

	// Close and Re-open the reader
	r.Close();
	r.Open();

	// Get frame
	f = r.GetFrame(1);
	CHECK_EQUAL(1, f->number);
	f = r.GetFrame(250);
	CHECK_EQUAL(250, f->number);

	// Close and Re-open the reader
	r.Close();
	r.Open();

	// Get frame
	f = r.GetFrame(750);
	CHECK_EQUAL(750, f->number);
	f = r.GetFrame(1000);
	CHECK_EQUAL(1000, f->number);

	// Close reader
	r.Close();
}

TEST(Verify_Parent_Timeline)
{
	// Create a reader
	stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	// Check size of frame image
	CHECK_EQUAL(r.GetFrame(1)->GetImage()->width(), 1280);
	CHECK_EQUAL(r.GetFrame(1)->GetImage()->height(), 720);
	r.GetFrame(1)->GetImage()->save("reader-1.png", "PNG");

	// Create a Clip associated with this reader
	Clip c1(&r);
	c1.Open();

	// Check size of frame image (should still be the same)
	CHECK_EQUAL(r.GetFrame(1)->GetImage()->width(), 1280);
	CHECK_EQUAL(r.GetFrame(1)->GetImage()->height(), 720);

	// Create Timeline
	Timeline t1(640, 480, Fraction(30,1), 44100, 2, LAYOUT_STEREO);
	t1.AddClip(&c1);

	// Check size of frame image (it should now match the parent timeline)
	CHECK_EQUAL(r.GetFrame(1)->GetImage()->width(), 640);
	CHECK_EQUAL(r.GetFrame(1)->GetImage()->height(), 360);
}

} // SUITE(FFmpegReader)
