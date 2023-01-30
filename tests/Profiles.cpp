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


#include "Profiles.h"

TEST_CASE( "empty constructor", "[libopenshot][profile]" )
{
    openshot::Profile p1;

    // Default values
    CHECK(p1.info.width == 0);
    CHECK(p1.info.height == 0);
    CHECK(p1.info.fps.num == 0);
    CHECK(p1.info.fps.den == 0);
    CHECK(p1.info.display_ratio.num == 0);
    CHECK(p1.info.display_ratio.den == 0);
    CHECK(p1.info.pixel_ratio.num == 0);
    CHECK(p1.info.pixel_ratio.den == 0);
    CHECK(p1.info.interlaced_frame == false);

}

TEST_CASE( "constructor with example profiles", "[libopenshot][profile]" )
{
    std::stringstream profile1;
    profile1 << TEST_MEDIA_PATH << "example_profile1";

	openshot::Profile p1(profile1.str());

	// Default values
	CHECK(p1.info.width == 1280);
    CHECK(p1.info.height == 720);
    CHECK(p1.info.fps.num == 24);
    CHECK(p1.info.fps.den == 1);
    CHECK(p1.info.display_ratio.num == 16);
    CHECK(p1.info.display_ratio.den == 9);
    CHECK(p1.info.pixel_ratio.num == 1);
    CHECK(p1.info.pixel_ratio.den == 1);
    CHECK(p1.info.interlaced_frame == false);

    std::stringstream profile2;
    profile2 << TEST_MEDIA_PATH << "example_profile2";

    openshot::Profile p2(profile2.str());

    // Default values
    CHECK(p2.info.width == 1920);
    CHECK(p2.info.height == 1080);
    CHECK(p2.info.fps.num == 30000);
    CHECK(p2.info.fps.den == 1001);
    CHECK(p2.info.display_ratio.num == 16);
    CHECK(p2.info.display_ratio.den == 9);
    CHECK(p2.info.pixel_ratio.num == 1);
    CHECK(p2.info.pixel_ratio.den == 1);
    CHECK(p2.info.interlaced_frame == true);
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
