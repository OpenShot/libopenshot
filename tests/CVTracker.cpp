/**
 * @file
 * @brief Unit tests for CVTracker
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2020 OpenShot Studios, LLC
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

#include <sstream>
#include <memory>

#include <catch2/catch.hpp>

#include "Clip.h"
#include "CVTracker.h"  // for FrameData, CVTracker
#include "ProcessingController.h"

using namespace openshot;

// Just for the tracker constructor, it won't be used
ProcessingController tracker_pc;

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
        "region": {"normalized_x": 0.459375, "normalized_y": 0.28333, "normalized_width": 0.28125, "normalized_height": 0.461111, "first-frame": 1}
    } )proto";

    // Create tracker
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
    CHECK(y == Approx(134).margin(1));
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
            "normalized_width": 0.5,
            "normalized_height": 0.5,
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
