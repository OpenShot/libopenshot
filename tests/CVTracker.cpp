/**
 * @file
 * @brief Unit tests for openshot::Frame
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
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
        "region": {"x": 294, "y": 102, "width": 180, "height": 166, "first-frame": 0}
    } )proto";

    // Create tracker
    CVTracker kcfTracker(json_data, tracker_pc);

    // Track clip for frames 0-20
    kcfTracker.trackClip(c1, 0, 20, true);
    // Get tracked data
    FrameData fd = kcfTracker.GetTrackedData(20);
    float x = fd.x1;
    float y = fd.y1;
    float width = fd.x2 - x;
    float height = fd.y2 - y;

    // Compare if tracked data is equal to pre-tested ones
    CHECK((int)(x * 640) == 259);
    CHECK((int)(y * 360) == 131);
    CHECK((int)(width * 640) == 180);
    CHECK((int)(height * 360) == 166);
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
        "region": {"x": 294, "y": 102, "width": 180, "height": 166, "first-frame": 0}
    } )proto";


    // Create first tracker
    CVTracker kcfTracker_1(json_data, tracker_pc);

    // Track clip for frames 0-20
    kcfTracker_1.trackClip(c1, 0, 20, true);

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
        "region": {"x": -1, "y": -1, "width": -1, "height": -1, "first-frame": 0}
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
    CHECK((int)(x_1 * 640) == (int)(x_2 * 640));
    CHECK((int)(y_1 * 360) == (int)(y_2 * 360));
    CHECK((int)(width_1 * 640) == (int)(width_2 * 640));
    CHECK((int)(height_1 * 360) == (int)(height_2 * 360));
}
