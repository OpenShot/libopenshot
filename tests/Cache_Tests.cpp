/**
 * @file
 * @brief Unit tests for openshot::Cache
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
#include "../include/Json.h"

using namespace std;
using namespace openshot;

TEST(Cache_Default_Constructor)
{
	// Create cache object
	CacheMemory c;

	// Loop 50 times
	for (int i = 0; i < 50; i++)
	{
		// Add blank frame to the cache
		std::shared_ptr<Frame> f(new Frame());
		f->number = i;
		c.Add(f);
	}

	CHECK_EQUAL(50, c.Count()); // Cache should have all frames, with no limit
	CHECK_EQUAL(0, c.GetMaxBytes()); // Max frames should default to 0
}

TEST(Cache_Max_Bytes_Constructor)
{
	// Create cache object (with a max of 5 previous items)
	CacheMemory c(250 * 1024);

	// Loop 20 times
	for (int i = 30; i > 0; i--)
	{
		// Add blank frame to the cache
		std::shared_ptr<Frame> f(new Frame(i, 320, 240, "#000000"));
		f->AddColor(320, 240, "#000000");
		c.Add(f);
	}

	// Cache should have all 20
	CHECK_EQUAL(20, c.Count());

	// Add 10 frames again
	for (int i = 10; i > 0; i--)
	{
		// Add blank frame to the cache
		std::shared_ptr<Frame> f(new Frame(i, 320, 240, "#000000"));
		f->AddColor(320, 240, "#000000");
		c.Add(f);
	}

	// Count should be 20, since we're more frames than can be cached.
	CHECK_EQUAL(20, c.Count());

	// Check which items the cache kept
	CHECK_EQUAL(true, c.GetFrame(1) != NULL);
	CHECK_EQUAL(true, c.GetFrame(10) != NULL);
	CHECK_EQUAL(true, c.GetFrame(11) != NULL);
	CHECK_EQUAL(true, c.GetFrame(19) != NULL);
	CHECK_EQUAL(true, c.GetFrame(20) != NULL);
	CHECK_EQUAL(false, c.GetFrame(21) != NULL);
	CHECK_EQUAL(false, c.GetFrame(30) != NULL);
}

TEST(Cache_Clear)
{
	// Create cache object
	CacheMemory c(250 * 1024);

	// Loop 10 times
	for (int i = 0; i < 10; i++)
	{
		// Add blank frame to the cache
		std::shared_ptr<Frame> f(new Frame());
		f->number = i;
		c.Add(f);
	}

	// Cache should only have 10 items
	CHECK_EQUAL(10, c.Count());

	// Clear Cache
	c.Clear();

	// Cache should now have 0 items
	CHECK_EQUAL(0, c.Count());
}

TEST(Cache_Add_Duplicate_Frames)
{
	// Create cache object
	CacheMemory c(250 * 1024);

	// Loop 10 times
	for (int i = 0; i < 10; i++)
	{
		// Add blank frame to the cache (each frame is #1)
		std::shared_ptr<Frame> f(new Frame());
		c.Add(f);
	}

	// Cache should only have 1 items (since all frames were frame #1)
	CHECK_EQUAL(1, c.Count());
}

TEST(Cache_Check_If_Frame_Exists)
{
	// Create cache object
	CacheMemory c(250 * 1024);

	// Loop 5 times
	for (int i = 1; i < 6; i++)
	{
		// Add blank frame to the cache
		std::shared_ptr<Frame> f(new Frame());
		f->number = i;
		c.Add(f);
	}

	// Check if certain frames exists (only 1-5 exist)
	CHECK_EQUAL(false, c.GetFrame(0) != NULL);
	CHECK_EQUAL(true, c.GetFrame(1) != NULL);
	CHECK_EQUAL(true, c.GetFrame(2) != NULL);
	CHECK_EQUAL(true, c.GetFrame(3) != NULL);
	CHECK_EQUAL(true, c.GetFrame(4) != NULL);
	CHECK_EQUAL(true, c.GetFrame(5) != NULL);
	CHECK_EQUAL(false, c.GetFrame(6) != NULL);
}

TEST(Cache_GetFrame)
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
	CHECK_EQUAL(true, c.GetFrame(0) == NULL);
	CHECK_EQUAL(true, c.GetFrame(4) == NULL);

	// Check if certain frames exists (only 1-5 exist)
	CHECK_EQUAL(1, c.GetFrame(1)->number);
	CHECK_EQUAL(2, c.GetFrame(2)->number);
	CHECK_EQUAL(3, c.GetFrame(3)->number);
}

TEST(Cache_GetSmallest)
{
	// Create cache object (with a max of 10 items)
	CacheMemory c(250 * 1024);

	// Create 3 frames
	Frame *red = new Frame(1, 300, 300, "red");
	Frame *blue = new Frame(2, 400, 400, "blue");
	Frame *green = new Frame(3, 500, 500, "green");

	// Add frames to cache
	c.Add(std::shared_ptr<Frame>(red));
	c.Add(std::shared_ptr<Frame>(blue));
	c.Add(std::shared_ptr<Frame>(green));

	// Check if frame 1 is the front
	CHECK_EQUAL(1, c.GetSmallestFrame()->number);

	// Check if frame 1 is STILL the front
	CHECK_EQUAL(1, c.GetSmallestFrame()->number);

	// Erase frame 1
	c.Remove(1);

	// Check if frame 2 is the front
	CHECK_EQUAL(2, c.GetSmallestFrame()->number);
}

TEST(Cache_Remove)
{
	// Create cache object (with a max of 10 items)
	CacheMemory c(250 * 1024);

	// Create 3 frames
	Frame *red = new Frame(1, 300, 300, "red");
	Frame *blue = new Frame(2, 400, 400, "blue");
	Frame *green = new Frame(3, 500, 500, "green");

	// Add frames to cache
	c.Add(std::shared_ptr<Frame>(red));
	c.Add(std::shared_ptr<Frame>(blue));
	c.Add(std::shared_ptr<Frame>(green));

	// Check if count is 3
	CHECK_EQUAL(3, c.Count());

	// Check if frame 2 exists
	CHECK_EQUAL(true, c.GetFrame(2) != NULL);

	// Remove frame 2
	c.Remove(2);

	// Check if frame 2 exists
	CHECK_EQUAL(false, c.GetFrame(2) != NULL);

	// Check if count is 2
	CHECK_EQUAL(2, c.Count());

	// Remove frame 1
	c.Remove(1);

	// Check if frame 1 exists
	CHECK_EQUAL(false, c.GetFrame(1) != NULL);

	// Check if count is 1
	CHECK_EQUAL(1, c.Count());
}

TEST(Cache_Set_Max_Bytes)
{
	// Create cache object
	CacheMemory c;

	// Loop 20 times
	for (int i = 0; i < 20; i++)
	{
		// Add blank frame to the cache
		std::shared_ptr<Frame> f(new Frame());
		f->number = i;
		c.Add(f);
	}

	CHECK_EQUAL(0, c.GetMaxBytes()); // Cache defaults max frames to -1, unlimited frames

	// Set max frames
	c.SetMaxBytes(8 * 1024);
	CHECK_EQUAL(8 * 1024, c.GetMaxBytes());

	// Set max frames
	c.SetMaxBytes(4 * 1024);
	CHECK_EQUAL(4 * 1024, c.GetMaxBytes());
}

TEST(Cache_Multiple_Remove)
{
	// Create cache object (using platform /temp/ directory)
	CacheMemory c;

	// Add frames to disk cache
	for (int i = 1; i <= 20; i++)
	{
		// Add blank frame to the cache
		std::shared_ptr<Frame> f(new Frame());
		f->number = i;
		// Add some picture data
		f->AddColor(1280, 720, "Blue");
		f->ResizeAudio(2, 500, 44100, LAYOUT_STEREO);
		f->AddAudioSilence(500);
		c.Add(f);
	}

	// Should have 20 frames
	CHECK_EQUAL(20, c.Count());

	// Remove all 20 frames
	c.Remove(1, 20);

	// Should have 20 frames
	CHECK_EQUAL(0, c.Count());
}

TEST(CacheDisk_Set_Max_Bytes)
{
	// Create cache object (using platform /temp/ directory)
	CacheDisk c("", "PPM", 1.0, 0.25);

	// Add frames to disk cache
	for (int i = 0; i < 20; i++)
	{
		// Add blank frame to the cache
		std::shared_ptr<Frame> f(new Frame());
		f->number = i;
		// Add some picture data
		f->AddColor(1280, 720, "Blue");
		f->ResizeAudio(2, 500, 44100, LAYOUT_STEREO);
		f->AddAudioSilence(500);
		c.Add(f);
	}

	CHECK_EQUAL(0, c.GetMaxBytes()); // Cache defaults max frames to -1, unlimited frames

	// Set max frames
	c.SetMaxBytes(8 * 1024);
	CHECK_EQUAL(8 * 1024, c.GetMaxBytes());

	// Set max frames
	c.SetMaxBytes(4 * 1024);
	CHECK_EQUAL(4 * 1024, c.GetMaxBytes());

	// Read frames from disk cache
	std::shared_ptr<Frame> f = c.GetFrame(5);
	CHECK_EQUAL(320, f->GetWidth());
	CHECK_EQUAL(180, f->GetHeight());
	CHECK_EQUAL(2, f->GetAudioChannelsCount());
	CHECK_EQUAL(500, f->GetAudioSamplesCount());
	CHECK_EQUAL(LAYOUT_STEREO, f->ChannelsLayout());
	CHECK_EQUAL(44100, f->SampleRate());

	// Check count of cache
	CHECK_EQUAL(20, c.Count());

	// Clear cache
	c.Clear();

	// Check count of cache
	CHECK_EQUAL(0, c.Count());

	// Delete cache directory
	QDir path = QDir::tempPath() + QString("/preview-cache/");
	path.removeRecursively();
}

TEST(CacheDisk_Multiple_Remove)
{
	// Create cache object (using platform /temp/ directory)
	CacheDisk c("", "PPM", 1.0, 0.25);

	// Add frames to disk cache
	for (int i = 1; i <= 20; i++)
	{
		// Add blank frame to the cache
		std::shared_ptr<Frame> f(new Frame());
		f->number = i;
		// Add some picture data
		f->AddColor(1280, 720, "Blue");
		f->ResizeAudio(2, 500, 44100, LAYOUT_STEREO);
		f->AddAudioSilence(500);
		c.Add(f);
	}

	// Should have 20 frames
	CHECK_EQUAL(20, c.Count());

	// Remove all 20 frames
	c.Remove(1, 20);

	// Should have 20 frames
	CHECK_EQUAL(0, c.Count());

	// Delete cache directory
	QDir path = QDir::tempPath() + QString("/preview-cache/");
	path.removeRecursively();
}

TEST(CacheDisk_JSON)
{
	// Create cache object (using platform /temp/ directory)
	CacheDisk c("", "PPM", 1.0, 0.25);

	// Add some frames (out of order)
	std::shared_ptr<Frame> f3(new Frame(3, 1280, 720, "Blue", 500, 2));
	c.Add(f3);
	CHECK_EQUAL(1, c.JsonValue()["ranges"].size());
	CHECK_EQUAL("1", c.JsonValue()["version"].asString());

	// Add some frames (out of order)
	std::shared_ptr<Frame> f1(new Frame(1, 1280, 720, "Blue", 500, 2));
	c.Add(f1);
	CHECK_EQUAL(2, c.JsonValue()["ranges"].size());
	CHECK_EQUAL("2", c.JsonValue()["version"].asString());

	// Add some frames (out of order)
	std::shared_ptr<Frame> f2(new Frame(2, 1280, 720, "Blue", 500, 2));
	c.Add(f2);
	CHECK_EQUAL(1, c.JsonValue()["ranges"].size());
	CHECK_EQUAL("3", c.JsonValue()["version"].asString());

	// Add some frames (out of order)
	std::shared_ptr<Frame> f5(new Frame(5, 1280, 720, "Blue", 500, 2));
	c.Add(f5);
	CHECK_EQUAL(2, c.JsonValue()["ranges"].size());
	CHECK_EQUAL("4", c.JsonValue()["version"].asString());

	// Add some frames (out of order)
	std::shared_ptr<Frame> f4(new Frame(4, 1280, 720, "Blue", 500, 2));
	c.Add(f4);
	CHECK_EQUAL(1, c.JsonValue()["ranges"].size());
	CHECK_EQUAL("5", c.JsonValue()["version"].asString());

	// Delete cache directory
	QDir path = QDir::tempPath() + QString("/preview-cache/");
	path.removeRecursively();
}

TEST(CacheMemory_JSON)
{
	// Create memory cache object
	CacheMemory c;

	// Add some frames (out of order)
	std::shared_ptr<Frame> f3(new Frame(3, 1280, 720, "Blue", 500, 2));
	c.Add(f3);
	CHECK_EQUAL(1, c.JsonValue()["ranges"].size());
	CHECK_EQUAL("1", c.JsonValue()["version"].asString());

	// Add some frames (out of order)
	std::shared_ptr<Frame> f1(new Frame(1, 1280, 720, "Blue", 500, 2));
	c.Add(f1);
	CHECK_EQUAL(2, c.JsonValue()["ranges"].size());
	CHECK_EQUAL("2", c.JsonValue()["version"].asString());

	// Add some frames (out of order)
	std::shared_ptr<Frame> f2(new Frame(2, 1280, 720, "Blue", 500, 2));
	c.Add(f2);
	CHECK_EQUAL(1, c.JsonValue()["ranges"].size());
	CHECK_EQUAL("3", c.JsonValue()["version"].asString());

	// Add some frames (out of order)
	std::shared_ptr<Frame> f5(new Frame(5, 1280, 720, "Blue", 500, 2));
	c.Add(f5);
	CHECK_EQUAL(2, c.JsonValue()["ranges"].size());
	CHECK_EQUAL("4", c.JsonValue()["version"].asString());

	// Add some frames (out of order)
	std::shared_ptr<Frame> f4(new Frame(4, 1280, 720, "Blue", 500, 2));
	c.Add(f4);
	CHECK_EQUAL(1, c.JsonValue()["ranges"].size());
	CHECK_EQUAL("5", c.JsonValue()["version"].asString());

}