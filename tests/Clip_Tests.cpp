/**
 * @file
 * @brief Unit tests for openshot::Clip
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

// Work around older versions of UnitTest++ without REQUIRE
#ifndef REQUIRE
  #define REQUIRE
#endif

// Prevent name clashes with juce::UnitTest
#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "Clip.h"
#include "Frame.h"
#include "Fraction.h"
#include "Timeline.h"
#include "Json.h"

using namespace openshot;

SUITE(Clip)
{

TEST(Default_Constructor)
{
	// Create a empty clip
	Clip c1;

	// Check basic settings
	CHECK_EQUAL(ANCHOR_CANVAS, c1.anchor);
	CHECK_EQUAL(GRAVITY_CENTER, c1.gravity);
	CHECK_EQUAL(SCALE_FIT, c1.scale);
	CHECK_EQUAL(0, c1.Layer());
	CHECK_CLOSE(0.0f, c1.Position(), 0.00001);
	CHECK_CLOSE(0.0f, c1.Start(), 0.00001);
	CHECK_CLOSE(0.0f, c1.End(), 0.00001);
}

TEST(Clip_Constructor)
{
	// Create a empty clip
	std::stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";
	Clip c1(path.str());
	c1.Open();

	// Check basic settings
	CHECK_EQUAL(ANCHOR_CANVAS, c1.anchor);
	CHECK_EQUAL(GRAVITY_CENTER, c1.gravity);
	CHECK_EQUAL(SCALE_FIT, c1.scale);
	CHECK_EQUAL(0, c1.Layer());
	CHECK_CLOSE(0.0f, c1.Position(), 0.00001);
	CHECK_CLOSE(0.0f, c1.Start(), 0.00001);
	CHECK_CLOSE(4.39937f, c1.End(), 0.00001);
}

TEST(Basic_Gettings_and_Setters)
{
	// Create a empty clip
	Clip c1;

	// Check basic settings
	CHECK_THROW(c1.Open(), ReaderClosed);
	CHECK_EQUAL(ANCHOR_CANVAS, c1.anchor);
	CHECK_EQUAL(GRAVITY_CENTER, c1.gravity);
	CHECK_EQUAL(SCALE_FIT, c1.scale);
	CHECK_EQUAL(0, c1.Layer());
	CHECK_CLOSE(0.0f, c1.Position(), 0.00001);
	CHECK_CLOSE(0.0f, c1.Start(), 0.00001);
	CHECK_CLOSE(0.0f, c1.End(), 0.00001);

	// Change some properties
	c1.Layer(1);
	c1.Position(5.0);
	c1.Start(3.5);
	c1.End(10.5);

	CHECK_EQUAL(1, c1.Layer());
	CHECK_CLOSE(5.0f, c1.Position(), 0.00001);
	CHECK_CLOSE(3.5f, c1.Start(), 0.00001);
	CHECK_CLOSE(10.5f, c1.End(), 0.00001);
}

TEST(Properties)
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
	CHECK_EQUAL(true, success);

	// Check for specific things
	CHECK_CLOSE(1.0f, root["alpha"]["value"].asDouble(), 0.01);
	CHECK_EQUAL(true, root["alpha"]["keyframe"].asBool());

	// Get properties JSON string at frame 250
	properties = c1.PropertiesJSON(250);

	// Parse JSON string into JSON objects
	root.clear();
	success = reader->parse(
		properties.c_str(),
		properties.c_str() + properties.size(),
		&root, &errors );
	REQUIRE CHECK_EQUAL(true, success);

	// Check for specific things
	CHECK_CLOSE(0.5f, root["alpha"]["value"].asDouble(), 0.01);
	CHECK_EQUAL(false, root["alpha"]["keyframe"].asBool());

	// Get properties JSON string at frame 250 (again)
	properties = c1.PropertiesJSON(250);

	// Parse JSON string into JSON objects
	root.clear();
	success = reader->parse(
		properties.c_str(),
		properties.c_str() + properties.size(),
		&root, &errors );
	REQUIRE CHECK_EQUAL(true, success);

	// Check for specific things
	CHECK_EQUAL(false, root["alpha"]["keyframe"].asBool());

	// Get properties JSON string at frame 500
	properties = c1.PropertiesJSON(500);

	// Parse JSON string into JSON objects
	root.clear();
	success = reader->parse(
		properties.c_str(),
		properties.c_str() + properties.size(),
		&root, &errors );
	REQUIRE CHECK_EQUAL(true, success);

	// Check for specific things
	CHECK_CLOSE(0.0f, root["alpha"]["value"].asDouble(), 0.00001);
	CHECK_EQUAL(true, root["alpha"]["keyframe"].asBool());

	// Free up the reader we allocated
	delete reader;
}

TEST(Effects)
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
	CHECK_EQUAL(255, (int)pixels[pixel_index]);
	CHECK_EQUAL(255, (int)pixels[pixel_index + 1]);
	CHECK_EQUAL(255, (int)pixels[pixel_index + 2]);
	CHECK_EQUAL(255, (int)pixels[pixel_index + 3]);

	// Check the # of Effects
	CHECK_EQUAL(1, (int)c10.Effects().size());


	// Add a 2nd negate effect
	Negate n1;
	c10.AddEffect(&n1);

	// Get frame 1
	f = c10.GetFrame(500);

	// Get the image data
	pixels = f->GetPixels(10);
	pixel_index = 112 * 4; // pixel 112 (4 bytes per pixel)

	// Check image properties on scanline 10, pixel 112
	CHECK_EQUAL(0, (int)pixels[pixel_index]);
	CHECK_EQUAL(0, (int)pixels[pixel_index + 1]);
	CHECK_EQUAL(0, (int)pixels[pixel_index + 2]);
	CHECK_EQUAL(255, (int)pixels[pixel_index + 3]);

	// Check the # of Effects
	CHECK_EQUAL(2, (int)c10.Effects().size());
}

TEST(Verify_Parent_Timeline)
{
	Timeline t1(640, 480, Fraction(30,1), 44100, 2, LAYOUT_STEREO);

	// Load clip with video
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	Clip c1(path.str());
	c1.Open();

	// Check size of frame image
	CHECK_EQUAL(c1.GetFrame(1)->GetImage()->width(), 1280);
	CHECK_EQUAL(c1.GetFrame(1)->GetImage()->height(), 720);

	// Add clip to timeline
	t1.AddClip(&c1);

	// Check size of frame image (with an associated timeline)
	CHECK_EQUAL(c1.GetFrame(1)->GetImage()->width(), 640);
	CHECK_EQUAL(c1.GetFrame(1)->GetImage()->height(), 480);
}

} // SUITE
