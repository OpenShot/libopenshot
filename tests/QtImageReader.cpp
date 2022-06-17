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

#include "QtImageReader.h"
#include "Clip.h"
#include "Exceptions.h"
#include "Frame.h"
#include "Timeline.h"

using namespace openshot;

TEST_CASE( "Default_Constructor", "[libopenshot][qtimagereader]" )
{
	// Check invalid path
	CHECK_THROWS_AS(QtImageReader(""), InvalidFile);
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
