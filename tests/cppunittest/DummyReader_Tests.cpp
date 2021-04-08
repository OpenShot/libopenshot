/**
 * @file
 * @brief Unit tests for openshot::DummyReader
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

#include <memory>

#include "UnitTest++.h"
// Prevent name clashes with juce::UnitTest
#define DONT_SET_USING_JUCE_NAMESPACE 1

#include "DummyReader.h"
#include "Exceptions.h"
#include "CacheMemory.h"
#include "Fraction.h"
#include "Frame.h"

SUITE (DummyReader) {

TEST (Default_Constructor) {
	openshot::DummyReader r;

	CHECK_EQUAL(false, r.IsOpen());
	CHECK_THROW(r.GetFrame(1), openshot::ReaderClosed);

	r.Open();

	// Default values
	CHECK_EQUAL(1280, r.info.width);
	CHECK_EQUAL(768, r.info.height);
	CHECK_EQUAL(24, r.info.fps.num);
	CHECK_EQUAL(1, r.info.fps.den);
	CHECK_EQUAL(44100, r.info.sample_rate);
	CHECK_EQUAL(2, r.info.channels);
	CHECK_EQUAL(30.0, r.info.duration);

	CHECK_EQUAL("DummyReader", r.Name());

	auto cache = r.GetCache();
	CHECK_EQUAL(true, cache == nullptr);
}

TEST (Constructor) {
	openshot::DummyReader r(openshot::Fraction(30, 1), 1920, 1080, 48000, 2, 60.0);
	r.Open();

	// Check values
	CHECK_EQUAL(1920, r.info.width);
	CHECK_EQUAL(1080, r.info.height);
	CHECK_EQUAL(30, r.info.fps.num);
	CHECK_EQUAL(1, r.info.fps.den);
	CHECK_EQUAL(48000, r.info.sample_rate);
	CHECK_EQUAL(2, r.info.channels);
	CHECK_EQUAL(60.0, r.info.duration);
}

TEST (Blank_Frame) {
	openshot::DummyReader r(openshot::Fraction(30, 1), 1920, 1080, 44100, 2, 30.0);
	r.Open();

	// Get a blank frame (because we have not passed a Cache object (full of Frame objects) to the constructor
	// Check values
	CHECK_EQUAL(1, r.GetFrame(1)->number);
	CHECK_EQUAL(1, r.GetFrame(1)->GetPixels(700)[700] == 0); // black pixel
	CHECK_EQUAL(1, r.GetFrame(1)->GetPixels(701)[701] == 0); // black pixel
}

TEST (Fake_Frame) {

	// Create cache object to hold test frames
	openshot::CacheMemory cache;

	// Let's create some test frames
	for (int64_t frame_number = 1; frame_number <= 30; frame_number++) {
		// Create blank frame (with specific frame #, samples, and channels)
		// Sample count should be 44100 / 30 fps = 1470 samples per frame
		int sample_count = 1470;
		auto f = std::make_shared<openshot::Frame>(frame_number, sample_count, 2);

		// Create test samples with incrementing value
		float *audio_buffer = new float[sample_count];
		for (int64_t sample_number = 0; sample_number < sample_count; sample_number++) {
			// Generate an incrementing audio sample value (just as an example)
			audio_buffer[sample_number] = float(frame_number) + (float(sample_number) / float(sample_count));
		}

		// Add custom audio samples to Frame (bool replaceSamples, int destChannel, int destStartSample, const float* source,
		f->AddAudio(true, 0, 0, audio_buffer, sample_count, 1.0); // add channel 1
		f->AddAudio(true, 1, 0, audio_buffer, sample_count, 1.0); // add channel 2

		// Add test frame to dummy reader
		cache.Add(f);
	}

	openshot::DummyReader r(openshot::Fraction(30, 1), 1920, 1080, 44100, 2, 30.0, &cache);
	r.Open();

	// Verify our artificial audio sample data is correct
	CHECK_EQUAL(1, r.GetFrame(1)->number);
	CHECK_EQUAL(1, r.GetFrame(1)->GetAudioSamples(0)[0]);
	CHECK_CLOSE(1.00068033, r.GetFrame(1)->GetAudioSamples(0)[1], 0.00001);
	CHECK_CLOSE(1.00136054, r.GetFrame(1)->GetAudioSamples(0)[2], 0.00001);
	CHECK_EQUAL(2, r.GetFrame(2)->GetAudioSamples(0)[0]);
	CHECK_CLOSE(2.00068033, r.GetFrame(2)->GetAudioSamples(0)[1], 0.00001);
	CHECK_CLOSE(2.00136054, r.GetFrame(2)->GetAudioSamples(0)[2], 0.00001);

	// Clean up
	cache.Clear();
	r.Close();
}

TEST (Invalid_Fake_Frame) {
	// Create fake frames (with specific frame #, samples, and channels)
	auto f1 = std::make_shared<openshot::Frame>(1, 1470, 2);
	auto f2 = std::make_shared<openshot::Frame>(2, 1470, 2);

	// Add test frames to cache object
	openshot::CacheMemory cache;
	cache.Add(f1);
	cache.Add(f2);

	openshot::DummyReader r(openshot::Fraction(30, 1), 1920, 1080, 44100, 2, 30.0, &cache);
	r.Open();

	// Verify exception
	CHECK_EQUAL(1, r.GetFrame(1)->number);
	CHECK_EQUAL(2, r.GetFrame(2)->number);
	CHECK_THROW(r.GetFrame(3)->number, openshot::InvalidFile);

	// Clean up
	cache.Clear();
	r.Close();
}


TEST(Json)
{
	openshot::DummyReader r1;
	openshot::DummyReader r2(openshot::Fraction(24, 1), 1280, 768, 44100, 2, 30.0);
	auto json1 = r1.Json();
	auto json2 = r2.JsonValue();
	auto json_string2 = json2.toStyledString();
	CHECK_EQUAL(json1, json_string2);
}

TEST(SetJson)
{
	openshot::DummyReader r1;
	std::stringstream json_stream;
	json_stream << R"json(
		{
			"width": 1920,
			"height": 1080,
			"fps": { "num": 15, "den": 1 },
			"duration": 15.0
		}
		)json";

	r1.SetJson(json_stream.str());
	CHECK_EQUAL(1920, r1.info.width);
	CHECK_EQUAL(1080, r1.info.height);
	CHECK_EQUAL(15, r1.info.fps.num);
	CHECK_EQUAL(1, r1.info.fps.den);
	CHECK_EQUAL(15.0, r1.info.duration);
}

}  // SUITE
