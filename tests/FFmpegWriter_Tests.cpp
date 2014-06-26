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
 * and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Also, if your software can interact with users remotely through a computer
 * network, you should also make sure that it provides a way for users to
 * get its source. For example, if your program is a web application, its
 * interface could display a "Source" link that leads users to an archive
 * of the code. There are many ways you could offer source, and different
 * solutions will be better for different programs; see section 13 for the
 * specific requirements.
 *
 * You should also get your employer (if you work as a programmer) or school,
 * if any, to sign a "copyright disclaimer" for the program, if necessary.
 * For more information on this, and how to apply and follow the GNU AGPL, see
 * <http://www.gnu.org/licenses/>.
 */

#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(FFmpegWriter_Test_Webm)
{
	// Reader
	FFmpegReader r("../../src/examples/sintel_trailer-720p.mp4");
	r.Open();

	/* WRITER ---------------- */
	FFmpegWriter w("output1.webm");

	// Set options
	w.SetAudioOptions(true, "libvorbis", 44100, 2, 188000);
	w.SetVideoOptions(true, "libvpx", Fraction(24,1), 1280, 720, Fraction(1,1), false, false, 30000000);

	// Prepare Streams
	w.PrepareStreams();

	// Write header
	w.WriteHeader();

	w.WriteFrame(&r, 24, 50);

	// Write Footer
	w.WriteTrailer();

	// Close writer & reader
	w.Close();
	r.Close();

	FFmpegReader r1("output1.webm");
	r1.Open();

	// Verify various settings on new MP4
	CHECK_EQUAL(2, r1.GetFrame(1)->GetAudioChannelsCount());
	CHECK_EQUAL(24, r1.info.fps.num);
	CHECK_EQUAL(1, r1.info.fps.den);

	// Check frame 20
	tr1::shared_ptr<Frame> f = r1.GetFrame(8);

	// Get the image data for row 50
	const Magick::PixelPacket* pixels = f->GetPixels(500);

	// Check pixel values on scanline 500, pixel 600
	CHECK_EQUAL(2056, pixels[600].red);
	CHECK_EQUAL(2056, pixels[600].blue);
	CHECK_EQUAL(2056, pixels[600].green);
	CHECK_EQUAL(0, pixels[600].opacity);
}
