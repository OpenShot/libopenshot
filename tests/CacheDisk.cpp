/**
 * @file
 * @brief Unit tests for openshot::Cache
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

#include "CacheDisk.h"
#include "Frame.h"
#include "Json.h"

using namespace openshot;

TEST_CASE( "constructor", "[libopenshot][cachedisk]" )
{
	QDir temp_path = QDir::tempPath() + QString("/constructor/");

	// Create cache object
	CacheDisk c(temp_path.path().toStdString(), "PPM", 1.0, 0.25);

	for (int i = 0; i < 20; i++)
	{
		// Add blank frame to the cache
		auto f = std::make_shared<openshot::Frame>();
		f->number = i;
		c.Add(f);
	}

	CHECK(c.Count() == 20); // Cache should have all frames, with no limit
	CHECK(c.GetMaxBytes() == 0); // Max frames should default to 0

	// Clean up
	c.Clear();
	temp_path.removeRecursively();
}

TEST_CASE( "MaxBytes constructor", "[libopenshot][cachedisk]" )
{
	QDir temp_path = QDir::tempPath() + QString("/maxbytes-constructor/");

	// Create cache object
	CacheDisk c(temp_path.path().toStdString(), "PPM", 1.0, 0.25, 20 * 1024);

	CHECK(c.GetMaxBytes() == 20 * 1024);

	for (int i = 0; i < 20; i++)
	{
		// Add blank frame to the cache
		auto f = std::make_shared<openshot::Frame>();
		f->number = i;
		c.Add(f);
	}

	CHECK(c.Count() == 20);
	CHECK(c.GetMaxBytes() == 20 * 1024);

	// Clean up
	c.Clear();
	temp_path.removeRecursively();
}

TEST_CASE( "SetMaxBytes", "[libopenshot][cachedisk]" )
{
	QDir temp_path = QDir::tempPath() + QString("/set_max_bytes/");

	// Create cache object
	CacheDisk c(temp_path.path().toStdString(), "PPM", 1.0, 0.25);

	// Add frames to disk cache
	for (int i = 0; i < 20; i++)
	{
		// Add blank frame to the cache
		auto f = std::make_shared<openshot::Frame>();
		f->number = i;
		// Add some picture data
		f->AddColor(1280, 720, "Blue");
		f->ResizeAudio(2, 500, 44100, LAYOUT_STEREO);
		f->AddAudioSilence(500);
		c.Add(f);
	}

	CHECK(c.GetMaxBytes() == 0); // Cache defaults max frames to -1, unlimited frames

	// Set max frames
	c.SetMaxBytes(8 * 1024);
	CHECK(c.GetMaxBytes() == 8 * 1024);

	// Set max frames
	c.SetMaxBytes(4 * 1024);
	CHECK(c.GetMaxBytes() == 4 * 1024);

	// Read frames from disk cache
	auto f = c.GetFrame(5);
	CHECK(f->GetWidth() == 320);
	CHECK(f->GetHeight() == 180);
	CHECK(f->GetAudioChannelsCount() == 2);
	CHECK(f->GetAudioSamplesCount() == 500);
	CHECK(f->ChannelsLayout() == LAYOUT_STEREO);
	CHECK(f->SampleRate() == 44100);

	// Check count of cache
	CHECK(c.Count() == 20);

	// Clear cache
	c.Clear();

	// Check count of cache
	CHECK(c.Count() == 0);

	// Delete cache directory
	temp_path.removeRecursively();
}

TEST_CASE( "freshen frames", "[libopensoht][cachedisk]" )
{
	QDir temp_path = QDir::tempPath() + QString("/freshen-frames/");

	// Create cache object
	CacheDisk c(temp_path.path().toStdString(), "PPM", 1.0, 0.25);

	auto f1 = std::make_shared<Frame>(1, 1280, 1024, "#FRIST!");

	c.Add(f1);

	for(int i = 2; i <= 20; i++)
	{
		// Add blank frame to the cache
		auto f = std::make_shared<openshot::Frame>();
		f->number = i;
		// Add some picture data
		f->AddColor(1280, 720, "Blue");
		f->ResizeAudio(2, 500, 44100, LAYOUT_STEREO);
		f->AddAudioSilence(500);
		c.Add(f);
	}

	CHECK(c.Count() == 20);

	// Capture current size of cache
	auto start_bytes = c.GetBytes();

	// Re-add existing frame a few times
	for (int x = 0; x < 5; x++)
	{
		c.Add(f1);
	}

	// Check that size hasn't changed
	CHECK(c.Count() == 20);
	CHECK(c.GetBytes() == start_bytes);

	// Clean up
	c.Clear();
	temp_path.removeRecursively();
}

TEST_CASE( "multiple remove", "[libopenshot][cachedisk]" )
{
	QDir temp_path = QDir::tempPath() + QString("/multiple-remove/");
	CacheDisk c(temp_path.path().toStdString(), "PPM", 1.0, 0.25);

	// Add frames to disk cache
	for (int i = 1; i <= 20; i++)
	{
		// Add blank frame to the cache
		auto f = std::make_shared<openshot::Frame>();
		f->number = i;
		// Add some picture data
		f->AddColor(1280, 720, "Blue");
		f->ResizeAudio(2, 500, 44100, LAYOUT_STEREO);
		f->AddAudioSilence(500);
		c.Add(f);
	}

	CHECK(c.Count() == 20);

	// Remove a single frame
	c.Remove(5);
	CHECK(c.Count() == 19);

	// Remove a range of frames
	c.Remove(4, 20);
	CHECK(c.Count() == 3);

	// Remove the rest
	c.Remove(1, 3);
	CHECK(c.Count() == 0);

	// Delete cache directory
	temp_path.removeRecursively();
}

TEST_CASE( "JSON", "[libopenshot][cachedisk]" )
{
	QDir temp_path = QDir::tempPath() + QString("/cache_json/");

	// Create cache object
	CacheDisk c(temp_path.path().toStdString(), "PPM", 1.0, 0.25);

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

	// Delete cache directory
	c.Clear();
	temp_path.removeRecursively();
}
