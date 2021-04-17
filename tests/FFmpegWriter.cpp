/**
 * @file
 * @brief Unit tests for openshot::FFmpegWriter
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

#include "FFmpegWriter.h"
#include "Exceptions.h"
#include "FFmpegReader.h"
#include "Fraction.h"
#include "Frame.h"

using namespace std;
using namespace openshot;

TEST_CASE( "Webm", "[libopenshot][ffmpegwriter]" )
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
	CHECK(r1.GetFrame(1)->GetAudioChannelsCount() == 2);
	CHECK(r1.info.fps.num == 24);
	CHECK(r1.info.fps.den == 1);

	// Get a specific frame
	std::shared_ptr<Frame> f = r1.GetFrame(8);

	// Get the image data for row 500
	const unsigned char* pixels = f->GetPixels(500);
	int pixel_index = 112 * 4; // pixel 112 (4 bytes per pixel)

	// Check image properties on scanline 10, pixel 112
	CHECK((int)pixels[pixel_index] == Approx(23).margin(5));
	CHECK((int)pixels[pixel_index + 1] == Approx(23).margin(5));
	CHECK((int)pixels[pixel_index + 2] == Approx(23).margin(5));
	CHECK((int)pixels[pixel_index + 3] == Approx(255).margin(5));
}

TEST_CASE( "Options_Overloads", "[libopenshot][ffmpegwriter]" )
{
	// Reader
	stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());
	r.Open();

	/* WRITER ---------------- */
	FFmpegWriter w("output1.mp4");

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

	FFmpegReader r1("output1.mp4");
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
