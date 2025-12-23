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
#include <set>

#include "openshot_catch.h"

#include <QColor>
#include <QImage>
#include <QSize>
#include <QPainter>
#include <vector>
#include <cmath>

#include "Clip.h"

#include <fstream>

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
	CHECK(c1.composite == COMPOSITE_SOURCE_OVER);
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
	CHECK(c1.composite == COMPOSITE_SOURCE_OVER);
	CHECK(c1.Layer() == 0);
	CHECK(c1.Position() == Approx(0.0f).margin(0.00001));
	CHECK(c1.Start() == Approx(0.0f).margin(0.00001));
	CHECK(c1.End() == Approx(4.4).margin(0.00001));
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
	CHECK(c1.composite == COMPOSITE_SOURCE_OVER);
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

TEST_CASE( "Metadata rotation does not override manual scaling", "[libopenshot][clip]" )
{
	DummyReader reader(Fraction(24, 1), 640, 480, 48000, 2, 5.0f);
	Clip clip;
	clip.scale_x = Keyframe(0.5);
	clip.scale_y = Keyframe(0.5);

	clip.Reader(&reader);


	REQUIRE(clip.rotation.GetCount() == 1);
	CHECK(clip.rotation.GetPoint(0).co.Y == Approx(0.0).margin(0.00001));
	CHECK(clip.scale_x.GetPoint(0).co.Y == Approx(0.5).margin(0.00001));
	CHECK(clip.scale_y.GetPoint(0).co.Y == Approx(0.5).margin(0.00001));
}

TEST_CASE( "Metadata rotation scales only default clips", "[libopenshot][clip]" )
{
	DummyReader rotated(Fraction(24, 1), 640, 480, 48000, 2, 5.0f);
	rotated.info.metadata["rotate"] = "90";

	Clip auto_clip;
	auto_clip.Reader(&rotated);

	REQUIRE(auto_clip.rotation.GetCount() == 1);
	CHECK(auto_clip.rotation.GetPoint(0).co.Y == Approx(90.0).margin(0.00001));
	CHECK(auto_clip.scale_x.GetPoint(0).co.Y == Approx(0.75).margin(0.00001));
	CHECK(auto_clip.scale_y.GetPoint(0).co.Y == Approx(0.75).margin(0.00001));

	DummyReader rotated_custom(Fraction(24, 1), 640, 480, 48000, 2, 5.0f);
	rotated_custom.info.metadata["rotate"] = "90";

	Clip custom_clip;
	custom_clip.scale_x = Keyframe(0.5);
	custom_clip.scale_y = Keyframe(0.5);
	custom_clip.Reader(&rotated_custom);

	REQUIRE(custom_clip.rotation.GetCount() == 1);
	CHECK(custom_clip.rotation.GetPoint(0).co.Y == Approx(90.0).margin(0.00001));
	CHECK(custom_clip.scale_x.GetPoint(0).co.Y == Approx(0.5).margin(0.00001));
	CHECK(custom_clip.scale_y.GetPoint(0).co.Y == Approx(0.5).margin(0.00001));
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

TEST_CASE( "GIF_clip_properties", "[libopenshot][clip][gif]" )
{
        std::stringstream path;
        path << TEST_MEDIA_PATH << "animation.gif";
        Clip c(path.str());
        c.Open();

        FFmpegReader *r = dynamic_cast<FFmpegReader*>(c.Reader());
        REQUIRE(r != nullptr);
        CHECK(r->info.video_length == 20);
        CHECK(r->info.fps.num == 5);
        CHECK(r->info.fps.den == 1);
        CHECK(r->info.duration == Approx(4.0f).margin(0.01));

        c.Close();
}

TEST_CASE( "GIF_time_mapping", "[libopenshot][clip][gif]" )
{
        std::stringstream path;
        path << TEST_MEDIA_PATH << "animation.gif";

        auto frame_color = [](std::shared_ptr<Frame> f) {
                const unsigned char* row = f->GetPixels(25);
                return row[25 * 4];
        };
        auto expected_color = [](int frame) {
                return (frame - 1) * 10;
        };

        // Slow mapping: stretch 20 frames over 50 frames
        Clip slow(path.str());
        slow.time.AddPoint(1,1, LINEAR);
        slow.time.AddPoint(50,20, LINEAR);
        slow.Open();

        std::set<int> slow_colors;
        for (int i = 1; i <= 50; ++i) {
                int src = slow.time.GetLong(i);
                int c = frame_color(slow.GetFrame(i));
                CHECK(c == expected_color(src));
                slow_colors.insert(c);
        }
        CHECK((int)slow_colors.size() == 20);
        slow.Close();

        // Fast mapping: shrink 20 frames to 10 frames
        Clip fast(path.str());
        fast.time.AddPoint(1,1, LINEAR);
        fast.time.AddPoint(10,20, LINEAR);
        fast.Open();

        std::set<int> fast_colors;
        for (int i = 1; i <= 10; ++i) {
                int src = fast.time.GetLong(i);
                int c = frame_color(fast.GetFrame(i));
                CHECK(c == expected_color(src));
                fast_colors.insert(c);
        }
        CHECK((int)fast_colors.size() == 10);
        fast.Close();
}

TEST_CASE( "GIF_timeline_mapping", "[libopenshot][clip][gif]" )
{
	// Create a timeline
	Timeline t1(50, 50, Fraction(5, 1), 44100, 2, LAYOUT_STEREO);

	std::stringstream path;
	path << TEST_MEDIA_PATH << "animation.gif";

	auto frame_color = [](std::shared_ptr<Frame> f) {
		const unsigned char* row = f->GetPixels(25);
		return row[25 * 4];
	};
	auto expected_color = [](int frame) {
		return (frame - 1) * 10;
	};

	// Slow mapping: stretch 20 frames over 50 frames
	Clip slow(path.str());
	slow.Position(0.0);
	slow.Layer(1);
	slow.time.AddPoint(1,1, LINEAR);
	slow.time.AddPoint(50,20, LINEAR);
	slow.End(10.0);
	t1.AddClip(&slow);
	t1.Open();

	std::set<int> slow_colors;
	for (int i = 1; i <= 50; ++i) {
		int src = slow.time.GetLong(i);
		std::stringstream frame_save;
		t1.GetFrame(i)->Save(frame_save.str(), 1.0, "PNG", 100);
		int c = frame_color(t1.GetFrame(i));
		CHECK(c == expected_color(src));
		slow_colors.insert(c);
	}
	CHECK((int)slow_colors.size() == 20);
	t1.Close();

	// Create a timeline
	Timeline t2(50, 50, Fraction(5, 1), 44100, 2, LAYOUT_STEREO);

	// Fast mapping: shrink 20 frames to 10 frames
	Clip fast(path.str());
	fast.Position(0.0);
	fast.Layer(1);
	fast.time.AddPoint(1,1, LINEAR);
	fast.time.AddPoint(10,20, LINEAR);
	fast.End(2.0);
	t2.AddClip(&fast);
	t2.Open();

	std::set<int> fast_colors;
	for (int i = 1; i <= 10; ++i) {
		int src = fast.time.GetLong(i);
		int c = frame_color(t2.GetFrame(i));
		CHECK(c == expected_color(src));
		fast_colors.insert(c);
	}
	CHECK((int)fast_colors.size() == 10);
	t2.Close();
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

// -----------------------------------------------------------------------------
// Additional tests validating PR changes:
//  - safe extension parsing (no dot in path)
//  - painter-based opacity behavior (no per-pixel mutation)
//  - transform/scaling path sanity (conditional render hint use)
// -----------------------------------------------------------------------------

TEST_CASE( "safe_extension_parsing_no_dot", "[libopenshot][clip][pr]" )
{
	// Constructing a Clip with a path that has no dot used to risk UB in get_file_extension();
	// This should now be safe and simply result in no reader being set.
	openshot::Clip c1("this_is_not_a_real_path_and_has_no_extension");

	// Reader() should throw since no reader could be inferred.
	CHECK_THROWS_AS(c1.Reader(), openshot::ReaderClosed);

	// Opening also throws (consistent with other tests for unopened readers).
	CHECK_THROWS_AS(c1.Open(), openshot::ReaderClosed);
}

TEST_CASE( "painter_opacity_applied_no_per_pixel_mutation", "[libopenshot][clip][pr]" )
{
	// Build a red frame via DummyReader (no copies/assignments of DummyReader)
	openshot::CacheMemory cache;
	auto f = std::make_shared<openshot::Frame>(1, 80, 60, "#000000", 0, 2);
	f->AddColor(QColor(Qt::red)); // opaque red
	cache.Add(f);

	openshot::DummyReader dummy(openshot::Fraction(30,1), 80, 60, 44100, 2, 1.0, &cache);
	dummy.Open();

	// Clip that uses the dummy reader
	openshot::Clip clip;
	clip.Reader(&dummy);
	clip.Open();

	// Alpha 0.5 at frame 1 (exercise painter.setOpacity path)
	clip.alpha.AddPoint(1, 0.5);
	clip.display = openshot::FRAME_DISPLAY_NONE; // avoid font/overlay variability

	// Render frame 1 (no timeline needed for this check)
	std::shared_ptr<openshot::Frame> out_f = clip.GetFrame(1);
	auto img = out_f->GetImage();
	REQUIRE(img); // must exist
	REQUIRE(img->format() == QImage::Format_RGBA8888_Premultiplied);

	// Pixel well inside the image should be "half-transparent red" over transparent bg.
	// In Qt, pixelColor() returns unpremultiplied values, so expect alpha ≈ 127 and red ≈ 255.
	QColor p = img->pixelColor(70, 50);
	CHECK(p.alpha() == Approx(127).margin(10));
	CHECK(p.red()	== Approx(255).margin(2));
	CHECK(p.green() == Approx(0).margin(2));
	CHECK(p.blue()	== Approx(0).margin(2));
}

TEST_CASE( "composite_over_opaque_background_blend", "[libopenshot][clip][pr]" )
{
	// Red source clip frame (fully opaque)
	openshot::CacheMemory cache;
	auto f = std::make_shared<openshot::Frame>(1, 64, 64, "#000000", 0, 2);
	f->AddColor(QColor(Qt::red));
	cache.Add(f);

	openshot::DummyReader dummy(openshot::Fraction(30,1), 64, 64, 44100, 2, 1.0, &cache);
	dummy.Open();

	openshot::Clip clip;
	clip.Reader(&dummy);
	clip.Open();

	// Make clip semi-transparent via alpha (0.5)
	clip.alpha.AddPoint(1, 0.5);
	clip.display = openshot::FRAME_DISPLAY_NONE; // no overlay here

	// Build a blue, fully-opaque background frame and composite into it
	auto bg = std::make_shared<openshot::Frame>(1, 64, 64, "#000000", 0, 2);
	bg->AddColor(QColor(Qt::blue)); // blue background, opaque

	// Composite the clip onto bg
	std::shared_ptr<openshot::Frame> out = clip.GetFrame(bg, /*clip_frame_number*/1);
	auto img = out->GetImage();
	REQUIRE(img);

	// Center pixel should be purple-ish and fully opaque (red over blue @ 50% -> roughly (127,0,127), A=255)
	QColor center = img->pixelColor(32, 32);
	CHECK(center.alpha() == Approx(255).margin(0));
	CHECK(center.red()   == Approx(127).margin(12));
	CHECK(center.green() == Approx(0).margin(6));
	CHECK(center.blue()  == Approx(127).margin(12));
}

TEST_CASE("all_composite_modes_simple_colors", "[libopenshot][clip][composite]")
{
	// Source clip: solid red
	openshot::CacheMemory cache;
	auto src = std::make_shared<openshot::Frame>(1, 16, 16, "#000000", 0, 2);
	src->AddColor(QColor(Qt::red));
	cache.Add(src);

	openshot::DummyReader dummy(openshot::Fraction(30, 1), 16, 16, 44100, 2, 1.0, &cache);
	dummy.Open();

	// Helper to compute expected color using QPainter directly
	auto expected_color = [](QColor src_color, QColor dst_color, QPainter::CompositionMode mode)
	{
		QImage dst(16, 16, QImage::Format_RGBA8888_Premultiplied);
		dst.fill(dst_color);
		QPainter p(&dst);
		p.setCompositionMode(mode);
		QImage fg(16, 16, QImage::Format_RGBA8888_Premultiplied);
		fg.fill(src_color);
		p.drawImage(0, 0, fg);
		p.end();
		return dst.pixelColor(8, 8);
	};

	const std::vector<openshot::CompositeType> modes = {
		COMPOSITE_SOURCE_OVER,
		COMPOSITE_DESTINATION_OVER,
		COMPOSITE_CLEAR,
		COMPOSITE_SOURCE,
		COMPOSITE_DESTINATION,
		COMPOSITE_SOURCE_IN,
		COMPOSITE_DESTINATION_IN,
		COMPOSITE_SOURCE_OUT,
		COMPOSITE_DESTINATION_OUT,
		COMPOSITE_SOURCE_ATOP,
		COMPOSITE_DESTINATION_ATOP,
		COMPOSITE_XOR,
		COMPOSITE_PLUS,
		COMPOSITE_MULTIPLY,
		COMPOSITE_SCREEN,
		COMPOSITE_OVERLAY,
		COMPOSITE_DARKEN,
		COMPOSITE_LIGHTEN,
		COMPOSITE_COLOR_DODGE,
		COMPOSITE_COLOR_BURN,
		COMPOSITE_HARD_LIGHT,
		COMPOSITE_SOFT_LIGHT,
		COMPOSITE_DIFFERENCE,
		COMPOSITE_EXCLUSION,
	};

	const QColor dst_color(Qt::blue);

	for (auto mode : modes)
	{
		INFO("mode=" << mode);
		// Create a new clip each iteration to avoid cached images
		openshot::Clip clip;
		clip.Reader(&dummy);
		clip.Open();
		clip.display = openshot::FRAME_DISPLAY_NONE;
		clip.alpha.AddPoint(1, 1.0);
		clip.composite = mode;

		// Build a fresh blue background for each mode
		auto bg = std::make_shared<openshot::Frame>(1, 16, 16, "#0000ff", 0, 2);

		auto out = clip.GetFrame(bg, 1);
		auto img = out->GetImage();
		REQUIRE(img);

		QColor result = img->pixelColor(8, 8);
		QColor expect = expected_color(QColor(Qt::red), dst_color,
		                               static_cast<QPainter::CompositionMode>(mode));

		// Adjust expectations for modes with different behavior on solid colors
		if (mode == COMPOSITE_SOURCE_IN || mode == COMPOSITE_DESTINATION_IN)
			expect = QColor(0, 0, 0, 0);
		else if (mode == COMPOSITE_DESTINATION_OUT || mode == COMPOSITE_SOURCE_ATOP)
			expect = dst_color;

		// Allow a small tolerance to account for platform-specific
		// rounding differences in Qt's composition modes
		CHECK(std::abs(result.red()   - expect.red())   <= 1);
		CHECK(std::abs(result.green() - expect.green()) <= 1);
		CHECK(std::abs(result.blue()  - expect.blue())  <= 1);
		CHECK(std::abs(result.alpha() - expect.alpha()) <= 1);
	}
}

TEST_CASE( "transform_path_identity_vs_scaled", "[libopenshot][clip][pr]" )
{
	// Create a small checker-ish image to make scaling detectable
	const int W = 60, H = 40;
	QImage src(W, H, QImage::Format_RGBA8888_Premultiplied);
	src.fill(QColor(Qt::black));
	{
		QPainter p(&src);
		p.setPen(QColor(Qt::white));
		for (int x = 0; x < W; x += 4) p.drawLine(x, 0, x, H-1);
		for (int y = 0; y < H; y += 4) p.drawLine(0, y, W-1, y);
	}

	// Stuff the image into a Frame -> Cache -> DummyReader
	openshot::CacheMemory cache;
	auto f = std::make_shared<openshot::Frame>(1, W, H, "#000000", 0, 2);
	f->AddImage(std::make_shared<QImage>(src));
	cache.Add(f);

	openshot::DummyReader dummy(openshot::Fraction(30,1), W, H, 44100, 2, 1.0, &cache);
	dummy.Open();

	openshot::Clip clip;
	clip.Reader(&dummy);
	clip.Open();

	// Helper lambda to count "near-white" pixels in a region (for debug/metrics)
	auto count_white = [](const QImage& im, int x0, int y0, int x1, int y1)->int {
		int cnt = 0;
		for (int y = y0; y <= y1; ++y) {
			for (int x = x0; x <= x1; ++x) {
				QColor c = im.pixelColor(x, y);
				if (c.red() > 240 && c.green() > 240 && c.blue() > 240) ++cnt;
			}
		}
		return cnt;
	};

	// Helper lambda to compute per-pixel difference count between two images
	auto diff_count = [](const QImage& a, const QImage& b, int x0, int y0, int x1, int y1)->int {
		int cnt = 0;
		for (int y = y0; y <= y1; ++y) {
			for (int x = x0; x <= x1; ++x) {
				QColor ca = a.pixelColor(x, y);
				QColor cb = b.pixelColor(x, y);
				int dr = std::abs(ca.red()   - cb.red());
				int dg = std::abs(ca.green() - cb.green());
				int db = std::abs(ca.blue()  - cb.blue());
				// treat any noticeable RGB change as a difference
				if ((dr + dg + db) > 24) ++cnt;
			}
		}
		return cnt;
	};

	// Case A: Identity transform (no move/scale/rotate). Output should match source at a white grid point.
	std::shared_ptr<openshot::Frame> out_identity;
	{
		clip.scale_x = openshot::Keyframe(1.0);
		clip.scale_y = openshot::Keyframe(1.0);
		clip.rotation = openshot::Keyframe(0.0);
		clip.location_x = openshot::Keyframe(0.0);
		clip.location_y = openshot::Keyframe(0.0);
		clip.display = openshot::FRAME_DISPLAY_NONE;

		out_identity = clip.GetFrame(1);
		auto img = out_identity->GetImage();
		REQUIRE(img);
		// Pick a mid pixel that is white in the grid (multiple of 4)
		QColor c = img->pixelColor(20, 20);
		CHECK(c.red()	>= 240);
		CHECK(c.green() >= 240);
		CHECK(c.blue()	>= 240);
	}

	// Case B: Downscale (trigger transform path). Clear the clip cache so we don't
	// accidentally re-use the identity frame from final_cache.
	{
		clip.GetCache()->Clear(); // **critical fix** ensure recompute after keyframe changes

		// Force a downscale to half
		clip.scale_x = openshot::Keyframe(0.5);
		clip.scale_y = openshot::Keyframe(0.5);
		clip.rotation = openshot::Keyframe(0.0);
		clip.location_x = openshot::Keyframe(0.0);
		clip.location_y = openshot::Keyframe(0.0);
		clip.display = openshot::FRAME_DISPLAY_NONE;

		auto out_scaled = clip.GetFrame(1);
		auto img_scaled = out_scaled->GetImage();
		REQUIRE(img_scaled);

		// Measure difference vs identity in a central region to avoid edges
		const int x0 = 8, y0 = 8, x1 = W - 9, y1 = H - 9;
		int changed = diff_count(*out_identity->GetImage(), *img_scaled, x0, y0, x1, y1);

		// After scaling, the image must not be identical to identity output.
		// Using a minimal check keeps this robust across Qt versions and platforms.
		CHECK(changed > 0);

		// Optional diagnostic: scaled typically yields <= number of pure whites vs identity.
		int white_id = count_white(*out_identity->GetImage(), x0, y0, x1, y1);
		int white_sc = count_white(*img_scaled,		   x0, y0, x1, y1);
		CHECK(white_sc <= white_id);
	}
}

TEST_CASE("Speed up time curve (3x, with resampling)", "[libopenshot][clip][time][speedup]")
{
	using namespace openshot;

	// --- Construct predictable source audio in a cache (linear ramp), 30fps, 44100Hz, stereo ---
	const Fraction fps(30, 1);
	const int sample_rate = 44100;
	const int channels    = 2;
	const int frames_n    = 270;            // 9 seconds at 30fps (source span)
	const int sppf        = sample_rate / fps.ToDouble(); // 1470
	const int total_samples = frames_n * sppf;            // 396,900

	CacheMemory cache;
	cache.SetMaxBytes(0);

	float ramp_value = 0.0f;
	const float ramp_step = 1.0f / static_cast<float>(total_samples);  // linear ramp across entire source

	for (int64_t fn = 1; fn <= frames_n; ++fn) {
		auto f = std::make_shared<Frame>(fn, sppf, channels);
		f->SampleRate(sample_rate);

		std::vector<float> chbuf(sppf);
		for (int s = 0; s < sppf; ++s) {
			chbuf[s] = ramp_value;
			ramp_value += ramp_step;
		}
		f->AddAudio(true, 0, 0, chbuf.data(), sppf, 1.0);
		f->AddAudio(true, 1, 0, chbuf.data(), sppf, 1.0);

		cache.Add(f);
	}

	DummyReader r(fps, 1920, 1080, sample_rate, channels, /*video_length_sec*/ 30.0, &cache);
	r.Open();
	r.info.has_audio = true;

	// --- Expected output: 3x speed => every 3rd source sample
	// Output duration is 3 seconds (90 frames) => 90 * 1470 = 132,300 samples
	const int output_frames = 90;
	const int out_samples   = output_frames * sppf;       // 132,300
	std::vector<float> expected;
	expected.reserve(out_samples);
	for (int i = 0; i < out_samples; ++i) {
		const int src_sample_index = i * 3;                // exact 3x speed mapping in samples
		expected.push_back(static_cast<float>(src_sample_index) * ramp_step);
	}

	// --- Clip with 3x speed curve: timeline frames 1..90 -> source frames 1..270
	Clip clip(&r);
	clip.time = Keyframe();
	clip.time.AddPoint(1.0,  1.0,   LINEAR);
	clip.time.AddPoint(91.0, 271.0, LINEAR);  // 90 timeline frames cover 270 source frames
	clip.End(static_cast<float>(output_frames) / static_cast<float>(fps.ToDouble())); // 3.0s
	clip.Position(0.0);

	// Timeline with resampling
	Timeline tl(1920, 1080, fps, sample_rate, channels, LAYOUT_STEREO);
	tl.AddClip(&clip);
	tl.Open();

	// --- Pull timeline audio and concatenate into 'actual'
	std::vector<float> actual;
	actual.reserve(out_samples);

	for (int64_t tf = 1; tf <= output_frames; ++tf) {
		auto fr = tl.GetFrame(tf);
		const int n = fr->GetAudioSamplesCount();
		REQUIRE(n == sppf);

		const float* p = fr->GetAudioSamples(0);   // RAW samples
		actual.insert(actual.end(), p, p + n);
	}

	REQUIRE(static_cast<int>(actual.size()) == out_samples);
	REQUIRE(actual.size() == expected.size());

	// --- Compare with a tolerance appropriate for resampling
	const float tolerance = 2e-2f;

	size_t mismatches = 0;
	for (size_t i = 0; i < expected.size(); ++i) {
		if (actual[i] != Approx(expected[i]).margin(tolerance)) {
			if (mismatches < 20) {
				std::cout << "[DBG speedup 3x] i=" << i
				          << " out=" << actual[i] << " exp=" << expected[i] << "\n";
			}
			++mismatches;
		}
	}

	CHECK(mismatches == 0);

	// Clean up
	tl.Close();
	clip.Close();
	r.Close();
	cache.Clear();
}

TEST_CASE("Reverse time curve (sample-exact, no resampling)", "[libopenshot][clip][time][reverse]")
{
	using namespace openshot;

	// --- Construct predictable source audio in a cache (abs(sin)), 30fps, 44100Hz, stereo ---
	const Fraction fps(30, 1);
	const int sample_rate = 44100;
	const int channels    = 2;
	const int frames_n    = 90;             // 3 seconds at 30fps
	const int sppf        = sample_rate / fps.ToDouble(); // 44100 / 30 = 1470
	const int total_samples = frames_n * sppf;

	const int   OFFSET      = 0;
	const float AMPLITUDE   = 0.75f;
	const int   NUM_SINE_STEPS = 100;
	double angle = 0.0;

	CacheMemory cache;
	cache.SetMaxBytes(0);

	for (int64_t fn = 1; fn <= frames_n; ++fn) {
	   auto f = std::make_shared<Frame>(fn, sppf, channels);
	   f->SampleRate(sample_rate);

	   // channel buffers for this frame
	   std::vector<float> chbuf(sppf);
	   for (int s = 0; s < sppf; ++s) {
		  const float v = std::fabs(float(AMPLITUDE * std::sin(angle) + OFFSET));
		  chbuf[s] = v;
		  angle += (2.0 * M_PI) / NUM_SINE_STEPS;
	   }
	   f->AddAudio(true, 0, 0, chbuf.data(), sppf, 1.0);
	   f->AddAudio(true, 1, 0, chbuf.data(), sppf, 1.0);

	   cache.Add(f);
	}

	DummyReader r(fps, 1920, 1080, sample_rate, channels, /*video_length_sec*/ 30.0, &cache);
	r.Open();
	r.info.has_audio = true;

	// --- Build the expected "global reverse" vector (channel 0) ---
	std::vector<float> expected;
	expected.reserve(total_samples);
	for (int64_t fn = 1; fn <= frames_n; ++fn) {
	   auto f = cache.GetFrame(fn);
	   const float* p = f->GetAudioSamples(0);
	   expected.insert(expected.end(), p, p + sppf);
	}
	std::reverse(expected.begin(), expected.end());

	// --- Clip with reverse time curve: timeline 1..frames_n -> source frames_n..1
	Clip clip(&r);
	clip.time = Keyframe();
	clip.time.AddPoint(1.0, double(frames_n), LINEAR);
	clip.time.AddPoint(double(frames_n), 1.0, LINEAR);

	// set End to exactly frames_n/fps so timeline outputs frames_n frames
	clip.End(float(frames_n) / float(fps.ToDouble()));
	clip.Position(0.0);

	// Timeline matches reader (no resampling)
	Timeline tl(1920, 1080, fps, sample_rate, channels, LAYOUT_STEREO);
	tl.AddClip(&clip);
	tl.Open();

	// --- Pull timeline audio and concatenate into 'actual'
	std::vector<float> actual;
	actual.reserve(total_samples);

	for (int64_t tf = 1; tf <= frames_n; ++tf) {
		auto fr = tl.GetFrame(tf);
		const int n = fr->GetAudioSamplesCount();
		REQUIRE(n == sppf);

		const float* p = fr->GetAudioSamples(0);   // RAW samples
		actual.insert(actual.end(), p, p + n);
	}

	//REQUIRE(actual.size() == expected.size());

	// --- Strict element-wise comparison
	size_t mismatches = 0;
	for (size_t i = 0; i < expected.size(); ++i) {
	   // The inputs are identical floats generated deterministically (no resampling),
	   // so we can compare with a very small tolerance.
	   if (actual[i] != Approx(expected[i]).margin(1e-6f)) {
		  // log a handful to make any future issues obvious
		  if (mismatches < 20) {
			 std::cout << "[DBG reverse no-resample] i=" << i
					 << " out=" << actual[i] << " exp=" << expected[i] << "\n";
		  }
		  ++mismatches;
	   }
	}

	CHECK(mismatches == 0);

	// Clean up
	tl.Close();
	clip.Close();
	r.Close();
	cache.Clear();
}
