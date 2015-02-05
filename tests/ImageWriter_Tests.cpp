/**
 * @file
 * @brief Unit tests for openshot::ImageWriter
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

using namespace std;
using namespace openshot;

TEST(ImageWriter_Test_Webm)
{
	// Reader
	FFmpegReader r("../../src/examples/sintel_trailer-720p.mp4");
	r.Open();

	/* WRITER ---------------- */
	ImageWriter w("output1.gif");

	// Set the image output settings (format, fps, width, height, quality, loops, combine)
	w.SetVideoOptions("GIF", r.info.fps, r.info.width, r.info.height, 70, 1, true);

	// Open writer
	w.Open();

	// Write some frames (start on frame 500 and go to frame 510)
	w.WriteFrame(&r, 500, 510);

	// Close writer & reader
	w.Close();
	r.Close();

	// Open up the 5th frame from the newly created GIF
	ImageReader r1("output1.gif[5]");
	r1.Open();

	// Verify various settings
	CHECK_EQUAL(r.info.width, r1.info.width);
	CHECK_EQUAL(r.info.height, r1.info.height);

	// Get a specific frame
	tr1::shared_ptr<Frame> f = r1.GetFrame(8);

	// Get the image data for row 500
	const Magick::PixelPacket* pixels = f->GetPixels(500);

	// Check pixel values on scanline 500, pixel 600
	CHECK_EQUAL(4883, pixels[600].red);
	CHECK_EQUAL(2570, pixels[600].blue);
	CHECK_EQUAL(3341, pixels[600].green);
	CHECK_EQUAL(0, pixels[600].opacity);
}
