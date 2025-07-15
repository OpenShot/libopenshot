/**
 * @file
 * @brief Unit tests for openshot::Profile
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2023 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"
#include <sstream>


#include "Profiles.h"

TEST_CASE( "empty constructor", "[libopenshot][profile]" )
{
    openshot::Profile p1;

    // Default values
    CHECK(p1.info.description.empty());
    CHECK(p1.info.width == 0);
    CHECK(p1.info.height == 0);
    CHECK(p1.info.fps.num == 0);
    CHECK(p1.info.fps.den == 0);
    CHECK(p1.info.display_ratio.num == 0);
    CHECK(p1.info.display_ratio.den == 0);
    CHECK(p1.info.pixel_ratio.num == 0);
    CHECK(p1.info.pixel_ratio.den == 0);
    CHECK(p1.info.interlaced_frame == false);
    CHECK(p1.info.spherical == false);

}

TEST_CASE( "constructor with example profiles", "[libopenshot][profile]" )
{
    std::stringstream profile1;
    profile1 << TEST_MEDIA_PATH << "example_profile1";

	openshot::Profile p1(profile1.str());

	// Default values
    CHECK(p1.info.description == "HD 720p 24 fps");
	CHECK(p1.info.width == 1280);
    CHECK(p1.info.height == 720);
    CHECK(p1.info.fps.num == 24);
    CHECK(p1.info.fps.den == 1);
    CHECK(p1.info.display_ratio.num == 16);
    CHECK(p1.info.display_ratio.den == 9);
    CHECK(p1.info.pixel_ratio.num == 1);
    CHECK(p1.info.pixel_ratio.den == 1);
    CHECK(p1.info.interlaced_frame == false);
    CHECK(p1.info.spherical == false);

    // Export to JSON
    openshot::Profile p1_json = openshot::Profile();
    p1_json.SetJson(p1.Json());

    CHECK(p1_json.info.description == "HD 720p 24 fps");
    CHECK(p1_json.info.width == 1280);
    CHECK(p1_json.info.height == 720);
    CHECK(p1_json.info.fps.num == 24);
    CHECK(p1_json.info.fps.den == 1);
    CHECK(p1_json.info.display_ratio.num == 16);
    CHECK(p1_json.info.display_ratio.den == 9);
    CHECK(p1_json.info.pixel_ratio.num == 1);
    CHECK(p1_json.info.pixel_ratio.den == 1);
    CHECK(p1_json.info.interlaced_frame == false);
    CHECK(p1_json.info.spherical == false);

    std::stringstream profile2;
    profile2 << TEST_MEDIA_PATH << "example_profile2";

    openshot::Profile p2(profile2.str());

    // Default values
    CHECK(p2.info.description == "HD 1080i 29.97 fps");
    CHECK(p2.info.width == 1920);
    CHECK(p2.info.height == 1080);
    CHECK(p2.info.fps.num == 30000);
    CHECK(p2.info.fps.den == 1001);
    CHECK(p2.info.display_ratio.num == 16);
    CHECK(p2.info.display_ratio.den == 9);
    CHECK(p2.info.pixel_ratio.num == 1);
    CHECK(p2.info.pixel_ratio.den == 1);
    CHECK(p2.info.interlaced_frame == true);
    CHECK(p2.info.spherical == false);
}

TEST_CASE( "24 fps names", "[libopenshot][profile]" )
{
    std::stringstream path;
    path << TEST_MEDIA_PATH << "example_profile1";

    openshot::Profile p(path.str());

    // Default values
    CHECK(p.Key() == "01280x0720p0024_16-09");
    CHECK(p.ShortName() == "1280x720p24");
    CHECK(p.LongName() == "1280x720p @ 24 fps (16:9)");
    CHECK(p.LongNameWithDesc() == "1280x720p @ 24 fps (16:9) HD 720p 24 fps");
}

TEST_CASE( "29.97 fps names", "[libopenshot][profile]" )
{
    std::stringstream path;
    path << TEST_MEDIA_PATH << "example_profile2";

    openshot::Profile p(path.str());

    // Default values
    CHECK(p.Key() == "01920x1080i2997_16-09");
    CHECK(p.ShortName() == "1920x1080i29.97");
    CHECK(p.LongName() == "1920x1080i @ 29.97 fps (16:9)");
    CHECK(p.LongNameWithDesc() == "1920x1080i @ 29.97 fps (16:9) HD 1080i 29.97 fps");
}

TEST_CASE( "compare profiles", "[libopenshot][profile]" )
{
    // 720p24
    std::stringstream profile1;
    profile1 << TEST_MEDIA_PATH << "example_profile1";
    openshot::Profile p1(profile1.str());

    // 720p24 (copy)
    openshot::Profile p1copy(profile1.str());

    // 1080i2997
    std::stringstream profile2;
    profile2 << TEST_MEDIA_PATH << "example_profile2";
    openshot::Profile p2(profile2.str());

    // 1080i2997 (copy)
    openshot::Profile p2copy(profile2.str());

    CHECK(p1 < p2);
    CHECK(p2 > p1);
    CHECK(p1 == p1copy);
    CHECK(p2 == p2copy);

    // 720p60
    openshot::Profile p3(profile1.str());
    p3.info.fps.num = 60;

    CHECK(p1 < p3);
    CHECK_FALSE(p1 == p3);

    // 72024, DAR: 4:3
    p3.info.fps.num = 24;
    p3.info.display_ratio.num = 4;
    p3.info.display_ratio.den = 3;

    CHECK(p1 > p3);
    CHECK(p3 < p1);
    CHECK_FALSE(p1 == p3);
}

TEST_CASE( "save profiles", "[libopenshot][profile]" )
{
    // Load profile
    std::stringstream profile1;
    profile1 << TEST_MEDIA_PATH << "example_profile1";
    openshot::Profile p1(profile1.str());

    // Save copy
    std::stringstream profile1_copy;
    profile1_copy << TEST_MEDIA_PATH << "example_profile1_copy";
    p1.Save(profile1_copy.str());

    // Load saved copy
    openshot::Profile p1_load_copy(profile1_copy.str());

    // Default values
    CHECK(p1_load_copy.info.description == "HD 720p 24 fps");
    CHECK(p1_load_copy.info.width == 1280);
    CHECK(p1_load_copy.info.height == 720);
    CHECK(p1_load_copy.info.fps.num == 24);
    CHECK(p1_load_copy.info.fps.den == 1);
    CHECK(p1_load_copy.info.display_ratio.num == 16);
    CHECK(p1_load_copy.info.display_ratio.den == 9);
    CHECK(p1_load_copy.info.pixel_ratio.num == 1);
    CHECK(p1_load_copy.info.pixel_ratio.den == 1);
    CHECK(p1_load_copy.info.interlaced_frame == false);
    CHECK(p1_load_copy.info.spherical == false);
}

TEST_CASE( "spherical profiles", "[libopenshot][profile]" )
{
    // Create a new profile with spherical=true
    openshot::Profile p;
    p.info.description = "360° Test Profile";
    p.info.width = 3840;
    p.info.height = 1920;
    p.info.fps.num = 30;
    p.info.fps.den = 1;
    p.info.display_ratio.num = 2;
    p.info.display_ratio.den = 1;
    p.info.pixel_ratio.num = 1;
    p.info.pixel_ratio.den = 1;
    p.info.interlaced_frame = false;
    p.info.spherical = true;

    // Test the name methods for spherical content
    CHECK(p.Key() == "03840x1920p0030_02-01_360");
    CHECK(p.ShortName() == "3840x1920p30 360°");
    CHECK(p.LongName() == "3840x1920p @ 30 fps (2:1) 360°");
    CHECK(p.LongNameWithDesc() == "3840x1920p @ 30 fps (2:1) 360° 360° Test Profile");

    // Test JSON serialization and deserialization
    std::string json = p.Json();
    openshot::Profile p_json;
    p_json.SetJson(json);

    CHECK(p_json.info.spherical == true);
    CHECK(p_json.ShortName() == "3840x1920p30 360°");

    // Save and reload to test file I/O
    std::stringstream profile_path;
    profile_path << TEST_MEDIA_PATH << "example_profile_360";
    p.Save(profile_path.str());

    // Load the saved profile
    openshot::Profile p_loaded(profile_path.str());
    CHECK(p_loaded.info.spherical == true);
    CHECK(p_loaded.ShortName() == "3840x1920p30 360°");

    // Test comparison operators
    openshot::Profile p_non_spherical = p;
    p_non_spherical.info.spherical = false;

    CHECK_FALSE(p == p_non_spherical);
}