/**
 * @file
 * @brief Unit tests for openshot::ImageWriter
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

#ifdef USE_IMAGEMAGICK
#include "ImageWriter.h"
#include "Exceptions.h"
#include "ImageReader.h"
#include "FFmpegReader.h"
#include "Frame.h"

using namespace std;
using namespace openshot;

SUITE(ImageWriter)
{

TEST(Gif)
{
	// Reader ---------------

	// Bad path
	FFmpegReader bad_r("/tmp/bleeblorp.xls", false);
	CHECK_THROW(bad_r.Open(), InvalidFile);

	// Good path
	stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());

	// Read-before-open error
	CHECK_THROW(r.GetFrame(1), ReaderClosed);

	r.Open();

	/* WRITER ---------------- */
	ImageWriter w("output1.gif");

	CHECK_EQUAL(false, w.IsOpen());

	// Check for exception on write-before-open
	CHECK_THROW(w.WriteFrame(&r, 500, 504), WriterClosed);

	// Set the image output settings (format, fps, width, height, quality, loops, combine)
	w.SetVideoOptions("GIF", r.info.fps, r.info.width, r.info.height, 70, 1, true);

	// Open writer
	w.Open();

	// Write some frames (start on frame 500 and go to frame 510)
	w.WriteFrame(&r, 500, 504);

	// Close writer & reader
	w.Close();
	r.Close();

	// Open up the 5th frame from the newly created GIF
	ImageReader r1("output1.gif[4]");

	// Basic Reader state queries
	CHECK_EQUAL("ImageReader", r1.Name());

	CacheBase* c = r1.GetCache();
	CHECK_EQUAL(true, c == nullptr);

	CHECK_EQUAL(false, r1.IsOpen());
	r1.Open();
	CHECK_EQUAL(true, r1.IsOpen());

	// Verify various settings
	CHECK_EQUAL(r.info.width, r1.info.width);
	CHECK_EQUAL(r.info.height, r1.info.height);

	// Get a specific frame
	std::shared_ptr<Frame> f = r1.GetFrame(8);

	// Get the image data for row 500
	const unsigned char* pixels = f->GetPixels(500);
	int pixel_index = 230 * 4; // pixel 230 (4 bytes per pixel)

	// Check image properties
	CHECK_CLOSE(20, (int)pixels[pixel_index], 5);
	CHECK_CLOSE(18, (int)pixels[pixel_index + 1], 5);
	CHECK_CLOSE(11, (int)pixels[pixel_index + 2], 5);
	CHECK_CLOSE(255, (int)pixels[pixel_index + 3], 5);
}

} // SUITE
#endif
