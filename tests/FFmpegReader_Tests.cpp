/**
 * @file
 * @brief Unit tests for openshot::FFmpegReader
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
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

#include "UnitTest++.h"
#include "../include/OpenShot.h"
#include "../include/Tests.h"

using namespace std;
using namespace openshot;

TEST(FFmpegReader_Invalid_Path)
{
	// Check invalid path
	CHECK_THROW(FFmpegReader(""), InvalidFile);
}

TEST(FFmpegReader_GetFrame_Before_Opening)
{
	// Create a reader
	stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";
	FFmpegReader r(path.str());

	// Check invalid path
	CHECK_THROW(r.GetFrame(1), ReaderClosed);
}

TEST(FFmpegReader_Check_Audio_File)
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
	CHECK_CLOSE(0.160781, samples[230], 0.00001);
	CHECK_CLOSE(-0.06125f, samples[300], 0.00001);

	// Close reader
	r.Close();
}

TEST(FFmpegReader_Check_Video_File)
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
	CHECK_EQUAL(21, (int)pixels[pixel_index]);
	CHECK_EQUAL(191, (int)pixels[pixel_index + 1]);
	CHECK_EQUAL(0, (int)pixels[pixel_index + 2]);
	CHECK_EQUAL(255, (int)pixels[pixel_index + 3]);

	// Get frame 1
	f = r.GetFrame(2);

	// Get the next frame
	pixels = f->GetPixels(10);
	pixel_index = 112 * 4; // pixel 112 (4 bytes per pixel)

	// Check image properties on scanline 10, pixel 112
	CHECK_EQUAL(0, (int)pixels[pixel_index]);
	CHECK_EQUAL(96, (int)pixels[pixel_index + 1]);
	CHECK_EQUAL(188, (int)pixels[pixel_index + 2]);
	CHECK_EQUAL(255, (int)pixels[pixel_index + 3]);

	// Close reader
	r.Close();
}

TEST(FFmpegReader_Seek)
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

TEST(FFmpegReader_Multiple_Open_and_Close)
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

