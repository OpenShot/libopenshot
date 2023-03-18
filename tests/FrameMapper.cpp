/**
 * @file
 * @brief Unit tests for openshot::FrameMapper
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include "CacheMemory.h"
#include "Clip.h"
#include "DummyReader.h"
#include "Exceptions.h"
#include "FFmpegReader.h"
#include "Fraction.h"
#include "Frame.h"
#include "FrameMapper.h"
#include "Timeline.h"

using namespace openshot;

TEST_CASE( "NoOp_GetMappedFrame", "[libopenshot][framemapper]" )
{
	// Create a reader
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping between 24 fps and 24 fps without pulldown
	FrameMapper mapping(&r, Fraction(24, 1), PULLDOWN_NONE, 22000, 2, LAYOUT_STEREO);
	CHECK(mapping.Name() == "FrameMapper");

	// Should find this frame
	MappedFrame f = mapping.GetMappedFrame(100);
	CHECK(f.Odd.Frame == 100);
	CHECK(f.Even.Frame == 100);

	// Should return end frame
	f = mapping.GetMappedFrame(150);
	CHECK(f.Odd.Frame == 120);
	CHECK(f.Even.Frame == 120);

	mapping.Close();
	mapping.Reader(nullptr);
	CHECK_THROWS_AS(mapping.Reader(), ReaderClosed);
}

TEST_CASE( "Invalid_Frame_Too_Small", "[libopenshot][framemapper]" )
{
	// Create a reader
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 29.97 fps
	FrameMapper mapping(&r, Fraction(30000, 1001), PULLDOWN_CLASSIC, 22000, 2, LAYOUT_STEREO);

	// Check invalid frame number
	CHECK_THROWS_AS(mapping.GetMappedFrame(0), OutOfBoundsFrame);

}

TEST_CASE( "24_fps_to_30_fps_Pulldown_Classic", "[libopenshot][framemapper]" )
{
	// Create a reader
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 30 fps
	FrameMapper mapping(&r, Fraction(30, 1), PULLDOWN_CLASSIC, 22000, 2, LAYOUT_STEREO);
	MappedFrame frame2 = mapping.GetMappedFrame(2);
	MappedFrame frame3 = mapping.GetMappedFrame(3);

	// Check for 3 fields of frame 2
	CHECK(frame2.Odd.Frame == 2);
	CHECK(frame2.Even.Frame == 2);
	CHECK(frame3.Odd.Frame == 2);
	CHECK(frame3.Even.Frame == 3);
}

TEST_CASE( "24_fps_to_30_fps_Pulldown_Advanced", "[libopenshot][framemapper]" )
{
	// Create a reader
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 30 fps
	FrameMapper mapping(&r, Fraction(30, 1), PULLDOWN_ADVANCED, 22000, 2, LAYOUT_STEREO);
	MappedFrame frame2 = mapping.GetMappedFrame(2);
	MappedFrame frame3 = mapping.GetMappedFrame(3);
	MappedFrame frame4 = mapping.GetMappedFrame(4);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK(frame2.Odd.Frame == 2);
	CHECK(frame2.Even.Frame == 2);
	CHECK(frame3.Odd.Frame == 2);
	CHECK(frame3.Even.Frame == 3);
	CHECK(frame4.Odd.Frame == 3);
	CHECK(frame4.Even.Frame == 3);
}

TEST_CASE( "24_fps_to_30_fps_Pulldown_None", "[libopenshot][framemapper]" )
{
	// Create a reader
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 30 fps
	FrameMapper mapping(&r, Fraction(30, 1), PULLDOWN_NONE, 22000, 2, LAYOUT_STEREO);
	MappedFrame frame4 = mapping.GetMappedFrame(4);
	MappedFrame frame5 = mapping.GetMappedFrame(5);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK(frame4.Odd.Frame == 4);
	CHECK(frame4.Even.Frame == 4);
	CHECK(frame5.Odd.Frame == 4);
	CHECK(frame5.Even.Frame == 4);
}

TEST_CASE( "30_fps_to_24_fps_Pulldown_Classic", "[libopenshot][framemapper]" )
{
	// Create a reader
	DummyReader r(Fraction(30, 1), 720, 480, 22000, 2, 5.0);

	// Create mapping between 30 fps and 24 fps
	FrameMapper mapping(&r, Fraction(24, 1), PULLDOWN_CLASSIC, 22000, 2, LAYOUT_STEREO);
	MappedFrame frame3 = mapping.GetMappedFrame(3);
	MappedFrame frame4 = mapping.GetMappedFrame(4);
	MappedFrame frame5 = mapping.GetMappedFrame(5);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK(frame3.Odd.Frame == 4);
	CHECK(frame3.Even.Frame == 3);
	CHECK(frame4.Odd.Frame == 5);
	CHECK(frame4.Even.Frame == 4);
	CHECK(frame5.Odd.Frame == 6);
	CHECK(frame5.Even.Frame == 6);
}

TEST_CASE( "30_fps_to_24_fps_Pulldown_Advanced", "[libopenshot][framemapper]" )
{
	// Create a reader
	DummyReader r(Fraction(30, 1), 720, 480, 22000, 2, 5.0);

	// Create mapping between 30 fps and 24 fps
	FrameMapper mapping(&r, Fraction(24, 1), PULLDOWN_ADVANCED, 22000, 2, LAYOUT_STEREO);
	MappedFrame frame2 = mapping.GetMappedFrame(2);
	MappedFrame frame3 = mapping.GetMappedFrame(3);
	MappedFrame frame4 = mapping.GetMappedFrame(4);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK(frame2.Odd.Frame == 2);
	CHECK(frame2.Even.Frame == 2);
	CHECK(frame3.Odd.Frame == 4);
	CHECK(frame3.Even.Frame == 4);
	CHECK(frame4.Odd.Frame == 5);
	CHECK(frame4.Even.Frame == 5);
}

TEST_CASE( "30_fps_to_24_fps_Pulldown_None", "[libopenshot][framemapper]" )
{
	// Create a reader
	DummyReader r(Fraction(30, 1), 720, 480, 22000, 2, 5.0);

	// Create mapping between 30 fps and 24 fps
	FrameMapper mapping(&r, Fraction(24, 1), PULLDOWN_NONE, 22000, 2, LAYOUT_STEREO);
	MappedFrame frame4 = mapping.GetMappedFrame(4);
	MappedFrame frame5 = mapping.GetMappedFrame(5);

	// Check for advanced pulldown (only 1 fake frame)
	CHECK(frame4.Odd.Frame == 4);
	CHECK(frame4.Even.Frame == 4);
	CHECK(frame5.Odd.Frame == 6);
	CHECK(frame5.Even.Frame == 6);
}

TEST_CASE( "resample_audio_48000_to_41000", "[libopenshot][framemapper]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sintel_trailer-720p.mp4";
	FFmpegReader r(path.str());

	// Map to 30 fps, 3 channels surround, 44100 sample rate
	FrameMapper map(&r, Fraction(30,1), PULLDOWN_NONE, 44100, 3, LAYOUT_SURROUND);
	map.Open();

	// Check details
	CHECK(map.GetFrame(1)->GetAudioChannelsCount() == 3);
	CHECK(map.GetFrame(1)->GetAudioSamplesCount() == 1470);
	CHECK(map.GetFrame(2)->GetAudioSamplesCount() == 1470);
	CHECK(map.GetFrame(50)->GetAudioSamplesCount() == 1470);
	CHECK(map.info.video_length == 1558);

	// Change mapping data
	map.ChangeMapping(Fraction(25,1), PULLDOWN_NONE, 22050, 1, LAYOUT_MONO);

	// Check details
	CHECK(map.GetFrame(1)->GetAudioChannelsCount() == 1);
	CHECK(map.GetFrame(1)->GetAudioSamplesCount() == Approx(882).margin(10.0));
	CHECK(map.GetFrame(2)->GetAudioSamplesCount() == Approx(882).margin(10.0));
	CHECK(map.GetFrame(50)->GetAudioSamplesCount() == Approx(882).margin(10.0));
	CHECK(map.info.video_length == 1299);

	// Close mapper
	map.Close();
}

TEST_CASE( "resample_audio_mapper", "[libopenshot][framemapper]" ) {
	// This test verifies that audio data can be resampled on FrameMapper
	// instances, even on frame rates that do not divide evenly, and that no audio data is misplaced
	// or duplicated. We verify this by creating a SIN wave, add those data points to a DummyReader,
	// and then resample, and compare the result back to the original SIN wave calculation.

	// Create cache object to hold test frames
	CacheMemory cache;

	const int OFFSET = 0;
	const float AMPLITUDE = 0.75;
	const int NUM_SAMPLES = 100;
	double angle = 0.0;

	// Let's create some test frames
	for (int64_t frame_number = 1; frame_number <= 90; frame_number++) {
		// Create blank frame (with specific frame #, samples, and channels)
		// Sample count should be 44100 / 30 fps = 1470 samples per frame
		int sample_count = 1470;
		auto f = std::make_shared<openshot::Frame>(frame_number, sample_count, 2);

		// Create test samples with sin wave (predictable values)
		float *audio_buffer = new float[sample_count * 2];
		for (int sample_number = 0; sample_number < sample_count; sample_number++)
		{
			// Calculate sin wave
			float sample_value = float(AMPLITUDE * sin(angle) + OFFSET);
			audio_buffer[sample_number] = abs(sample_value);
			angle += (2 * M_PI) / NUM_SAMPLES;
		}

		// Add custom audio samples to Frame (bool replaceSamples, int destChannel, int destStartSample, const float* source,
		f->AddAudio(true, 0, 0, audio_buffer, sample_count, 1.0); // add channel 1
		f->AddAudio(true, 1, 0, audio_buffer, sample_count, 1.0); // add channel 2

		// Add test frame to dummy reader
		cache.Add(f);

		delete[] audio_buffer;
	}

	// Create a dummy reader, with a pre-existing cache
	openshot::DummyReader r(openshot::Fraction(30, 1), 1, 1, 44100, 2, 30.0, &cache);
	r.Open(); // Open the reader

	// Sample rates
	std::vector<int> arr = { 44100, 16000 };
	for (auto& rate : arr) {
		// Reset SIN wave
		angle = 0.0;

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
				float sample_value = abs(float(AMPLITUDE * sin(angle) + OFFSET));
				angle += (2 * M_PI) / (NUM_SAMPLES * resample_multiplier);

				// Verify each mapped sample value is correct (after being redistributed by the FrameMapper)
				float resampled_value = map.GetFrame(frame_index)->GetAudioSample(0, sample_index, 1.0);

				// TODO: 0.1 is much to broad to accurately test this, but without this, all the resampled values are too far away from expected
				CHECK(resampled_value == Approx(sample_value).margin(0.1));
			}
			// Increment sample value
			num_samples += map.GetFrame(frame_index)->GetAudioSamplesCount();
		}

		// Verify samples per second is correct (i.e. 44100)
		CHECK(map.info.sample_rate == num_samples);

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
		angle = 0.0;

		for (int frame_index = 1; frame_index < 24; frame_index++) {
			auto f = t1.GetFrame(frame_index);
			auto sample_count = f->GetAudioSamplesCount();
			for (int i = 0; i < sample_count; i++) {

				// Calculate sin wave
				float sample_value = abs(float(AMPLITUDE * sin(angle) + OFFSET));
				angle += (2 * M_PI) / (NUM_SAMPLES * resample_multiplier);

				// Verify each mapped sample value is correct (after being redistributed by the FrameMapper)
				float resampled_value = f->GetAudioSample(0, i, 1.0);

				// TODO: 0.1 is much to broad to accurately test this, but without this, all the resampled values are too far away from expected
				// Testing wave value X 2, since we have 2 overlapping clips
				CHECK(resampled_value == Approx(sample_value * 2.0).margin(0.1));

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

TEST_CASE( "redistribute_samples_per_frame", "[libopenshot][framemapper]" ) {
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
		auto f = std::make_shared<openshot::Frame>(frame_number, sample_count, 2);

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

		delete[] audio_buffer;
	}

	// Create a dummy reader, with a pre-existing cache
	openshot::DummyReader r(openshot::Fraction(30, 1), 1920, 1080, 44100, 2, 30.0, &cache);
	r.Open(); // Open the reader

	// Sample rates
	std::vector<int> arr = { 24, 30, 60 };
	for (auto& fps : arr) {
		// Map to 24 fps, which should create a variable # of samples per frame
		FrameMapper map(&r, Fraction(fps,1), PULLDOWN_NONE, 44100, 2, LAYOUT_STEREO);
		map.info.has_audio = true;
		map.Open();

		// Loop through samples, and verify FrameMapper didn't mess up individual sample values
		sample_value = 0;
		for (int frame_index = 1; frame_index <= map.info.fps.ToInt(); frame_index++) {
			for (int i = 0; i < map.GetFrame(frame_index)->GetAudioSamplesCount(); i++) {
				// Verify each mapped sample value is correct (after being redistributed by the FrameMapper)
				CHECK(map.GetFrame(frame_index)->GetAudioSample(0, i, 1.0) == sample_value + i);
			}
			// Increment sample value
			sample_value += map.GetFrame(frame_index)->GetAudioSamplesCount();
		}

		// Verify samples per second is correct (i.e. 44100)
		CHECK(map.info.sample_rate == sample_value);

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
				CHECK(sample_diff == 2);

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

TEST_CASE( "Distribute samples", "[libopenshot][framemapper]" ) {
	// This test verifies that audio data can be redistributed correctly
	// between common and uncommon frame rates
	int sample_rate = 48000;
	int channels = 2;
	int num_seconds = 1;

	// Source frame rates (varies the # of samples per frame)
	std::vector<openshot::Fraction> rates = {
		openshot::Fraction(30,1),
		openshot::Fraction(24,1) ,
		openshot::Fraction(119,4),
		openshot::Fraction(30000,1001)
	};

	for (auto& frame_rate : rates) {
		// Init sin wave variables
		const int OFFSET = 0;
		const float AMPLITUDE = 0.75;
		const int NUM_SAMPLES = 100;
		double angle = 0.0;

		// Create cache object to hold test frames
		openshot::CacheMemory cache;

		// Let's create some test frames
		for (int64_t frame_number = 1; frame_number <= (frame_rate.ToFloat() * num_seconds * 2); ++frame_number) {
			// Create blank frame (with specific frame #, samples, and channels)
			int sample_count = openshot::Frame::GetSamplesPerFrame(frame_number, frame_rate, sample_rate, channels);
			auto f = std::make_shared<openshot::Frame>(frame_number, sample_count, channels);
			f->SampleRate(sample_rate);

			// Create test samples with sin wave (predictable values)
			float *audio_buffer = new float[sample_count * 2];
			for (int sample_number = 0; sample_number < sample_count; sample_number++) {
				// Calculate sin wave
				float sample_value = float(AMPLITUDE * sin(angle) + OFFSET);
				audio_buffer[sample_number] = abs(sample_value);
				angle += (2 * M_PI) / NUM_SAMPLES;
			}

			// Add custom audio samples to Frame (bool replaceSamples, int destChannel, int destStartSample, const float* source,
			f->AddAudio(true, 0, 0, audio_buffer, sample_count, 1.0); // add channel 1
			f->AddAudio(true, 1, 0, audio_buffer, sample_count, 1.0); // add channel 2

			// Add test frame to dummy reader
			cache.Add(f);

			delete[] audio_buffer;
		}

		openshot::DummyReader r(frame_rate, 1920, 1080, sample_rate, channels, 30.0, &cache);
		r.Open();

		// Target frame rates
	std::vector<openshot::Fraction> mapped_rates = {
			openshot::Fraction(30,1),
			openshot::Fraction(24,1),
			openshot::Fraction(119,4),
			openshot::Fraction(30000,1001)
		};
		for (auto &mapped_rate : mapped_rates) {
			// Reset SIN wave
			angle = 0.0;

			// Map to different fps
			FrameMapper map(&r, mapped_rate, PULLDOWN_NONE, sample_rate, channels, LAYOUT_STEREO);
			map.info.has_audio = true;
			map.Open();

			// Loop through samples, and verify FrameMapper didn't mess up individual sample values
			for (int frame_index = 1; frame_index <= (map.info.fps.ToInt() * num_seconds); frame_index++) {
				int sample_count = map.GetFrame(frame_index)->GetAudioSamplesCount();
				for (int sample_index = 0; sample_index < sample_count; sample_index++) {

					// Calculate sin wave
					float predicted_value = abs(float(AMPLITUDE * sin(angle) + OFFSET));
					angle += (2 * M_PI) / NUM_SAMPLES;

					// Verify each mapped sample value is correct (after being redistributed by the FrameMapper)
					float mapped_value = map.GetFrame(frame_index)->GetAudioSample(0, sample_index, 1.0);
					CHECK(predicted_value == Approx(mapped_value).margin(0.001));
				}
			}

			float clip_position = 3.77;
			int starting_clip_frame = round(clip_position * map.info.fps.ToFloat()) + 1;

			// Create Timeline (same specs as reader)
			Timeline t1(map.info.width, map.info.height, map.info.fps, map.info.sample_rate, map.info.channels,
						map.info.channel_layout);

			Clip c1;
			c1.Reader(&map);
			c1.Layer(1);
			c1.Position(clip_position);
			c1.Start(0.0);
			c1.End(10.0);

			// Add clips
			t1.AddClip(&c1);
			t1.Open();

			// Reset SIN wave
			angle = 0.0;

			for (int frame_index = starting_clip_frame; frame_index < (starting_clip_frame + (t1.info.fps.ToFloat() * num_seconds)); frame_index++) {
				for (int sample_index = 0; sample_index < t1.GetFrame(frame_index)->GetAudioSamplesCount(); sample_index++) {
					// Calculate sin wave
					float predicted_value = abs(float(AMPLITUDE * sin(angle) + OFFSET));
					angle += (2 * M_PI) / NUM_SAMPLES;

					// Verify each mapped sample value is correct (after being redistributed by the FrameMapper)
					float timeline_value = t1.GetFrame(frame_index)->GetAudioSample(0, sample_index, 1.0);
					CHECK(predicted_value == Approx(timeline_value).margin(0.001));
				}
			}

			// Close mapper
			map.Close();
			t1.Close();
		}

		// Clean up reader
		r.Close();
		cache.Clear();

	} // for rates
}

TEST_CASE( "PrintMapping", "[libopenshot][framemapper]" )
{
	const std::string expected(
		R"(Target frame #: 1 mapped to original frame #:	(1 odd, 1 even)
  - Audio samples mapped to frame 1:0 to frame 1:1599
Target frame #: 2 mapped to original frame #:	(2 odd, 2 even)
  - Audio samples mapped to frame 1:1600 to frame 2:1199
Target frame #: 3 mapped to original frame #:	(2 odd, 3 even)
  - Audio samples mapped to frame 2:1200 to frame 3:799
Target frame #: 4 mapped to original frame #:	(3 odd, 4 even)
  - Audio samples mapped to frame 3:800 to frame 4:399
Target frame #: 5 mapped to original frame #:	(4 odd, 4 even)
  - Audio samples mapped to frame 4:400 to frame 4:1999
Target frame #: 6 mapped to original frame #:	(5 odd, 5 even)
  - Audio samples mapped to frame 5:0 to frame 5:1599
Target frame #: 7 mapped to original frame #:	(6 odd, 6 even)
  - Audio samples mapped to frame 5:1600 to frame 6:1199
Target frame #: 8 mapped to original frame #:	(6 odd, 7 even)
  - Audio samples mapped to frame 6:1200 to frame 7:799
Target frame #: 9 mapped to original frame #:	(7 odd, 8 even)
  - Audio samples mapped to frame 7:800 to frame 8:399
Target frame #: 10 mapped to original frame #:	(8 odd, 8 even)
  - Audio samples mapped to frame 8:400 to frame 8:1999)");

	DummyReader r(Fraction(24,1), 720, 480, 48000, 2, 5.0);
	// Create mapping 24 fps and 30 fps
	FrameMapper mapping(
		&r, Fraction(30, 1), PULLDOWN_CLASSIC, 48000, 2, LAYOUT_STEREO);
	std::stringstream mapping_out;
	mapping.PrintMapping(&mapping_out);

	// Compare a [0, expected.size()) substring of output to expected
	CHECK(mapping_out.str().substr(0, expected.size()) == expected);
}

TEST_CASE( "Json", "[libopenshot][framemapper]" )
{
	DummyReader r(Fraction(30,1), 1280, 720, 48000, 2, 5.0);
	FrameMapper map(&r, Fraction(30, 1), PULLDOWN_NONE, 48000, 2, LAYOUT_STEREO);

	// Read JSON config & write it back again
	const std::string map_config = map.Json();
	map.SetJson(map_config);

	CHECK(map.info.sample_rate == 48000);
	CHECK(map.info.fps.num == 30);
}

TEST_CASE( "SampleRange", "[libopenshot][framemapper]")
{
	openshot::Fraction fps(30, 1);
	int sample_rate = 44100;
	int channels = 2;

	int64_t start_frame = 10;
	int start_sample = 0;
	int total_samples = Frame::GetSamplesPerFrame(start_frame, fps, sample_rate, channels);

	int64_t end_frame = 10;
	int end_sample = (total_samples - 1);

	SampleRange Samples = {start_frame, start_sample, end_frame, end_sample, total_samples };
	CHECK(Samples.frame_start == 10);
	CHECK(Samples.sample_start == 0);
	CHECK(Samples.frame_end == 10);
	CHECK(Samples.sample_end == 1469);

	// ------ RIGHT -------
	// Extend range to the RIGHT
	Samples.Extend(50, fps, sample_rate, channels, true);
	CHECK(Samples.frame_start == 10);
	CHECK(Samples.sample_start == 0);
	CHECK(Samples.frame_end == 11);
	CHECK(Samples.sample_end == 49);
	CHECK(Samples.total == total_samples + 50);

	// Shrink range from the RIGHT
	Samples.Shrink(50, fps, sample_rate, channels, true);
	CHECK(Samples.frame_start == 10);
	CHECK(Samples.sample_start == 0);
	CHECK(Samples.frame_end == 10);
	CHECK(Samples.sample_end == 1469);
	CHECK(Samples.total == total_samples);


	// ------ LEFT -------
	// Extend range to the LEFT
	Samples.Extend(50, fps, sample_rate, channels, false);
	CHECK(Samples.frame_start == 9);
	CHECK(Samples.sample_start == 1420);
	CHECK(Samples.frame_end == 10);
	CHECK(Samples.sample_end == 1469);
	CHECK(Samples.total == total_samples + 50);

	// Shrink range from the LEFT
	Samples.Shrink(50, fps, sample_rate, channels, false);
	CHECK(Samples.frame_start == 10);
	CHECK(Samples.sample_start == 0);
	CHECK(Samples.frame_end == 10);
	CHECK(Samples.sample_end == 1469);
	CHECK(Samples.total == total_samples);


	// ------ SHIFT -------
	// Shift range to the RIGHT
	Samples.Shift(50, fps, sample_rate, channels, true);
	CHECK(Samples.frame_start == 10);
	CHECK(Samples.sample_start == 50);
	CHECK(Samples.frame_end == 11);
	CHECK(Samples.sample_end == 49);
	CHECK(Samples.total == total_samples);

	// Shift range to the LEFT
	Samples.Shift(50, fps, sample_rate, channels, false);
	CHECK(Samples.frame_start == 10);
	CHECK(Samples.sample_start == 0);
	CHECK(Samples.frame_end == 10);
	CHECK(Samples.sample_end == 1469);
	CHECK(Samples.total == total_samples);


	// Shift range to the RIGHT
	Samples.Shift(50, fps, sample_rate, channels, true);
	CHECK(Samples.frame_start == 10);
	CHECK(Samples.sample_start == 50);
	CHECK(Samples.frame_end == 11);
	CHECK(Samples.sample_end == 49);
	CHECK(Samples.total == total_samples);

	// Shift range to the LEFT
	Samples.Shift(75, fps, sample_rate, channels, false);
	CHECK(Samples.frame_start == 9);
	CHECK(Samples.sample_start == 1445);
	CHECK(Samples.frame_end == 10);
	CHECK(Samples.sample_end == 1444);
	CHECK(Samples.total == total_samples);

	// Shift range to the RIGHT
	Samples.Shift(25, fps, sample_rate, channels, true);
	CHECK(Samples.frame_start == 10);
	CHECK(Samples.sample_start == 0);
	CHECK(Samples.frame_end == 10);
	CHECK(Samples.sample_end == 1469);
	CHECK(Samples.total == total_samples);
}