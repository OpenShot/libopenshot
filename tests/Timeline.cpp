/**
 * @file
 * @brief Unit tests for openshot::Timeline
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <string>
#include <sstream>
#include <memory>
#include <list>

#include <catch2/catch.hpp>

#include "Timeline.h"
#include "Clip.h"
#include "Frame.h"
#include "Fraction.h"
#include "effects/Blur.h"
#include "effects/Negate.h"

using namespace openshot;

TEST_CASE( "constructor", "[libopenshot][timeline]" )
{
	Fraction fps(30000,1000);
	Timeline t1(640, 480, fps, 44100, 2, LAYOUT_STEREO);

	// Check values
	CHECK(t1.info.width == 640);
	CHECK(t1.info.height == 480);

	Timeline t2(300, 240, fps, 44100, 2, LAYOUT_STEREO);

	// Check values
	CHECK(t2.info.width == 300);
	CHECK(t2.info.height == 240);
}

TEST_CASE("ReaderInfo constructor", "[libopenshot][timeline]")
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "test.mp4";
	Clip clip_video(path.str());
	clip_video.Open();
	const auto r1 = clip_video.Reader();

	// Configure a Timeline with the same parameters
	Timeline t1(r1->info);

	CHECK(r1->info.width == t1.info.width);
	CHECK(r1->info.height == t1.info.height);
	CHECK(r1->info.fps.num == t1.info.fps.num);
	CHECK(r1->info.fps.den == t1.info.fps.den);
	CHECK(r1->info.sample_rate == t1.info.sample_rate);
	CHECK(r1->info.channels == t1.info.channels);
	CHECK(r1->info.channel_layout == t1.info.channel_layout);
}

TEST_CASE( "width and height functions", "[libopenshot][timeline]" )
{
	Fraction fps(30000,1000);
	Timeline t1(640, 480, fps, 44100, 2, LAYOUT_STEREO);

	// Check values
	CHECK(t1.info.width == 640);
	CHECK(t1.info.height == 480);

	// Set width
	t1.info.width = 600;

	// Check values
	CHECK(t1.info.width == 600);
	CHECK(t1.info.height == 480);

	// Set height
	t1.info.height = 400;

	// Check values
	CHECK(t1.info.width == 600);
	CHECK(t1.info.height == 400);
}

TEST_CASE( "Framerate", "[libopenshot][timeline]" )
{
	Fraction fps(24,1);
	Timeline t1(640, 480, fps, 44100, 2, LAYOUT_STEREO);

	// Check values
	CHECK(t1.info.fps.ToFloat() == Approx(24.0f).margin(0.00001));
}

TEST_CASE( "two-track video", "[libopenshot][timeline]" )
{
	// Create a reader
	std::stringstream path;
	path << TEST_MEDIA_PATH << "test.mp4";
	Clip clip_video(path.str());
	clip_video.Layer(0);
	clip_video.Position(0.0);

	std::stringstream path_overlay;
	path_overlay << TEST_MEDIA_PATH << "front3.png";
	Clip clip_overlay(path_overlay.str());
	clip_overlay.Layer(1);
	clip_overlay.Position(0.05); // Delay the overlay by 0.05 seconds
	clip_overlay.End(0.5);	// Make the duration of the overlay 1/2 second

	// Create a timeline
	Timeline t(1280, 720, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	// Add clips
	t.AddClip(&clip_video);
	t.AddClip(&clip_overlay);

	t.Open();

	std::shared_ptr<Frame> f = t.GetFrame(1);

	// Get the image data
	int pixel_row = 200;
	int pixel_index = 230 * 4; // pixel 230 (4 bytes per pixel)

	// Check image properties
	CHECK((int)f->GetPixels(pixel_row)[pixel_index] == Approx(21).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 1] == Approx(191).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 2] == Approx(0).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 3] == Approx(255).margin(5));

	f = t.GetFrame(2);

	// Check image properties
	CHECK((int)f->GetPixels(pixel_row)[pixel_index] == Approx(176).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 1] == Approx(0).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 2] == Approx(186).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 3] == Approx(255).margin(5));

	f = t.GetFrame(3);

	// Check image properties
	CHECK((int)f->GetPixels(pixel_row)[pixel_index] == Approx(23).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 1] == Approx(190).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 2] == Approx(0).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 3] == Approx(255).margin(5));

	f = t.GetFrame(24);

	// Check image properties
	CHECK((int)f->GetPixels(pixel_row)[pixel_index] == Approx(186).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 1] == Approx(106).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 2] == Approx(0).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 3] == Approx(255).margin(5));

	f = t.GetFrame(5);

	// Check image properties
	CHECK((int)f->GetPixels(pixel_row)[pixel_index] == Approx(23).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 1] == Approx(190).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 2] == Approx(0).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 3] == Approx(255).margin(5));

	f = t.GetFrame(25);

	// Check image properties
	CHECK((int)f->GetPixels(pixel_row)[pixel_index] == Approx(0).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 1] == Approx(94).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 2] == Approx(186).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 3] == Approx(255).margin(5));

	f = t.GetFrame(4);

	// Check image properties
	CHECK((int)f->GetPixels(pixel_row)[pixel_index] == Approx(176).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 1] == Approx(0).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 2] == Approx(186).margin(5));
	CHECK((int)f->GetPixels(pixel_row)[pixel_index + 3] == Approx(255).margin(5));

	t.Close();
}

TEST_CASE( "Clip order", "[libopenshot][timeline]" )
{
	// Create a timeline
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	// Add some clips out of order
	std::stringstream path_top;
	path_top << TEST_MEDIA_PATH << "front3.png";
	Clip clip_top(path_top.str());
	clip_top.Layer(2);
	t.AddClip(&clip_top);

	std::stringstream path_middle;
	path_middle << TEST_MEDIA_PATH << "front.png";
	Clip clip_middle(path_middle.str());
	clip_middle.Layer(0);
	t.AddClip(&clip_middle);

	std::stringstream path_bottom;
	path_bottom << TEST_MEDIA_PATH << "back.png";
	Clip clip_bottom(path_bottom.str());
	clip_bottom.Layer(1);
	t.AddClip(&clip_bottom);

	t.Open();

	// Loop through Clips and check order (they should have been sorted into the correct order)
	// Bottom layer to top layer, then by position.
	std::list<Clip*> clips = t.Clips();
	int n = 0;
	for (auto clip : clips) {
		CHECK(clip->Layer() == n);
		++n;
	}

	// Add another clip
	std::stringstream path_middle1;
	path_middle1 << TEST_MEDIA_PATH << "interlaced.png";
	Clip clip_middle1(path_middle1.str());
	clip_middle1.Layer(1);
	clip_middle1.Position(0.5);
	t.AddClip(&clip_middle1);

	// Loop through clips again, and re-check order
	clips = t.Clips();
	n = 0;
	for (auto clip : clips) {
		switch (n) {
		case 0:
			CHECK(clip->Layer() == 0);
			break;
		case 1:
			CHECK(clip->Layer() == 1);
			CHECK(clip->Position() == Approx(0.0).margin(0.0001));
			break;
		case 2:
			CHECK(clip->Layer() == 1);
			CHECK(clip->Position() == Approx(0.5).margin(0.0001));
			break;
		case 3:
			CHECK(clip->Layer() == 2);
			break;
		}
		++n;
	}

	t.Close();
}

TEST_CASE( "TimelineBase", "[libopenshot][timeline]" )
{
    // Create a timeline
    Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

    // Add some clips out of order
    std::stringstream path;
    path << TEST_MEDIA_PATH << "front3.png";
    Clip clip1(path.str());
    clip1.Layer(1);
    t.AddClip(&clip1);

    Clip clip2(path.str());
    clip2.Layer(0);
    t.AddClip(&clip2);

    // Verify that the list of clips can be accessed
    // through the Clips() method of a TimelineBase*
    TimelineBase* base = &t;
    auto l = base->Clips();
    CHECK(l.size() == 2);
    auto find1 = std::find(l.begin(), l.end(), &clip1);
    auto find2 = std::find(l.begin(), l.end(), &clip2);
    CHECK(find1 != l.end());
    CHECK(find2 != l.end());
}


TEST_CASE( "Effect order", "[libopenshot][timeline]" )
{
	// Create a timeline
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	// Add some effects out of order
	Negate effect_top;
	effect_top.Id("C");
	effect_top.Layer(2);
	t.AddEffect(&effect_top);

	Negate effect_middle;
	effect_middle.Id("A");
	effect_middle.Layer(0);
	t.AddEffect(&effect_middle);

	Negate effect_bottom;
	effect_bottom.Id("B");
	effect_bottom.Layer(1);
	t.AddEffect(&effect_bottom);

	t.Open();

	// Loop through effects and check order (they should have been sorted into the correct order)
	// Bottom layer to top layer, then by position, and then by order.
	std::list<EffectBase*> effects = t.Effects();
	int n = 0;
	for (auto effect : effects) {
		CHECK(effect->Layer() == n);
		CHECK(effect->Order() == 0);
		switch (n) {
		case 0:
			CHECK(effect->Id() == "A");
			break;
		case 1:
			CHECK(effect->Id() == "B");
			break;
		case 2:
			CHECK(effect->Id() == "C");
			break;
		}
		++n;
	}

	// Add some more effects out of order
	Negate effect_top1;
	effect_top1.Id("B-2");
	effect_top1.Layer(1);
	effect_top1.Position(0.5);
	effect_top1.Order(2);
	t.AddEffect(&effect_top1);

	Negate effect_middle1;
	effect_middle1.Id("B-3");
	effect_middle1.Layer(1);
	effect_middle1.Position(0.5);
	effect_middle1.Order(1);
	t.AddEffect(&effect_middle1);

	Negate effect_bottom1;
	effect_bottom1.Id("B-1");
	effect_bottom1.Layer(1);
	effect_bottom1.Position(0);
	effect_bottom1.Order(3);
	t.AddEffect(&effect_bottom1);


	// Loop through effects again, and re-check order
	effects = t.Effects();
	n = 0;
	for (auto effect : effects) {
		switch (n) {
		case 0:
			CHECK(effect->Layer() == 0);
			CHECK(effect->Id() == "A");
			CHECK(effect->Order() == 0);
			break;
		case 1:
			CHECK(effect->Layer() == 1);
			CHECK(effect->Id() == "B-1");
			CHECK(effect->Position() == Approx(0.0).margin(0.0001));
			CHECK(effect->Order() == 3);
			break;
		case 2:
			CHECK(effect->Layer() == 1);
			CHECK(effect->Id() == "B");
			CHECK(effect->Position() == Approx(0.0).margin(0.0001));
			CHECK(effect->Order() == 0);
			break;
		case 3:
			CHECK(effect->Layer() == 1);
			CHECK(effect->Id() == "B-2");
			CHECK(effect->Position() == Approx(0.5).margin(0.0001));
			CHECK(effect->Order() == 2);
			break;
		case 4:
			CHECK(effect->Layer() == 1);
			CHECK(effect->Id() == "B-3");
			CHECK(effect->Position() == Approx(0.5).margin(0.0001));
			CHECK(effect->Order() == 1);
			break;
		case 5:
			CHECK(effect->Layer() == 2);
			CHECK(effect->Id() == "C");
			CHECK(effect->Order() == 0);
			break;
		}
		++n;
	}

	t.Close();
}

TEST_CASE( "GetClip by id", "[libopenshot][timeline]" )
{
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	std::stringstream path1;
	path1 << TEST_MEDIA_PATH << "interlaced.png";
	auto media_path1 = path1.str();

	std::stringstream path2;
	path2 << TEST_MEDIA_PATH << "front.png";
	auto media_path2 = path2.str();

	Clip clip1(media_path1);
	std::string clip1_id("CLIP00001");
	clip1.Id(clip1_id);
	clip1.Layer(1);

	Clip clip2(media_path2);
	std::string clip2_id("CLIP00002");
	clip2.Id(clip2_id);
	clip2.Layer(2);
	clip2.Waveform(true);

	t.AddClip(&clip1);
	t.AddClip(&clip2);

	// We explicitly want to get returned a Clip*, here
	Clip* matched = t.GetClip(clip1_id);
	CHECK(matched->Id() == clip1_id);
	CHECK(matched->Layer() == 1);

	Clip* matched2 = t.GetClip(clip2_id);
	CHECK(matched2->Id() == clip2_id);
	CHECK_FALSE(matched2->Layer() < 2);

	Clip* matched3 = t.GetClip("BAD_ID");
	CHECK(matched3 == nullptr);

	// Ensure we can access the Clip API interfaces after lookup
	CHECK_FALSE(matched->Waveform());
	CHECK(matched2->Waveform() == true);
}

TEST_CASE( "GetClipEffect by id", "[libopenshot][timeline]" )
{
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	std::stringstream path1;
	path1 << TEST_MEDIA_PATH << "interlaced.png";
	auto media_path1 = path1.str();

	// Create a clip, nothing special
	Clip clip1(media_path1);
	std::string clip1_id("CLIP00001");
	clip1.Id(clip1_id);
	clip1.Layer(1);

	// Add a blur effect
	Keyframe horizontal_radius(5.0);
	Keyframe vertical_radius(5.0);
	Keyframe sigma(3.0);
	Keyframe iterations(3.0);
	Blur blur1(horizontal_radius, vertical_radius, sigma, iterations);
	std::string blur1_id("EFFECT00011");
	blur1.Id(blur1_id);
	clip1.AddEffect(&blur1);

	// A second clip, different layer
	Clip clip2(media_path1);
	std::string clip2_id("CLIP00002");
	clip2.Id(clip2_id);
	clip2.Layer(2);

	// Some effects for clip2
	Negate neg2;
	std::string neg2_id("EFFECT00021");
	neg2.Id(neg2_id);
	neg2.Layer(2);
	clip2.AddEffect(&neg2);
	Blur blur2(horizontal_radius, vertical_radius, sigma, iterations);
	std::string blur2_id("EFFECT00022");
	blur2.Id(blur2_id);
	blur2.Layer(2);
	clip2.AddEffect(&blur2);

	t.AddClip(&clip1);

	// Check that we can look up clip1's effect
	auto match1 = t.GetClipEffect("EFFECT00011");
	CHECK(match1->Id() == blur1_id);

	// clip2 hasn't been added yet, shouldn't be found
	match1 = t.GetClipEffect(blur2_id);
	CHECK(match1 == nullptr);

	t.AddClip(&clip2);

	// Check that blur2 can now be found via clip2
	match1 = t.GetClipEffect(blur2_id);
	CHECK(match1->Id() == blur2_id);
	CHECK(match1->Layer() == 2);
}

TEST_CASE( "GetEffect by id", "[libopenshot][timeline]" )
{
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	// Create a timeline effect
	Keyframe horizontal_radius(5.0);
	Keyframe vertical_radius(5.0);
	Keyframe sigma(3.0);
	Keyframe iterations(3.0);
	Blur blur1(horizontal_radius, vertical_radius, sigma, iterations);
	std::string blur1_id("EFFECT00011");
	blur1.Id(blur1_id);
	blur1.Layer(1);
	t.AddEffect(&blur1);

	auto match1 = t.GetEffect(blur1_id);
	CHECK(match1->Id() == blur1_id);
	CHECK(match1->Layer() == 1);

	match1 = t.GetEffect("NOSUCHNAME");
	CHECK(match1 == nullptr);
}

TEST_CASE( "Effect: Blur", "[libopenshot][timeline]" )
{
	// Create a timeline
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	std::stringstream path_top;
	path_top << TEST_MEDIA_PATH << "interlaced.png";
	Clip clip_top(path_top.str());
	clip_top.Layer(2);
	t.AddClip(&clip_top);

	// Add some effects out of order
	Keyframe horizontal_radius(5.0);
	Keyframe vertical_radius(5.0);
	Keyframe sigma(3.0);
	Keyframe iterations(3.0);
	Blur blur(horizontal_radius, vertical_radius, sigma, iterations);
	blur.Id("B");
	blur.Layer(2);
	t.AddEffect(&blur);

	// Open Timeline
	t.Open();

	// Get frame
	std::shared_ptr<Frame> f = t.GetFrame(1);

	REQUIRE(f != nullptr);
	CHECK(f->number == 1);

	// Close reader
	t.Close();
}

TEST_CASE( "GetMaxFrame and GetMaxTime", "[libopenshot][timeline]" )
{
	// Create a timeline
	Timeline t(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);

	std::stringstream path1;
	path1 << TEST_MEDIA_PATH << "interlaced.png";
	Clip clip1(path1.str());
	clip1.Layer(1);
	clip1.Position(50);
	clip1.End(45);
	t.AddClip(&clip1);

	CHECK(t.GetMaxTime() == Approx(95.0).margin(0.001));
	CHECK(t.GetMaxFrame() == 95 * 30 + 1);

	Clip clip2(path1.str());
	clip2.Layer(2);
	clip2.Position(0);
	clip2.End(55);
	t.AddClip(&clip2);

	CHECK(t.GetMaxFrame() == 95 * 30 + 1);
	CHECK(t.GetMaxTime() == Approx(95.0).margin(0.001));

	clip2.Position(100);
	clip1.Position(80);
	CHECK(t.GetMaxFrame() == 155 * 30 + 1);
	CHECK(t.GetMaxTime() == Approx(155.0).margin(0.001));
	t.RemoveClip(&clip2);
	CHECK(t.GetMaxFrame() == 125 * 30 + 1);
	CHECK(t.GetMaxTime() == Approx(125.0).margin(0.001));
}
