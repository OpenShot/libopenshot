/**
 * @file
 * @brief Unit tests for openshot::FrameMapper
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

#include "UnitTest++.h"
// Prevent name clashes with juce::UnitTest
#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "CacheMemory.h"
#include "Clip.h"
#include "DummyReader.h"
#include "FFmpegReader.h"
#include "Fraction.h"
#include "Frame.h"
#include "FrameMapper.h"
#include "Timeline.h"

using namespace std;
using namespace openshot;

SUITE(FrameMapper) {

TEST(NoOp_GetMappedFrame)
{
	// Create a reader
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping between 24 fps and 24 fps without pulldown
	FrameMapper mapping(&r, Fraction(24, 1), PULLDOWN_NONE, 22000, 2, LAYOUT_STEREO);
	CHECK_EQUAL("FrameMapper", mapping.Name());

	// Should find this frame
	MappedFrame f = mapping.GetMappedFrame(100);
	CHECK_EQUAL(100, f.Odd.Frame);
	CHECK_EQUAL(100, f.Even.Frame);

	// Should return end frame
	f = mapping.GetMappedFrame(150);
	CHECK_EQUAL(120, f.Odd.Frame);
	CHECK_EQUAL(120, f.Even.Frame);

	mapping.Close();
	mapping.Reader(nullptr);
	CHECK_THROW(mapping.Reader(), ReaderClosed);
}

TEST(Invalid_Frame_Too_Small)
{
	// Create a reader
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 29.97 fps
	FrameMapper mapping(&r, Fraction(30000, 1001), PULLDOWN_CLASSIC, 22000, 2, LAYOUT_STEREO);

	// Check invalid frame number
	CHECK_THROW(mapping.GetMappedFrame(0), OutOfBoundsFrame);

}

TEST(24_fps_to_30_fps_Pulldown_Classic)
{
	// Create a reader
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 30 fps
	FrameMapper mapping(&r, Fraction(30, 1), PULLDOWN_CLASSIC, 22000, 2, LAYOUT_STEREO);
	MappedFrame frame2 = mapping.GetMappedFrame(2);
	MappedFrame frame3 = mapping.GetMappedFrame(3);

	// Check for 3 fields of frame 2
	CHECK_EQUAL(2, frame2.Odd.Frame);
	CHECK_EQUAL(2, frame2.Even.Frame);
	CHECK_EQUAL(2, frame3.Odd.Frame);
	CHECK_EQUAL(3, frame3.Even.Frame);
}

TEST(24_fps_to_30_fps_Pulldown_Advanced)
{
	// Create a reader
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 30 fps
	FrameMapper mapping(&r, Fraction(30, 1), PULLDOWN_ADVANCED, 22000, 2, LAYOUT_STEREO);
	MappedFrame frame2 = mapping.GetMappedFrame(2);
	MappedFrame frame3 = mapping.GetMappedFrame(3);
	MappedFrame frame4 = mapping.GetMappedFrame(4);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK_EQUAL(2, frame2.Odd.Frame);
	CHECK_EQUAL(2, frame2.Even.Frame);
	CHECK_EQUAL(2, frame3.Odd.Frame);
	CHECK_EQUAL(3, frame3.Even.Frame);
	CHECK_EQUAL(3, frame4.Odd.Frame);
	CHECK_EQUAL(3, frame4.Even.Frame);
}

TEST(24_fps_to_30_fps_Pulldown_None)
{
	// Create a reader
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 30 fps
	FrameMapper mapping(&r, Fraction(30, 1), PULLDOWN_NONE, 22000, 2, LAYOUT_STEREO);
	MappedFrame frame4 = mapping.GetMappedFrame(4);
	MappedFrame frame5 = mapping.GetMappedFrame(5);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK_EQUAL(4, frame4.Odd.Frame);
	CHECK_EQUAL(4, frame4.Even.Frame);
	CHECK_EQUAL(4, frame5.Odd.Frame);
	CHECK_EQUAL(4, frame5.Even.Frame);
}

TEST(30_fps_to_24_fps_Pulldown_Classic)
{
	// Create a reader
	DummyReader r(Fraction(30, 1), 720, 480, 22000, 2, 5.0);

	// Create mapping between 30 fps and 24 fps
	FrameMapper mapping(&r, Fraction(24, 1), PULLDOWN_CLASSIC, 22000, 2, LAYOUT_STEREO);
	MappedFrame frame3 = mapping.GetMappedFrame(3);
	MappedFrame frame4 = mapping.GetMappedFrame(4);
	MappedFrame frame5 = mapping.GetMappedFrame(5);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK_EQUAL(4, frame3.Odd.Frame);
	CHECK_EQUAL(3, frame3.Even.Frame);
	CHECK_EQUAL(5, frame4.Odd.Frame);
	CHECK_EQUAL(4, frame4.Even.Frame);
	CHECK_EQUAL(6, frame5.Odd.Frame);
	CHECK_EQUAL(6, frame5.Even.Frame);
}

TEST(30_fps_to_24_fps_Pulldown_Advanced)
{
	// Create a reader
	DummyReader r(Fraction(30, 1), 720, 480, 22000, 2, 5.0);

	// Create mapping between 30 fps and 24 fps
	FrameMapper mapping(&r, Fraction(24, 1), PULLDOWN_ADVANCED, 22000, 2, LAYOUT_STEREO);
	MappedFrame frame2 = mapping.GetMappedFrame(2);
	MappedFrame frame3 = mapping.GetMappedFrame(3);
	MappedFrame frame4 = mapping.GetMappedFrame(4);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK_EQUAL(2, frame2.Odd.Frame);
	CHECK_EQUAL(2, frame2.Even.Frame);
	CHECK_EQUAL(4, frame3.Odd.Frame);
	CHECK_EQUAL(4, frame3.Even.Frame);
	CHECK_EQUAL(5, frame4.Odd.Frame);
	CHECK_EQUAL(5, frame4.Even.Frame);
}

TEST(30_fps_to_24_fps_Pulldown_None)
{
	// Create a reader
	DummyReader r(Fraction(30, 1), 720, 480, 22000, 2, 5.0);

	// Create mapping between 30 fps and 24 fps
	FrameMapper mapping(&r, Fraction(24, 1), PULLDOWN_NONE, 22000, 2, LAYOUT_STEREO);
	MappedFrame frame4 = mapping.GetMappedFrame(4);
	MappedFrame frame5 = mapping.GetMappedFrame(5);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK_EQUAL(4, frame4.Odd.Frame);
	CHECK_EQUAL(4, frame4.Even.Frame);
	CHECK_EQUAL(6, frame5.Odd.Frame);
	CHECK_EQUAL(6, frame5.Even.Frame);
}

TEST(resample_audio_48000_to_41000)
{
	// Create a reader: 24 fps, 2 channels, 48000 sample rate
	stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());

	// Map to 30 fps, 3 channels surround, 44100 sample rate
	FrameMapper map(&r, Fraction(30,1), PULLDOWN_NONE, 44100, 3, LAYOUT_SURROUND);
	map.Open();

	// Check details
	CHECK_EQUAL(3, map.GetFrame(1)->GetAudioChannelsCount());
	CHECK_EQUAL(1470, map.GetFrame(1)->GetAudioSamplesCount());
	CHECK_EQUAL(1470, map.GetFrame(2)->GetAudioSamplesCount());
	CHECK_EQUAL(1470, map.GetFrame(50)->GetAudioSamplesCount());

	// Change mapping data
	map.ChangeMapping(Fraction(25,1), PULLDOWN_NONE, 22050, 1, LAYOUT_MONO);

	// Check details
	CHECK_EQUAL(1, map.GetFrame(1)->GetAudioChannelsCount());
	CHECK_CLOSE(882, map.GetFrame(1)->GetAudioSamplesCount(), 10.0);
	CHECK_CLOSE(882, map.GetFrame(2)->GetAudioSamplesCount(), 10.0);
	CHECK_CLOSE(882, map.GetFrame(50)->GetAudioSamplesCount(), 10.0);

	// Close mapper
	map.Close();
}

TEST(resample_audio_mapper) {
	// This test verifies that audio data can be resampled on FrameMapper
	// instances, even on frame rates that do not divide evenly, and that no audio data is misplaced
	// or duplicated. We verify this by creating a SIN wave, add those data points to a DummyReader,
	// and then resample, and compare the result back to the original SIN wave calculation.

	// Create cache object to hold test frames
	CacheMemory cache;

	int OFFSET = 0;
	float AMPLITUDE = 0.75;
	double ANGLE = 0.0;
	int NUM_SAMPLES = 100;

	// Let's create some test frames
	for (int64_t frame_number = 1; frame_number <= 90; frame_number++) {
		// Create blank frame (with specific frame #, samples, and channels)
		// Sample count should be 44100 / 30 fps = 1470 samples per frame
		int sample_count = 1470;
		std::shared_ptr<openshot::Frame> f(new openshot::Frame(frame_number, sample_count, 2));

		// Create test samples with sin wave (predictable values)
		float *audio_buffer = new float[sample_count * 2];
		for (int sample_number = 0; sample_number < sample_count; sample_number++)
		{
			// Calculate sin wave
			float sample_value = float(AMPLITUDE * sin(ANGLE) + OFFSET);
			audio_buffer[sample_number] = abs(sample_value);
			ANGLE += (2 * M_PI) / NUM_SAMPLES;
		}

		// Add custom audio samples to Frame (bool replaceSamples, int destChannel, int destStartSample, const float* source,
		f->AddAudio(true, 0, 0, audio_buffer, sample_count, 1.0); // add channel 1
		f->AddAudio(true, 1, 0, audio_buffer, sample_count, 1.0); // add channel 2

		// Add test frame to dummy reader
		cache.Add(f);
	}

	// Create a default fraction (should be 1/1)
	openshot::DummyReader r(openshot::Fraction(30, 1), 1920, 1080, 44100, 2, 30.0, &cache);
	r.Open(); // Open the reader

	// Sample rates
	vector<int> arr = { 44100, 16000 };
	for (auto& rate : arr) {
		// Reset SIN wave
		ANGLE = 0.0;

		// Map to 24 fps, which should create a variable # of samples per frame
		FrameMapper map(&r, Fraction(24,1), PULLDOWN_NONE, rate, 2, LAYOUT_STEREO);
		map.info.has_audio = true;
		map.Open();

		// Calculating how much the initial sample rate has changed
		float resample_multiplier = ((float) rate / r.info.sample_rate);

		// Loop through samples, and verify FrameMapper didn't mess up individual sample values
		int num_samples = 0;
		for (int frame_index = 1; frame_index <= map.info.fps.ToInt(); frame_index++) {
			int sample_count = map.GetFrame(frame_index)->GetAudioSamplesCount();
			for (int sample_index = 0; sample_index < sample_count; sample_index++) {

				// Calculate sin wave
				float sample_value = abs(float(AMPLITUDE * sin(ANGLE) + OFFSET));
				ANGLE += (2 * M_PI) / (NUM_SAMPLES * resample_multiplier);

				// Verify each mapped sample value is correct (after being redistributed by the FrameMapper)
				float resampled_value = map.GetFrame(frame_index)->GetAudioSample(0, sample_index, 1.0);

				// TODO: 0.1 is much to broad to accurately test this, but without this, all the resampled values are too far away from expected
				CHECK_CLOSE(sample_value, resampled_value, 0.1);
			}
			// Increment sample value
			num_samples += map.GetFrame(frame_index)->GetAudioSamplesCount();
		}

		// Verify samples per second is correct (i.e. 44100)
		CHECK_EQUAL(num_samples, map.info.sample_rate);

		// Create Timeline (same specs as reader)
		Timeline t1(map.info.width, map.info.height, map.info.fps, rate, map.info.channels, map.info.channel_layout);

		Clip c1;
		c1.Reader(&map);
		c1.Layer(1);
		c1.Position(0.0);
		c1.Start(0.0);
		c1.End(10.0);

		// Create 2nd map to 24 fps, which should create a variable # of samples per frame (for some sample rates)
		FrameMapper map2(&r, Fraction(24, 1), PULLDOWN_NONE, rate, 2, LAYOUT_STEREO);
		map2.info.has_audio = true;
		map2.Open();

		Clip c2;
		c2.Reader(&map2);
		c2.Layer(1);
		c2.Position(0.0);
		c2.Start(0.0);
		c2.End(10.0);

		// Add clips
		t1.AddClip(&c1);
		t1.AddClip(&c2);
		t1.Open();

		// Reset SIN wave
		ANGLE = 0.0;

		for (int frame_index = 1; frame_index < 24; frame_index++) {
			t1.GetFrame(frame_index);
			for (int sample_index = 0; sample_index < t1.GetFrame(frame_index)->GetAudioSamplesCount(); sample_index++) {

				// Calculate sin wave
				float sample_value = abs(float(AMPLITUDE * sin(ANGLE) + OFFSET));
				ANGLE += (2 * M_PI) / (NUM_SAMPLES * resample_multiplier);

				// Verify each mapped sample value is correct (after being redistributed by the FrameMapper)
				float resampled_value = t1.GetFrame(frame_index)->GetAudioSample(0, sample_index, 1.0);

				// TODO: 0.1 is much to broad to accurately test this, but without this, all the resampled values are too far away from expected
				// Testing wave value X 2, since we have 2 overlapping clips
				CHECK_CLOSE(sample_value * 2.0, resampled_value, 0.1);

			}
		}

		// Close mapper
		map.Close();
		map2.Close();
		t1.Close();
	}

	// Clean up
	cache.Clear();
	r.Close();
}

TEST(redistribute_samples_per_frame) {
	// This test verifies that audio data is correctly aligned on
	// FrameMapper instances. We do this by creating 2 Clips based on the same parent reader
	// (i.e. same exact audio sample data). We use a Timeline to overlap these clips
	// (and offset 1 clip by 1 frame), and we verify that the correct # of samples is returned by each
	// Clip Frame instance. In the past, FrameMappers would sometimes generate the wrong # of samples
	// in a frame, and the Timeline recieve mismatching # of audio samples from 2 or more clips...
	// causing audio data to be truncated and lost (i.e. creating a pop).

	// Create cache object to hold test frames
	CacheMemory cache;

	// Let's create some test frames
	int sample_value = 0;
	for (int64_t frame_number = 1; frame_number <= 90; frame_number++) {
		// Create blank frame (with specific frame #, samples, and channels)
		// Sample count should be 44100 / 30 fps = 1470 samples per frame
		int sample_count = 1470;
		std::shared_ptr<openshot::Frame> f(new openshot::Frame(frame_number, sample_count, 2));

		// Create test samples with incrementing value
		float *audio_buffer = new float[sample_count];
		for (int64_t sample_number = 0; sample_number < sample_count; sample_number++) {
			audio_buffer[sample_number] = sample_value + sample_number;
		}

		// Increment counter
		sample_value += sample_count;

		// Add custom audio samples to Frame (bool replaceSamples, int destChannel, int destStartSample, const float* source,
		f->AddAudio(true, 0, 0, audio_buffer, sample_count, 1.0); // add channel 1
		f->AddAudio(true, 1, 0, audio_buffer, sample_count, 1.0); // add channel 2

		// Add test frame to dummy reader
		cache.Add(f);
	}

	// Create a default fraction (should be 1/1)
	openshot::DummyReader r(openshot::Fraction(30, 1), 1920, 1080, 44100, 2, 30.0, &cache);
	r.Open(); // Open the reader

	// Sample rates
	vector<int> arr = { 24, 30, 60 };
	for (auto& fps : arr) {
		// Map to 24 fps, which should create a variable # of samples per frame
		FrameMapper map(&r, Fraction(fps,1), PULLDOWN_NONE, 44100, 2, LAYOUT_STEREO);
		map.info.has_audio = true;
		map.Open();

		// Loop through samples, and verify FrameMapper didn't mess up individual sample values
		sample_value = 0;
		for (int frame_index = 1; frame_index <= map.info.fps.ToInt(); frame_index++) {
			for (int sample_index = 0; sample_index < map.GetFrame(frame_index)->GetAudioSamplesCount(); sample_index++) {
				// Verify each mapped sample value is correct (after being redistributed by the FrameMapper)
				CHECK_EQUAL(sample_value + sample_index, map.GetFrame(frame_index)->GetAudioSample(0, sample_index, 1.0));
			}
			// Increment sample value
			sample_value += map.GetFrame(frame_index)->GetAudioSamplesCount();
		}

		// Verify samples per second is correct (i.e. 44100)
		CHECK_EQUAL(sample_value, map.info.sample_rate);

		// Create Timeline (same specs as reader)
		Timeline t1(map.info.width, map.info.height, map.info.fps, 44100, map.info.channels, map.info.channel_layout);

		Clip c1;
		c1.Reader(&map);
		c1.Layer(1);
		c1.Position(0.0);
		c1.Start(0.0);
		c1.End(10.0);

		// Create 2nd map to 24 fps, which should create a variable # of samples per frame
		FrameMapper map2(&r, Fraction(fps, 1), PULLDOWN_NONE, 44100, 2, LAYOUT_STEREO);
		map2.info.has_audio = true;
		map2.Open();

		Clip c2;
		c2.Reader(&map2);
		c2.Layer(1);
		// Position 1 frame into the video, this should mis-align the audio and create situations
		// which overlapping Frame instances have different # of samples for the Timeline.
		c2.Position(map2.info.video_timebase.ToFloat());
		c2.Start(0.0);
		c2.End(10.0);

		// Add clips
		t1.AddClip(&c1);
		t1.AddClip(&c2);
		t1.Open();

		// Loop through samples, and verify Timeline didn't mess up individual sample values
		int previous_sample_value = 0;
		for (int frame_index = 2; frame_index < 24; frame_index++) {
			t1.GetFrame(frame_index);
			for (int sample_index = 0; sample_index < t1.GetFrame(frame_index)->GetAudioSamplesCount(); sample_index++) {
				int sample_diff = t1.GetFrame(frame_index)->GetAudioSample(0, sample_index, 1.0) - previous_sample_value;
				if (previous_sample_value == 0) {
					sample_diff = 2;
				}

				// Check if sample_value - previous_value == 2
				// This should be true, because the DummyReader is added twice to the Timeline, and is overlapping
				// This should be an ever increasing linear curve, increasing by 2 each sample on the Timeline
				CHECK_EQUAL(2, sample_diff);

				// Set previous sample value
				previous_sample_value = t1.GetFrame(frame_index)->GetAudioSample(0, sample_index, 1.0);
			}
		}

		// Close mapper
		map.Close();
		map2.Close();
		t1.Close();
	}

	// Clean up
	cache.Clear();
	r.Close();
}

TEST(Json)
{
	DummyReader r(Fraction(30,1), 1280, 720, 48000, 2, 5.0);
	FrameMapper map(&r, Fraction(30, 1), PULLDOWN_NONE, 48000, 2, LAYOUT_STEREO);

	// Read JSON config & write it back again
	const std::string map_config = map.Json();
	map.SetJson(map_config);

	CHECK_EQUAL(48000, map.info.sample_rate);
	CHECK_EQUAL(30, map.info.fps.num);
}

}  // SUITE
