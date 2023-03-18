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

#include "openshot_catch.h"

#include <QColor>
#include <QImage>
#include <QSize>

#include "Clip.h"
#include "DummyReader.h"
#include "Enums.h"
#include "Exceptions.h"
#include "FFmpegReader.h"
#include "Frame.h"
#include "Fraction.h"
#include "FrameMapper.h"
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

TEST_CASE( "access frames past reader length", "[libopenshot][clip]" )
{
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

		delete[] audio_buffer;
	}

	// Create a dummy reader, with a pre-existing cache
	openshot::DummyReader r(openshot::Fraction(30, 1), 1920, 1080, 44100, 2, 1.0, &cache);
	r.Open(); // Open the reader

	openshot::Clip c1;
	c1.Reader(&r);
	c1.Open();

	// Get the last valid frame #
	std::shared_ptr<openshot::Frame> frame = c1.GetFrame(30);

	CHECK(frame->GetAudioSamples(0)[0] == Approx(30.0).margin(0.00001));
	CHECK(frame->GetAudioSamples(0)[600] == Approx(30.4081631).margin(0.00001));
	CHECK(frame->GetAudioSamples(0)[1200] == Approx(30.8163261).margin(0.00001));

	// Get the +1 past the end of the reader (should be audio silence)
	frame = c1.GetFrame(31);

	CHECK(frame->GetAudioSamples(0)[0] == Approx(0.0).margin(0.00001));
	CHECK(frame->GetAudioSamples(0)[600] == Approx(0.0).margin(0.00001));
	CHECK(frame->GetAudioSamples(0)[1200] == Approx(0.0).margin(0.00001));

	// Get the +2 past the end of the reader (should be audio silence)
	frame = c1.GetFrame(32);

	CHECK(frame->GetAudioSamples(0)[0] == Approx(0.0).margin(0.00001));
	CHECK(frame->GetAudioSamples(0)[600] == Approx(0.0).margin(0.00001));
	CHECK(frame->GetAudioSamples(0)[1200] == Approx(0.0).margin(0.00001));
}

TEST_CASE( "setting and clobbering readers", "[libopenshot][clip]" )
{
	// Create a dummy reader #1, with a pre-existing cache
	openshot::DummyReader r1(openshot::Fraction(24, 1), 1920, 1080, 44100, 2, 1.0);
	r1.Open(); // Open the reader

	// Create a dummy reader #2, with a pre-existing cache
	openshot::DummyReader r2(openshot::Fraction(30, 1), 1920, 1080, 44100, 2, 1.0);
	r2.Open(); // Open the reader

	// Create a clip with constructor (and an allocated internal reader A)
	std::stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";
	Clip c1(path.str());
	c1.Open();

	// Clobber allocated reader A with reader #1
	c1.Reader(&r1);

	// Clobber reader #1 with reader #2
	c1.Reader(&r2);

	// Clobber reader #2 with SetJson (allocated reader B)
	c1.SetJson("{\"reader\":{\"acodec\":\"raw\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channel_layout\":4,\"channels\":2,\"display_ratio\":{\"den\":9,\"num\":16},\"duration\":1.0,\"file_size\":\"8294400\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_single_image\":false,\"has_video\":true,\"height\":1080,\"interlaced_frame\":false,\"metadata\":{},\"pixel_format\":-1,\"pixel_ratio\":{\"den\":1,\"num\":1},\"sample_rate\":44100,\"top_field_first\":true,\"type\":\"DummyReader\",\"vcodec\":\"raw\",\"video_bit_rate\":0,\"video_length\":\"30\",\"video_stream_index\":-1,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":1920}}");

	// Clobber allocated reader B with reader 2
	c1.Reader(&r2);

	// Clobber reader 2 with reader 1
	c1.Reader(&r1);
}

TEST_CASE( "time remapping", "[libopenshot][clip]" )
{
	Fraction fps(23,1);
	Timeline t1(640, 480, fps, 44100, 2, LAYOUT_STEREO);

	// Load clip with video
	std::stringstream path;
	path << TEST_MEDIA_PATH << "piano.wav";

	Clip clip(path.str());
	int original_video_length = clip.Reader()->info.video_length;
	clip.Position(0.0);
	clip.Start(0.0);

	// Set time keyframe (4X speed REVERSE)
	clip.time.AddPoint(1, original_video_length, openshot::LINEAR);
	clip.time.AddPoint(original_video_length, 1.0, openshot::LINEAR);

	// TODO: clip.Duration() != clip.Reader->info.duration
	// Set clip length based on time-values
	if (clip.time.GetLength() > 1) {
		clip.End(clip.time.GetLength() / fps.ToDouble());
	} else {
		clip.End(clip.Reader()->info.duration);
	}
	
	// Add clip
	t1.AddClip(&clip);
	t1.Open();

	// Get frame
	int64_t clip_start_frame = (clip.Position() * fps.ToDouble()) + 1;
	int64_t clip_end_frame = clip_start_frame + clip.time.GetLength();
	if (clip.time.GetLength() == 1) {
		clip_end_frame = clip_start_frame + (clip.Duration() * fps.ToDouble());
	}

	// Loop through frames
	for (int64_t frame = clip_start_frame; frame <= clip_end_frame; frame++) {
		int expected_sample_count = Frame::GetSamplesPerFrame(frame, t1.info.fps,
															  t1.info.sample_rate,
															  t1.info.channels);

		std::shared_ptr<Frame> f = t1.GetFrame(frame);
		CHECK(expected_sample_count == f->GetAudioSamplesCount());
	}

	// Clear cache
	t1.ClearAllCache(true);

	// Loop again through frames
	// Time-remapping should start over (detect a gap)
	for (int64_t frame = clip_start_frame; frame <= clip_end_frame; frame++) {
		int expected_sample_count = Frame::GetSamplesPerFrame(frame, t1.info.fps,
															  t1.info.sample_rate,
															  t1.info.channels);

		std::shared_ptr<Frame> f = t1.GetFrame(frame);
		CHECK(expected_sample_count == f->GetAudioSamplesCount());
	}

	t1.Close();

}

TEST_CASE( "resample_audio_8000_to_48000_reverse", "[libopenshot][clip]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "sine.wav";
	openshot::FFmpegReader reader(path.str(), true);

	// Map to 24 fps, 2 channels stereo, 44100 sample rate
	FrameMapper map(&reader, Fraction(24,1), PULLDOWN_NONE, 48000, 2, LAYOUT_STEREO);
	map.Open();

	Clip clip;
	clip.Reader(&map);
	clip.Open();
	int original_video_length = clip.Reader()->info.video_length;

	clip.Position(0.0);
	clip.Start(0.0);

	// Set time keyframe (REVERSE direction using bezier curve)
	clip.time.AddPoint(1, original_video_length, openshot::LINEAR);
	clip.time.AddPoint(original_video_length, 1.0, openshot::BEZIER);

	// Loop again through frames
	// Time-remapping should start over (detect a gap)
	for (int64_t frame = 1; frame <= original_video_length; frame++) {
		int expected_sample_count = Frame::GetSamplesPerFrame(frame, map.info.fps,
															  map.info.sample_rate,
															  map.info.channels);

		std::shared_ptr<Frame> f = clip.GetFrame(frame);
		CHECK(expected_sample_count == f->GetAudioSamplesCount());
	}

	// Clear clip cache
	clip.GetCache()->Clear();

	// Loop again through frames
	// Time-remapping should start over (detect a gap)
	for (int64_t frame = 1; frame < original_video_length; frame++) {
		int expected_sample_count = Frame::GetSamplesPerFrame(frame, map.info.fps,
															  map.info.sample_rate,
															  map.info.channels);

		std::shared_ptr<Frame> f = clip.GetFrame(frame);
		CHECK(expected_sample_count == f->GetAudioSamplesCount());
	}

	// Close mapper
	map.Close();
	reader.Close();
	clip.Close();
}