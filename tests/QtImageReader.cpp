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

#include <catch2/catch.hpp>

#include "test_utils.h"

#include <Qt>
#include <QGuiApplication>
#include <QColor>
#include <QImage>

#include "QtImageReader.h"
#include "Clip.h"
#include "Exceptions.h"
#include "Frame.h"
#include "Timeline.h"

using namespace openshot;

TEST_CASE( "Default constructor", "[libopenshot][qtimagereader]" )
{
    openshot::QtImageReader r("", false);
    CHECK_FALSE(r.IsOpen());
    CHECK_THROWS_AS(r.Open(), openshot::InvalidFile);
}

TEST_CASE( "Construct from a QImage", "[libopenshot][qtimagereader]" )
{
    QImage i(1280, 720, QImage::Format_RGBA8888_Premultiplied);
    i.fill(Qt::red);
    openshot::QtImageReader r(i);

    CHECK_FALSE(r.IsOpen());
    CHECK(r.info.width == 1280);
    CHECK(r.info.height == 720);

    r.Open();
    CHECK(r.IsOpen());

    auto f = r.GetFrame(1);
    CHECK(f->GetWidth() == 1280);
    CHECK(f->GetHeight() == 720);
    CHECK(f->number == 1);

    auto frame_img = f->GetImage();
    CHECK(*frame_img == i);
}

TEST_CASE( "Exceptions and protections", "[libopenshot][qtimagereader]" )
{
	// Check invalid path
	CHECK_THROWS_AS(QtImageReader(""), InvalidFile);

	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "front.png";
	openshot::QtImageReader r(path.str());

	// Double open
	r.Open();
	CHECK(r.IsOpen());
	r.Open();
	CHECK(r.IsOpen());

	// Double close
	r.Close();
	CHECK_FALSE(r.IsOpen());
	r.Close();
	CHECK_FALSE(r.IsOpen());

	// Load bad path
	CHECK_THROWS_AS(
		openshot::QtImageReader("filethatdoesnotexist.png"),
		openshot::InvalidFile);
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

TEST_CASE( "Set path or image", "[libopenshot][qtimagereader]" )
{
    std::stringstream pathA;
    pathA << TEST_MEDIA_PATH << "1F0CF.svg";
    std::stringstream pathB;
    pathB << TEST_MEDIA_PATH << "front.png";

    QImage imgB(QString::fromStdString(pathB.str()));
    openshot::QtImageReader r(pathB.str());
    r.Open();
#if QT_VERSION > QT_VERSION_CHECK(5, 10, 0)
    CHECK(r.info.file_size == imgB.sizeInBytes());
#else
    CHECK(r.info.file_size == imgB.byteCount());
#endif
    CHECK(imgB.size() == QSize(r.info.width, r.info.height));

    // Ignored update with bad input
    r.SetPath("");
    CHECK(r.IsOpen());
#if QT_VERSION > QT_VERSION_CHECK(5, 10, 0)
    CHECK(r.info.file_size == imgB.sizeInBytes());
#else
    CHECK(r.info.file_size == imgB.byteCount());
#endif

    // Update with new path
    r.SetPath(pathA.str());
    CHECK(r.IsOpen());
    CHECK(r.info.width == r.info.height);
    CHECK(r.info.vcodec == "QImage");

    // Change to existing QImage
    r.SetQImage(imgB);
    auto f = r.GetFrame(1);
    auto i = f->GetImage();
    CHECK(i->size() == imgB.size());
    CHECK(i->pixelColor(10, 10) == imgB.pixelColor(10, 10));
    r.Close();

    // Ignored attempt to supply unusable image
    r.SetQImage(QImage());
    CHECK_FALSE(r.IsOpen());
    r.Open();
    CHECK(r.IsOpen());
    CHECK(r.info.width == i->width());
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
