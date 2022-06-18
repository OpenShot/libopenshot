/**
 * @file
 * @brief Unit tests for openshot::CacheMemory
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <memory>
#include <QDir>

#include "openshot_catch.h"

#include "CacheMemory.h"
#include "Frame.h"
#include "Json.h"

using namespace openshot;

TEST_CASE( "default constructor", "[libopenshot][cachememory]" )
{
	// Create cache object
	CacheMemory c;

	// Loop 50 times
	for (int i = 0; i < 50; i++)
	{
		// Add blank frame to the cache
		auto f = std::make_shared<Frame>();
		f->number = i;
		c.Add(f);
	}

	CHECK(c.Count() == 50); // Cache should have all frames, with no limit
	CHECK(c.GetMaxBytes() == 0); // Max frames should default to 0
}

TEST_CASE( "MaxBytes constructor", "[libopenshot][cachememory]" )
{
	// Create cache object (with a max of 5 previous items)
	CacheMemory c(250 * 1024);

	// Loop 20 times
	for (int i = 30; i > 0; i--)
	{
		// Add blank frame to the cache
		auto f = std::make_shared<Frame>(i, 320, 240, "#000000");
		f->AddColor(320, 240, "#000000");
		c.Add(f);
	}

	// Cache should have all 20
	CHECK(c.Count() == 20);

	// Add 10 frames again
	for (int i = 10; i > 0; i--)
	{
		// Add blank frame to the cache
		auto f = std::make_shared<Frame>(i, 320, 240, "#000000");
		f->AddColor(320, 240, "#000000");
		c.Add(f);
	}

	// Count should be 20, since we're more frames than can be cached.
	CHECK(c.Count() == 20);

	// Check which items the cache kept
	CHECK(c.GetFrame(1) != nullptr);
	CHECK(c.GetFrame(10) != nullptr);
	CHECK(c.GetFrame(11) != nullptr);
	CHECK(c.GetFrame(19) != nullptr);
	CHECK(c.GetFrame(20) != nullptr);
	CHECK(c.GetFrame(21) == nullptr);
	CHECK(c.GetFrame(30) == nullptr);
}

TEST_CASE( "Clear", "[libopenshot][cachememory]" )
{
	// Create cache object
	CacheMemory c(250 * 1024);

	// Loop 10 times
	for (int i = 0; i < 10; i++)
	{
		// Add blank frame to the cache
		auto f = std::make_shared<Frame>();
		f->number = i;
		c.Add(f);
	}

	// Cache should only have 10 items
	CHECK(c.Count() == 10);

	// Clear Cache
	c.Clear();

	// Cache should now have 0 items
	CHECK(c.Count() == 0);
}

TEST_CASE( "add duplicate Frames", "[libopenshot][cachememory]" )
{
	// Create cache object
	CacheMemory c(250 * 1024);

	// Loop 10 times
	for (int i = 0; i < 10; i++)
	{
		// Add blank frame to the cache (each frame is #1)
		auto f = std::make_shared<Frame>();
		c.Add(f);
	}

	// Cache should only have 1 items (since all frames were frame #1)
	CHECK(c.Count() == 1);
}

TEST_CASE( "check if Frame exists", "[libopenshot][cachememory]" )
{
	// Create cache object
	CacheMemory c(250 * 1024);

	// Loop 5 times
	for (int i = 1; i < 6; i++)
	{
		// Add blank frame to the cache
		auto f = std::make_shared<Frame>();
		f->number = i;
		c.Add(f);
	}

	// Check if certain frames exists (only 1-5 exist)
	CHECK(c.GetFrame(0) == nullptr);
	CHECK(c.GetFrame(1) != nullptr);
	CHECK(c.GetFrame(2) != nullptr);
	CHECK(c.GetFrame(3) != nullptr);
	CHECK(c.GetFrame(4) != nullptr);
	CHECK(c.GetFrame(5) != nullptr);
	CHECK(c.GetFrame(6) == nullptr);
}

TEST_CASE( "GetFrame", "[libopenshot][cachememory]" )
{
	// Create cache object
	CacheMemory c(250 * 1024);

	// Create 3 frames
	Frame *red = new Frame(1, 300, 300, "red");
	Frame *blue = new Frame(2, 400, 400, "blue");
	Frame *green = new Frame(3, 500, 500, "green");

	// Add frames to cache
	c.Add(std::shared_ptr<Frame>(red));
	c.Add(std::shared_ptr<Frame>(blue));
	c.Add(std::shared_ptr<Frame>(green));

	// Get frames
	CHECK(c.GetFrame(0) == nullptr);
	CHECK(c.GetFrame(4) == nullptr);

	// Check if certain frames exists (only 1-5 exist)
	CHECK(c.GetFrame(1)->number == 1);
	CHECK(c.GetFrame(2)->number == 2);
	CHECK(c.GetFrame(3)->number == 3);
}

TEST_CASE( "GetSmallest", "[libopenshot][cachememory]" )
{
	// Create cache object (with a max of 10 items)
	CacheMemory c(250 * 1024);

	// Create 3 frames
	auto red = std::make_shared<Frame>(1, 300, 300, "red");
	auto blue = std::make_shared<Frame>(2, 400, 400, "blue");
	auto green = std::make_shared<Frame>(3, 500, 500, "green");

	// Add frames to cache
	c.Add(red);
	c.Add(blue);

	// Check if frame 1 is the front
	CHECK(c.GetSmallestFrame()->number == 1);

	c.Add(green);

	// Check if frame 1 is STILL the front
	CHECK(c.GetSmallestFrame()->number == 1);

	c.Remove(1);

	// Check if frame 2 is now the front
	CHECK(c.GetSmallestFrame()->number == 2);
}

TEST_CASE( "Remove", "[libopenshot][cachememory]" )
{
	// Create cache object (with a max of 10 items)
	CacheMemory c(250 * 1024);

	// Create 3 frames
	auto red = std::make_shared<Frame>(1, 300, 300, "red");
	auto blue = std::make_shared<Frame>(2, 400, 400, "blue");
	auto green = std::make_shared<Frame>(3, 500, 500, "green");

	// Add frames to cache
	c.Add(red);
	c.Add(blue);
	c.Add(green);

	// Check if count is 3
	CHECK(c.Count() == 3);

	// Check if frame 2 exists
	CHECK(c.GetFrame(2) != nullptr);

	// Remove frame 2
	c.Remove(2);

	// Check if frame 2 exists
	CHECK(c.GetFrame(2) == nullptr);

	// Check if count is 2
	CHECK(c.Count() == 2);

	// Remove frame 1
	c.Remove(1);

	// Check if frame 1 exists
	CHECK(c.GetFrame(1) == nullptr);

	// Check if count is 1
	CHECK(c.Count() == 1);
}

TEST_CASE( "SetMaxBytes", "[libopenshot][cachememory]" )
{
	// Create cache object
	CacheMemory c;

	// Loop 20 times
	for (int i = 0; i < 20; i++)
	{
		// Add blank frame to the cache
		auto f = std::make_shared<Frame>();
		f->number = i;
		c.Add(f);
	}

	CHECK(c.GetMaxBytes() == 0); // Cache defaults max frames to -1, unlimited frames

	// Set max frames
	c.SetMaxBytes(8 * 1024);
	CHECK(c.GetMaxBytes() == 8 * 1024);

	// Set max frames
	c.SetMaxBytes(4 * 1024);
	CHECK(c.GetMaxBytes() == 4 * 1024);
}

TEST_CASE( "multiple remove", "[libopenshot][cachememory]" )
{
	// Create cache object (using platform /temp/ directory)
	CacheMemory c;

	// Add frames to disk cache
	for (int i = 1; i <= 20; i++)
	{
		// Add blank frame to the cache
		auto f = std::make_shared<Frame>();
		f->number = i;
		// Add some picture data
		f->AddColor(1280, 720, "Blue");
		f->ResizeAudio(2, 500, 44100, LAYOUT_STEREO);
		f->AddAudioSilence(500);
		c.Add(f);
	}

	CHECK(c.Count() == 20);

	// Remove a single frame
	c.Remove(17);
	CHECK(c.Count() == 19);

	// Remove a range of frames
	c.Remove(16, 18);
	CHECK(c.Count() == 17);

	// Remove all remaining frames
	c.Remove(1, 20);
	CHECK(c.Count() == 0);
}



TEST_CASE( "JSON", "[libopenshot][cachememory]" )
{
	// Create memory cache object
	CacheMemory c;

	// Add some frames (out of order)
	auto f3 = std::make_shared<Frame>(3, 1280, 720, "Blue", 500, 2);
	c.Add(f3);
	CHECK((int)c.JsonValue()["ranges"].size() == 1);
	CHECK(c.JsonValue()["version"].asString() == "1");

	// Add some frames (out of order)
	auto f1 = std::make_shared<Frame>(1, 1280, 720, "Blue", 500, 2);
	c.Add(f1);
	CHECK((int)c.JsonValue()["ranges"].size() == 2);
	CHECK(c.JsonValue()["version"].asString() == "2");

	// Add some frames (out of order)
	auto f2 = std::make_shared<Frame>(2, 1280, 720, "Blue", 500, 2);
	c.Add(f2);
	CHECK((int)c.JsonValue()["ranges"].size() == 1);
	CHECK(c.JsonValue()["version"].asString() == "3");

	// Add some frames (out of order)
	auto f5 = std::make_shared<Frame>(5, 1280, 720, "Blue", 500, 2);
	c.Add(f5);
	CHECK((int)c.JsonValue()["ranges"].size() == 2);
	CHECK(c.JsonValue()["version"].asString() == "4");

	// Add some frames (out of order)
	auto f4 = std::make_shared<Frame>(4, 1280, 720, "Blue", 500, 2);
	c.Add(f4);
	CHECK((int)c.JsonValue()["ranges"].size() == 1);
	CHECK(c.JsonValue()["version"].asString() == "5");

}
