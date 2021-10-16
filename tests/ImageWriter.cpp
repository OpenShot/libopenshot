/**
 * @file
 * @brief Unit tests for openshot::ImageWriter
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifdef USE_IMAGEMAGICK

#include <sstream>
#include <memory>

#include <catch2/catch.hpp>

#include "ImageWriter.h"
#include "Exceptions.h"
#include "ImageReader.h"
#include "FFmpegReader.h"
#include "Frame.h"

using namespace openshot;

TEST_CASE( "Gif", "[libopenshot][imagewriter]" )
{
	// Reader ---------------

	// Bad path
	FFmpegReader bad_r("/tmp/bleeblorp.xls", false);
	CHECK_THROWS_AS(bad_r.Open(), InvalidFile);

	// Good path
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());

	// Read-before-open error
	CHECK_THROWS_AS(r.GetFrame(1), ReaderClosed);

	r.Open();

	/* WRITER ---------------- */
	ImageWriter w("output1.gif");

	CHECK_FALSE(w.IsOpen());

	// Check for exception on write-before-open
	CHECK_THROWS_AS(w.WriteFrame(&r, 500, 504), WriterClosed);

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
	CHECK(r1.Name() == "ImageReader");

	CacheBase* c = r1.GetCache();
	CHECK(c == nullptr);

	CHECK_FALSE(r1.IsOpen());
	r1.Open();
	CHECK(r1.IsOpen() == true);

	// Verify various settings
	CHECK(r1.info.width == r.info.width);
	CHECK(r1.info.height == r.info.height);

	// Get a specific frame
	std::shared_ptr<Frame> f = r1.GetFrame(8);

	// Get the image data for row 500
	const unsigned char* pixels = f->GetPixels(500);
	int pixel_index = 230 * 4; // pixel 230 (4 bytes per pixel)

	// Check image properties
	CHECK((int)pixels[pixel_index] == Approx(20).margin(5));
	CHECK((int)pixels[pixel_index + 1] == Approx(18).margin(5));
	CHECK((int)pixels[pixel_index + 2] == Approx(11).margin(5));
	CHECK((int)pixels[pixel_index + 3] == Approx(255).margin(5));
}
#endif  // USE_IMAGEMAGICK
