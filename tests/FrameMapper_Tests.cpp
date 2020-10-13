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
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(FrameMapper_Get_Valid_Frame)
{
	// Create a reader
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping between 24 fps and 29.97 fps using classic pulldown
	FrameMapper mapping(&r, Fraction(30000, 1001), PULLDOWN_CLASSIC, 22000, 2, LAYOUT_STEREO);

	try
	{
		// Should find this frame
		MappedFrame f = mapping.GetMappedFrame(125);
		CHECK(true); // success
	}
	catch (OutOfBoundsFrame &e)
	{
		// Unexpected failure to find frame
		CHECK(false);
	}
}

TEST(FrameMapper_Invalid_Frame_Too_Small)
{
	// Create a reader
	DummyReader r(Fraction(24,1), 720, 480, 22000, 2, 5.0);

	// Create mapping 24 fps and 29.97 fps
	FrameMapper mapping(&r, Fraction(30000, 1001), PULLDOWN_CLASSIC, 22000, 2, LAYOUT_STEREO);

	// Check invalid frame number
	CHECK_THROW(mapping.GetMappedFrame(0), OutOfBoundsFrame);

}

TEST(FrameMapper_24_fps_to_30_fps_Pulldown_Classic)
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

TEST(FrameMapper_24_fps_to_30_fps_Pulldown_Advanced)
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

TEST(FrameMapper_24_fps_to_30_fps_Pulldown_None)
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

TEST(FrameMapper_30_fps_to_24_fps_Pulldown_Classic)
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

TEST(FrameMapper_30_fps_to_24_fps_Pulldown_Advanced)
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

TEST(FrameMapper_30_fps_to_24_fps_Pulldown_None)
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

TEST(FrameMapper_resample_audio_48000_to_41000)
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

TEST(FrameMapper_AudioSample_Distribution)
{
	CacheMemory cache;
	int OFFSET = 0;
	float AMPLITUDE = 0.2;
	double ANGLE = 0.0;
	int NUM_SAMPLES = 100;
	//std::cout << "Starting Resample Test" << std::endl;

	for (int64_t frame_number = 1; frame_number <= 90; frame_number++)
	{

		// Create blank frame (with specific frame #, samples, and channels)
		// Sample count should be 44100 / 30 fps = 1470 samples per frame

		int sample_count = 1470;
		std::shared_ptr<openshot::Frame> f(new openshot::Frame(frame_number, sample_count, 2));

		// Create test samples with sin wave (predictable values)
		float *audio_buffer = new float[sample_count * 2];

		for (int sample_number = 0; sample_number < sample_count; sample_number++)
		{
			// Calculate sin wave
			// TODO: I'm using abs(), because calling AddAudio only seems to be adding the positive values and it's bizarre
			float sample_value = float(AMPLITUDE * sin(ANGLE) + OFFSET);
			audio_buffer[sample_number] = sample_value;//abs(sample_value);
			ANGLE += (2 * M_PI) / NUM_SAMPLES;

			// Add custom audio samples to Frame (bool replaceSamples, int destChannel, int destStartSample, const float* source,
			f->AddAudio(true, 0, 0, audio_buffer, sample_count, 1.0); // add channel 1
			f->AddAudio(true, 1, 0, audio_buffer, sample_count, 1.0); // add channel 2

			// Add test frame to dummy reader
			cache.Add(f);
		}
	}
	// Create a default fraction (should be 1/1)
	openshot::DummyReader r(openshot::Fraction(30, 1), 1920, 1080, 44100, 2, 30.0, &cache);
	r.info.has_audio = true;
	r.Open(); // Open the reader

	// Map to 24 fps, which should create a variable # of samples per frame
	///FrameMapper map(&r, Fraction(24, 1), PULLDOWN_NONE, 44100, 2, LAYOUT_STEREO);
	//map.info.has_audio = true;
	//map.Open();

	Timeline t1(1920, 1080, Fraction(24, 1), 44100, 2, LAYOUT_STEREO);

	Clip c1;
	c1.Reader(&r);
	c1.Layer(1);
	c1.Position(0.0);
	c1.Start(0.0);
	c1.End(10.0);

	// Create 2nd map to 24 fps, which should create a variable # of samples per frame

	//FrameMapper map2(&r, Fraction(24, 1), PULLDOWN_NONE, 44100, 2, LAYOUT_STEREO);

	//map2.info.has_audio = true;
	//map2.Open();

	Clip c2;
	c2.Reader(&r);
	c2.Layer(2);

	// Position 1 frame into the video, this should mis-align the audio and create situations
	// which overlapping Frame instances have different # of samples for the Timeline.
	// TODO: Moving to 0.0 position, to simplify this test for now

	c2.Position(0.041666667 * 14);
	c2.Start(1.0);
	c2.End(10.0);

	// Add clips

	t1.AddClip(&c1);
	t1.AddClip(&c2);
	
	std::string json_val = t1.Json();
	
	//std::cout << json_val << std::endl;

	//t1.SetJson(t1.Json());
	t1.Open();

	FFmpegWriter w("output-resample.mp4");

	// Set options
	w.SetAudioOptions("aac", 44100, 192000);
	w.SetVideoOptions("libx264", 1280, 720, Fraction(24,1), 5000000);

	// Open writer
	w.Open();

	w.WriteFrame(&t1, 5, 50); 

	//for (int64_t frame_number = 1; frame_number <= 90; frame_number++){
	//	w.WriteFrame(t1.GetFrame(frame_number));
	//}

	// Close writer & reader
	w.Close();

	//map.Close();
	//map2.Close();

	t1.Close();

	// Clean up
	cache.Clear();

	r.Close();
}
