/**
 * @file
 * @brief Unit tests for openshot::FFmpegWriter
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

TEST(FFmpegWriter_Test_Webm)
{
	// Reader
	stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	/* WRITER ---------------- */
	FFmpegWriter w("output1.webm");

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

	FFmpegReader r1("output1.webm");
	r1.Open();

	// Verify various settings on new MP4
	CHECK_EQUAL(2, r1.GetFrame(1)->GetAudioChannelsCount());
	CHECK_EQUAL(24, r1.info.fps.num);
	CHECK_EQUAL(1, r1.info.fps.den);

	// Get a specific frame
	std::shared_ptr<Frame> f = r1.GetFrame(8);

	// Get the image data for row 500
	const unsigned char* pixels = f->GetPixels(500);
	int pixel_index = 112 * 4; // pixel 112 (4 bytes per pixel)

	// Check image properties on scanline 10, pixel 112
	CHECK_EQUAL(23, (int)pixels[pixel_index]);
	CHECK_EQUAL(23, (int)pixels[pixel_index + 1]);
	CHECK_EQUAL(23, (int)pixels[pixel_index + 2]);
	CHECK_EQUAL(255, (int)pixels[pixel_index + 3]);
}
