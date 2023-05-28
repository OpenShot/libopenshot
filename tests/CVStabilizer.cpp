/**
 * @file
 * @brief Unit tests for CVStabilizer
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
#include "CVStabilization.h"  // for TransformParam, CamTrajectory, CVStabilization
#include "ProcessingController.h"

using namespace openshot;

// Just for the stabilizer constructor, it won't be used
ProcessingController stabilizer_pc;

TEST_CASE( "Stabilize_Video", "[libopenshot][opencv][stabilizer]" )
{
    // Create a video clip
    std::stringstream path;
    path << TEST_MEDIA_PATH << "test.avi";

    // Open clip
    openshot::Clip c1(path.str());
    c1.Open();

    std::string json_data = R"proto(
    {
        "protobuf_data_path": "stabilizer.data",
        "smoothing-window": 30
    } )proto";

    // Create stabilizer
    CVStabilization stabilizer(json_data, stabilizer_pc);

    // Stabilize clip for frames 0-21
    stabilizer.stabilizeClip(c1, 1, 21, true);

    // Get stabilized data
    TransformParam tp = stabilizer.GetTransformParamData(20);
    CamTrajectory ct = stabilizer.GetCamTrajectoryTrackedData(20);

    // // Compare if stabilized data is equal to pre-tested ones
    int dx = tp.dx*1000;
    int dy = tp.dy*1000;
    int da = tp.da*1000;
    int x = ct.x*1000;
    int y = std::round(ct.y*1000);
    int a = ct.a*1000;

    CHECK(dx == (int) (58));
    CHECK(dy == (int) (-88));
    CHECK(da == (int) (7));
    CHECK(x == (int) (0));
    CHECK(y == (int) (-1));
    CHECK(a == (int) (0));
}


TEST_CASE( "SaveLoad_Protobuf", "[libopenshot][opencv][stabilizer]" )
{

    // Create a video clip
    std::stringstream path;
    path << TEST_MEDIA_PATH << "test.avi";

    // Open clip
    openshot::Clip c1(path.str());
    c1.Open();

    std::string json_data = R"proto(
    {
        "protobuf_data_path": "stabilizer.data",
        "smoothing-window": 30
    } )proto";

    // Create first stabilizer
    CVStabilization stabilizer_1(json_data, stabilizer_pc);

    // Stabilize clip for frames 0-20
    stabilizer_1.stabilizeClip(c1, 1, 20+1, true);

    // Get stabilized data
    TransformParam tp_1 = stabilizer_1.GetTransformParamData(20);
    CamTrajectory ct_1 = stabilizer_1.GetCamTrajectoryTrackedData(20);

    // Save stabilized data
    stabilizer_1.SaveStabilizedData();

    // Create second stabilizer
    CVStabilization stabilizer_2(json_data, stabilizer_pc);

    // Load stabilized data from first stabilizer protobuf data
    stabilizer_2._LoadStabilizedData();

    // Get stabilized data
    TransformParam tp_2 = stabilizer_2.GetTransformParamData(20);
    CamTrajectory ct_2 = stabilizer_2.GetCamTrajectoryTrackedData(20);

    // Compare first stabilizer data with second stabilizer data
    CHECK((int) (tp_1.dx * 10000) == (int) (tp_2.dx *10000));
    CHECK((int) (tp_1.dy * 10000) == (int) (tp_2.dy * 10000));
    CHECK((int) (tp_1.da * 10000) == (int) (tp_2.da * 10000));
    CHECK((int) (ct_1.x * 10000) == (int) (ct_2.x * 10000));
    CHECK((int) (ct_1.y * 10000) == (int) (ct_2.y * 10000));
    CHECK((int) (ct_1.a * 10000) == (int) (ct_2.a * 10000));
}
