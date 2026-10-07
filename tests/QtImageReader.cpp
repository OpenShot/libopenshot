/**
 * @file
 * @brief Unit tests for openshot::QtImageReader
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include <QGuiApplication>
#include <sstream>

#include "QtImageReader.h"
#include "Clip.h"
#include "Exceptions.h"
#include "Frame.h"
#include "Timeline.h"

using namespace openshot;

TEST_CASE( "Default_Constructor", "[libopenshot][qtimagereader]" )
{
	// Check invalid path and error details
	const std::string invalid_path = "/tmp/__openshot_missing_test_file__.png";
	try {
		QtImageReader r(invalid_path);
		FAIL("Expected InvalidFile for missing image path");
	} catch (const InvalidFile& e) {
		const std::string message = e.what();
		CHECK(message.find("QtImageReader could not open image file.") != std::string::npos);
		CHECK(message.find(invalid_path) != std::string::npos);
	}
}

TEST_CASE( "GetFrame_Before_Opening", "[libopenshot][qtimagereader]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "front.png";
	QtImageReader r(path.str());

	// Check invalid path
	CHECK_THROWS_AS(r.GetFrame(1), ReaderClosed);
}

TEST_CASE( "Check_SVG_Loading", "[libopenshot][qtimagereader]" )
{
    // Create a reader
    std::stringstream path;
    path << TEST_MEDIA_PATH << "1F0CF.svg";
    QtImageReader r(path.str());
    r.Open();

    // Get frame, with no Timeline or Clip
    // Size should be equal to default SVG size
    std::shared_ptr<Frame> f = r.GetFrame(1);
    CHECK(f->GetImage()->width() == 72);
    CHECK(f->GetImage()->height() == 72);

    Fraction fps(30000,1000);
    Timeline t1(640, 480, fps, 44100, 2, LAYOUT_STEREO);

    Clip clip1(path.str());
    clip1.Layer(1);
    clip1.Position(0.0); // Delay the overlay by 0.05 seconds
    clip1.End(10.0);	// Make the duration of the overlay 1/2 second

    // Add clips
    t1.AddClip(&clip1);
    t1.Open();

    // Get frame, with 640x480 Timeline
    // Should scale to 480
    clip1.Reader()->Open();
    f = clip1.Reader()->GetFrame(2);
    CHECK(f->GetImage()->width() == 480);
    CHECK(f->GetImage()->height() == 480);

    // Add scale_x and scale_y. Should scale the square SVG
    // by the largest scale keyframe (i.e. 4)
    clip1.scale_x.AddPoint(1.0, 2.0, openshot::LINEAR);
    clip1.scale_y.AddPoint(1.0, 2.0, openshot::LINEAR);
    f = clip1.Reader()->GetFrame(3);
    CHECK(f->GetImage()->width() == 480 * 2);
    CHECK(f->GetImage()->height() == 480 * 2);

    // Close reader
    t1.Close();
    r.Close();
}

TEST_CASE( "Duration_And_Length_QtImageReader", "[libopenshot][qtimagereader]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "front.png";
	QtImageReader r(path.str());
	r.Open();

	// Duration and frame count should be aligned to fps (1 hour at 30 fps)
	CHECK(r.info.fps.num == 30);
	CHECK(r.info.fps.den == 1);
	CHECK(r.info.video_length == 108000);
	CHECK(r.info.duration == Approx(3600.0f).margin(0.001f));

	r.Close();
}

TEST_CASE( "Max_Decode_Size_QtImageReader", "[libopenshot][qtimagereader]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "front.png";

	QtImageReader r(path.str());
	r.Open();

	std::shared_ptr<Frame> full = r.GetFrame(1);
	REQUIRE(full != nullptr);
	CHECK(full->GetWidth() == r.info.width);
	CHECK(full->GetHeight() == r.info.height);

	r.SetMaxDecodeSize(64, 64);
	std::shared_ptr<Frame> limited = r.GetFrame(2);
	REQUIRE(limited != nullptr);
	CHECK(limited->GetWidth() <= 64);
	CHECK(limited->GetHeight() <= 64);
	CHECK(limited->GetWidth() < full->GetWidth());
	CHECK(limited->GetHeight() < full->GetHeight());

	r.Close();
}

TEST_CASE( "Max_Decode_Size_SVG_QtImageReader", "[libopenshot][qtimagereader]" )
{
	std::stringstream path;
	path << TEST_MEDIA_PATH << "1F0CF.svg";

	QtImageReader r(path.str());
	r.Open();

	std::shared_ptr<Frame> original = r.GetFrame(1);
	REQUIRE(original != nullptr);
	CHECK(original->GetWidth() == 72);
	CHECK(original->GetHeight() == 72);

	r.SetMaxDecodeSize(256, 128);
	std::shared_ptr<Frame> limited = r.GetFrame(2);
	REQUIRE(limited != nullptr);
	CHECK(limited->GetWidth() == 128);
	CHECK(limited->GetHeight() == 128);

	r.Close();
}
