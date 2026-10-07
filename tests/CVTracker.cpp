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
#include <cmath>

#include "openshot_catch.h"

#include "Clip.h"
#include "CVTracker.h"  // for FrameData, CVTracker
#include "ProcessingController.h"
#include "Exceptions.h"
#include "sort_filter/sort.hpp"

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

TEST_CASE( "Track_BoundingBoxClipping", "[libopenshot][opencv][tracker]" )
{
    // Create a video clip
    std::stringstream path;
    path << TEST_MEDIA_PATH << "test.avi";

    // Open clip
    openshot::Clip c1(path.str());
    c1.Open();

    std::string json_data = R"proto(
    {
        "tracker-type": "KCF",
        "region": {
            "normalized_x": -0.2,
            "normalized_y": -0.2,
        "normalized_width": 1.5,
        "normalized_height": 1.5,
        "first-frame": 1
        }
    } )proto";

    ProcessingController tracker_pc;
    CVTracker tracker(json_data, tracker_pc);
    tracker_pc.SetError(false, "");

    // Grab first frame and run tracker directly
    std::shared_ptr<openshot::Frame> f = c1.GetFrame(1);
    cv::Mat image = f->GetImageCV();

    tracker.initTracker(image, 1);
    tracker.trackFrame(image, 2);

    INFO(tracker_pc.GetErrorMessage());
    CHECK(tracker_pc.GetError() == false);
}

TEST_CASE( "Track_FrameSizeChangeDoesNotCrash", "[libopenshot][opencv][tracker]" )
{
    std::string json_data = R"proto(
    {
        "tracker-type": "KCF",
        "region": {
            "normalized_x": 0.2,
            "normalized_y": 0.2,
            "normalized_width": 0.3,
            "normalized_height": 0.3,
            "first-frame": 1
        }
    } )proto";

    ProcessingController tracker_pc;
    CVTracker tracker(json_data, tracker_pc);

    cv::Mat frame1(360, 640, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::rectangle(frame1, cv::Rect(128, 72, 160, 108), cv::Scalar(255, 255, 255), cv::FILLED);

    cv::Mat frame2 = frame1.clone();

    cv::Mat frame3(180, 320, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::rectangle(frame3, cv::Rect(64, 36, 80, 54), cv::Scalar(255, 255, 255), cv::FILLED);

    REQUIRE_NOTHROW(tracker.initTracker(frame1, 1));
    REQUIRE_NOTHROW(tracker.trackFrame(frame2, 2));
    REQUIRE_NOTHROW(tracker.trackFrame(frame3, 3));

    FrameData fd = tracker.GetTrackedData(3);
    CHECK(fd.frame_id == 3);
    CHECK(fd.x1 >= 0.0f);
    CHECK(fd.y1 >= 0.0f);
    CHECK(fd.x2 <= 1.0f);
    CHECK(fd.y2 <= 1.0f);
}

TEST_CASE( "KalmanTracker smooths class scores", "[libopenshot][opencv][tracker]" )
{
    KalmanTracker tracker(
        cv::Rect_<float>(0.0f, 0.0f, 10.0f, 10.0f),
        0.9f, 1, 42,
        { ClassScore(1, 0.9f), ClassScore(2, 0.1f) }
    );

    CHECK(tracker.classId == 1);
    CHECK(tracker.confidence == Approx(0.9f));

    tracker.update_class_scores({ ClassScore(1, 0.1f), ClassScore(2, 0.9f) }, 2, 0.9f);
    CHECK(tracker.classId == 1);

    tracker.update_class_scores({ ClassScore(1, 0.1f), ClassScore(2, 0.9f) }, 2, 0.9f);
    CHECK(tracker.classId == 1);

    tracker.update_class_scores({ ClassScore(1, 0.1f), ClassScore(2, 0.9f) }, 2, 0.9f);
    tracker.update_class_scores({ ClassScore(1, 0.1f), ClassScore(2, 0.9f) }, 2, 0.9f);
    CHECK(tracker.classId == 2);
}

TEST_CASE( "SortTracker does not reacquire a missed track onto a nearby object", "[libopenshot][opencv][tracker]" )
{
    SortTracker sort(50, 1, 7, 0.1, 0.5, 0.0);
    const double diagonal = std::sqrt(1920.0 * 1920.0 + 1080.0 * 1080.0);

    sort.update(
        { cv::Rect(100, 100, 60, 60) },
        1,
        diagonal,
        { 0.95f },
        { 2 },
        { { ClassScore(2, 0.95f) } }
    );
    sort.update(
        { cv::Rect(100, 100, 60, 60) },
        2,
        diagonal,
        { 0.95f },
        { 2 },
        { { ClassScore(2, 0.95f) } }
    );
    REQUIRE(sort.frameTrackingResult.size() == 1);
    const int first_id = sort.frameTrackingResult[0].id;

    sort.update({}, 3, diagonal, {}, {}, {});
    REQUIRE(sort.frameTrackingResult.size() == 1);
    CHECK(sort.frameTrackingResult[0].id == first_id);

    sort.update(
        { cv::Rect(100, 145, 60, 60) },
        4,
        diagonal,
        { 0.95f },
        { 2 },
        { { ClassScore(2, 0.95f) } }
    );

    bool original_track_coasted = false;
    for (const auto& result : sort.frameTrackingResult) {
        if (result.id == first_id) {
            original_track_coasted = true;
            CHECK(result.box.y < 130.0f);
        }
    }
    CHECK(original_track_coasted);
    CHECK(sort.trackers.size() >= 2);
}

TEST_CASE( "SortTracker rejects adjacent-object handoff for active track", "[libopenshot][opencv][tracker]" )
{
    SortTracker sort(50, 1, 3, 0.1, 0.5, 0.0);
    const double diagonal = std::sqrt(960.0 * 960.0 + 540.0 * 540.0);

    sort.update(
        { cv::Rect(299, 181, 112, 97) },
        1,
        diagonal,
        { 0.80f },
        { 2 },
        { { ClassScore(2, 0.80f) } }
    );
    sort.update(
        { cv::Rect(299, 181, 112, 97) },
        2,
        diagonal,
        { 0.80f },
        { 2 },
        { { ClassScore(2, 0.80f) } }
    );
    REQUIRE(sort.frameTrackingResult.size() == 1);
    const int first_id = sort.frameTrackingResult[0].id;

    sort.update(
        { cv::Rect(248, 156, 103, 71) },
        3,
        diagonal,
        { 0.77f },
        { 2 },
        { { ClassScore(2, 0.77f) } }
    );

    bool original_track_did_not_jump = false;
    for (const auto& result : sort.frameTrackingResult) {
        if (result.id == first_id) {
            original_track_did_not_jump = true;
            CHECK(result.box.x > 285.0f);
            CHECK(result.box.y > 170.0f);
        }
    }
    CHECK(original_track_did_not_jump);
    CHECK(sort.trackers.size() >= 2);
}

TEST_CASE( "SortTracker rejects tiny nested detection for vehicle track", "[libopenshot][opencv][tracker]" )
{
    SortTracker sort(50, 1, 3, 0.1, 0.5, 0.0);
    const double diagonal = std::sqrt(960.0 * 960.0 + 540.0 * 540.0);

    sort.update(
        { cv::Rect(520, 178, 123, 91) },
        1,
        diagonal,
        { 0.77f },
        { 2 },
        { { ClassScore(2, 0.77f) } }
    );
    sort.update(
        { cv::Rect(520, 178, 123, 91) },
        2,
        diagonal,
        { 0.77f },
        { 2 },
        { { ClassScore(2, 0.77f) } }
    );
    REQUIRE(sort.frameTrackingResult.size() == 1);
    const int car_id = sort.frameTrackingResult[0].id;

    sort.update(
        { cv::Rect(592, 198, 30, 13) },
        3,
        diagonal,
        { 0.36f },
        { 0 },
        { { ClassScore(0, 0.36f), ClassScore(2, 0.15f) } }
    );

    bool car_track_did_not_shrink = false;
    for (const auto& result : sort.frameTrackingResult) {
        if (result.id == car_id) {
            car_track_did_not_shrink = true;
            CHECK(result.box.width > 90.0f);
            CHECK(result.box.height > 70.0f);
        }
    }
    CHECK(car_track_did_not_shrink);
    CHECK(sort.trackers.size() >= 2);
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
