/**
 * @file
 * @brief Unit tests for openshot::Clip
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <sstream>
#include <memory>

#include <catch2/catch.hpp>

#include <QColor>
#include <QImage>
#include <QSize>

#include "Clip.h"
#include "Enums.h"
#include "Exceptions.h"
#include "Frame.h"
#include "Fraction.h"
#include "Timeline.h"
#include "Json.h"
#include "effects/Negate.h"

using namespace openshot;

TEST_CASE( "default constructor", "[libopenshot][clip]" )
{
	// Create a empty clip
	Clip c1;

	// Check basic settings
	CHECK(c1.anchor == ANCHOR_CANVAS);
	CHECK(c1.gravity == GRAVITY_CENTER);
	CHECK(c1.scale == SCALE_FIT);
	CHECK(c1.Layer() == 0);
	CHECK(c1.Position() == Approx(0.0f).margin(0.00001));
	CHECK(c1.Start() == Approx(0.0f).margin(0.00001));
	CHECK(c1.End() == Approx(0.0f).margin(0.00001));
}

TEST_CASE( "path string constructor", "[libopenshot][clip]" )
{
	// Create a empty clip
	std::stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";
	Clip c1(path.str());
	c1.Open();

	// Check basic settings
	CHECK(c1.anchor == ANCHOR_CANVAS);
	CHECK(c1.gravity == GRAVITY_CENTER);
	CHECK(c1.scale == SCALE_FIT);
	CHECK(c1.Layer() == 0);
	CHECK(c1.Position() == Approx(0.0f).margin(0.00001));
	CHECK(c1.Start() == Approx(0.0f).margin(0.00001));
	CHECK(c1.End() == Approx(4.39937f).margin(0.00001));
}

TEST_CASE( "basic getters and setters", "[libopenshot][clip]" )
{
	// Create a empty clip
	Clip c1;

	// Check basic settings
	CHECK_THROWS_AS(c1.Open(), ReaderClosed);
	CHECK(c1.anchor == ANCHOR_CANVAS);
	CHECK(c1.gravity == GRAVITY_CENTER);
	CHECK(c1.scale == SCALE_FIT);
	CHECK(c1.Layer() == 0);
	CHECK(c1.Position() == Approx(0.0f).margin(0.00001));
	CHECK(c1.Start() == Approx(0.0f).margin(0.00001));
	CHECK(c1.End() == Approx(0.0f).margin(0.00001));

	// Change some properties
	c1.Layer(1);
	c1.Position(5.0);
	c1.Start(3.5);
	c1.End(10.5);

	CHECK(c1.Layer() == 1);
	CHECK(c1.Position() == Approx(5.0f).margin(0.00001));
	CHECK(c1.Start() == Approx(3.5f).margin(0.00001));
	CHECK(c1.End() == Approx(10.5f).margin(0.00001));
}

TEST_CASE( "properties", "[libopenshot][clip]" )
{
	// Create a empty clip
	Clip c1;

	// Change some properties
	c1.Layer(1);
	c1.Position(5.0);
	c1.Start(3.5);
	c1.End(10.5);
	c1.alpha.AddPoint(1, 1.0);
	c1.alpha.AddPoint(500, 0.0);

	// Get properties JSON string at frame 1
	std::string properties = c1.PropertiesJSON(1);

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::CharReaderBuilder rbuilder;
	Json::CharReader* reader(rbuilder.newCharReader());
	std::string errors;
	bool success = reader->parse(
		properties.c_str(),
		properties.c_str() + properties.size(),
		&root, &errors );
	CHECK(success == true);

	// Check for specific things
	CHECK(root["alpha"]["value"].asDouble() == Approx(1.0f).margin(0.01));
	CHECK(root["alpha"]["keyframe"].asBool() == true);

	// Get properties JSON string at frame 250
	properties = c1.PropertiesJSON(250);

	// Parse JSON string into JSON objects
	root.clear();
	success = reader->parse(
		properties.c_str(),
		properties.c_str() + properties.size(),
		&root, &errors );
	CHECK(success == true);

	// Check for specific things
	CHECK(root["alpha"]["value"].asDouble() == Approx(0.5f).margin(0.01));
	CHECK_FALSE(root["alpha"]["keyframe"].asBool());

	// Get properties JSON string at frame 250 (again)
	properties = c1.PropertiesJSON(250);

	// Parse JSON string into JSON objects
	root.clear();
	success = reader->parse(
		properties.c_str(),
		properties.c_str() + properties.size(),
		&root, &errors );
	CHECK(success == true);

	// Check for specific things
	CHECK_FALSE(root["alpha"]["keyframe"].asBool());

	// Get properties JSON string at frame 500
	properties = c1.PropertiesJSON(500);

	// Parse JSON string into JSON objects
	root.clear();
	success = reader->parse(
		properties.c_str(),
		properties.c_str() + properties.size(),
		&root, &errors );
	CHECK(success == true);

	// Check for specific things
	CHECK(root["alpha"]["value"].asDouble() == Approx(0.0f).margin(0.00001));
	CHECK(root["alpha"]["keyframe"].asBool() == true);

	// Free up the reader we allocated
	delete reader;
}

TEST_CASE( "effects", "[libopenshot][clip]" )
{
	// Load clip with video
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	Clip c10(path.str());
	c10.Open();

	Negate n;
	c10.AddEffect(&n);

	// Get frame 1
	std::shared_ptr<Frame> f = c10.GetFrame(500);

	// Get the image data
	const unsigned char* pixels = f->GetPixels(10);
	int pixel_index = 112 * 4; // pixel 112 (4 bytes per pixel)

	// Check image properties on scanline 10, pixel 112
	CHECK((int)pixels[pixel_index] == 255);
	CHECK((int)pixels[pixel_index + 1] == 255);
	CHECK((int)pixels[pixel_index + 2] == 255);
	CHECK((int)pixels[pixel_index + 3] == 255);

	// Check the # of Effects
	CHECK((int)c10.Effects().size() == 1);


	// Add a 2nd negate effect
	Negate n1;
	c10.AddEffect(&n1);

	// Get frame 1
	f = c10.GetFrame(500);

	// Get the image data
	pixels = f->GetPixels(10);
	pixel_index = 112 * 4; // pixel 112 (4 bytes per pixel)

	// Check image properties on scanline 10, pixel 112
	CHECK((int)pixels[pixel_index] == 0);
	CHECK((int)pixels[pixel_index + 1] == 0);
	CHECK((int)pixels[pixel_index + 2] == 0);
	CHECK((int)pixels[pixel_index + 3] == 255);

	// Check the # of Effects
	CHECK((int)c10.Effects().size() == 2);
}

TEST_CASE( "verify parent Timeline", "[libopenshot][clip]" )
{
	Timeline t1(640, 480, Fraction(30,1), 44100, 2, LAYOUT_STEREO);

	// Load clip with video
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	Clip c1(path.str());
	c1.Open();

	// Check size of frame image
	CHECK(1280 == c1.GetFrame(1)->GetImage()->width());
	CHECK(720 == c1.GetFrame(1)->GetImage()->height());

	// Add clip to timeline
	t1.AddClip(&c1);

	// Check size of frame image (with an associated timeline)
	CHECK(640 == c1.GetFrame(1)->GetImage()->width());
	CHECK(360 == c1.GetFrame(1)->GetImage()->height());
}

TEST_CASE( "has_video", "[libopenshot][clip]" )
{
    std::stringstream path;
    path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
    openshot::Clip c1(path.str());

    c1.has_video.AddPoint(1.0, 0.0);
    c1.has_video.AddPoint(5.0, -1.0, openshot::CONSTANT);
    c1.has_video.AddPoint(10.0, 1.0, openshot::CONSTANT);

    c1.Open();

    auto trans_color = QColor(Qt::transparent);
    auto f1 = c1.GetFrame(1);
    CHECK(f1->has_image_data);

    auto f2 = c1.GetFrame(5);
    CHECK(f2->has_image_data);

    auto f3 = c1.GetFrame(5);
    CHECK(f3->has_image_data);

    auto i1 = f1->GetImage();
    QSize f1_size(f1->GetWidth(), f1->GetHeight());
    CHECK(i1->size() == f1_size);
    CHECK(i1->pixelColor(20, 20) == trans_color);

    auto i2 = f2->GetImage();
    QSize f2_size(f2->GetWidth(), f2->GetHeight());
    CHECK(i2->size() == f2_size);
    CHECK(i2->pixelColor(20, 20) != trans_color);

    auto i3 = f3->GetImage();
    QSize f3_size(f3->GetWidth(), f3->GetHeight());
    CHECK(i3->size() == f3_size);
    CHECK(i3->pixelColor(20, 20) != trans_color);
}
