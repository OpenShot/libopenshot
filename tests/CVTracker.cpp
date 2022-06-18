/**
 * @file
 * @brief Unit tests for CVTracker
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2020 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <sstream>
#include <memory>

#include "openshot_catch.h"

#include "Clip.h"
#include "CVTracker.h"  // for FrameData, CVTracker
#include "ProcessingController.h"
#include "Exceptions.h"

using namespace openshot;

TEST_CASE( "initialization", "[libopenshot][opencv][tracker]" )
{
    std::string bad_json = R"proto(
    }
        [1, 2, 3, "a"]
    } )proto";
    ProcessingController badPC;
    CVTracker* badTracker;
    CHECK_THROWS_AS(
        badTracker = new CVTracker(bad_json, badPC),
        openshot::InvalidJSON
    );

    std::string json1 = R"proto(
    {
        "tracker-type": "KCF"
    } )proto";

    ProcessingController pc1;
    CVTracker tracker1(json1, pc1);
    CHECK(pc1.GetError() == true);
    CHECK(pc1.GetErrorMessage() == "No initial bounding box selected");

    std::string json2 = R"proto(
    {
        "tracker-type": "KCF",
        "region": {
            "normalized_x": 0.459375,
            "normalized_y": 0.28333,
            "normalized_width": -0.28125,
            "normalized_height": -0.461111
        }
    } )proto";

    // Create tracker
    ProcessingController pc2;
    CVTracker tracker2(json2, pc2);
    CHECK(pc2.GetError() == true);
    CHECK(pc2.GetErrorMessage() == "No first-frame");
}

TEST_CASE( "Track_Video", "[libopenshot][opencv][tracker]" )
{
    // Create a video clip
    std::stringstream path;
    path << TEST_MEDIA_PATH << "test.avi";

    // Open clip
    openshot::Clip c1(path.str());
    c1.Open();

    std::string json_data = R"proto(
    {
        "protobuf_data_path": "kcf_tracker.data",
        "tracker-type": "KCF",
        "region": {
            "normalized_x": 0.459375,
            "normalized_y": 0.28333,
            "normalized_width": 0.28125,
            "normalized_height": 0.461111,
            "first-frame": 1
        }
    } )proto";

    // Create tracker
    ProcessingController tracker_pc;
    CVTracker kcfTracker(json_data, tracker_pc);

    // Track clip for frames 0-20
    kcfTracker.trackClip(c1, 1, 20, true);
    // Get tracked data
    FrameData fd = kcfTracker.GetTrackedData(20);
    int x = (float)fd.x1 * 640;
    int y = (float)fd.y1 * 360;
    int width = ((float)fd.x2*640) - x;
    int height = ((float)fd.y2*360) - y;

    // Compare if tracked data is equal to pre-tested ones
    CHECK(x == Approx(256).margin(1));
    CHECK(y == Approx(132).margin(1));
    CHECK(width == Approx(180).margin(1));
    CHECK(height == Approx(166).margin(2));
}


TEST_CASE( "SaveLoad_Protobuf", "[libopenshot][opencv][tracker]" )
{

    // Create a video clip
    std::stringstream path;
    path << TEST_MEDIA_PATH << "test.avi";

    // Open clip
    openshot::Clip c1(path.str());
    c1.Open();

    std::string json_data = R"proto(
    {
        "protobuf_data_path": "kcf_tracker.data",
        "tracker-type": "KCF",
        "region": {
            "normalized_x": 0.46,
            "normalized_y": 0.28,
            "normalized_width": 0.28,
            "normalized_height": 0.46,
            "first-frame": 1
        }
    } )proto";

    // Create first tracker
    ProcessingController tracker_pc;
    CVTracker kcfTracker_1(json_data, tracker_pc);

    // Track clip for frames 0-20
    kcfTracker_1.trackClip(c1, 1, 20, true);

    // Get tracked data
    FrameData fd_1 = kcfTracker_1.GetTrackedData(20);

    float x_1 = fd_1.x1;
    float y_1 = fd_1.y1;
    float width_1 = fd_1.x2 - x_1;
    float height_1 = fd_1.y2 - y_1;

    // Save tracked data
    kcfTracker_1.SaveTrackedData();

    std::string proto_data_1 = R"proto(
    {
        "protobuf_data_path": "kcf_tracker.data",
        "tracker_type": "",
        "region": {
            "normalized_x": 0.1,
            "normalized_y": 0.1,
            "normalized_width": -0.5,
            "normalized_height": -0.5,
            "first-frame": 1
        }
    } )proto";

    // Create second tracker
    CVTracker kcfTracker_2(proto_data_1, tracker_pc);

    // Load tracked data from first tracker protobuf data
    kcfTracker_2._LoadTrackedData();

    // Get tracked data
    FrameData fd_2 = kcfTracker_2.GetTrackedData(20);

    float x_2 = fd_2.x1;
    float y_2 = fd_2.y1;
    float width_2 = fd_2.x2 - x_2;
    float height_2 = fd_2.y2 - y_2;

    // Compare first tracker data with second tracker data
    CHECK(x_1 == Approx(x_2).margin(0.01));
    CHECK(y_1 == Approx(y_2).margin(0.01));
    CHECK(width_1 == Approx(width_2).margin(0.01));
    CHECK(height_1 == Approx(height_2).margin(0.01));

}
