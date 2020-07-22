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

#include "UnitTest++.h"
// Prevent name clashes with juce::UnitTest
#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "../include/OpenShot.h"
#include "../include/ProcessingController.h"
#include <QImage>

using namespace openshot;

SUITE(CVStabilizer_Tests)
{

    // Just for the stabilizer constructor, it won't be used
    ProcessingController processingController;

    TEST(Stabilize_Video)
    {
        // Create a video clip
        std::stringstream path;
        path << TEST_MEDIA_PATH << "test.avi";

        // Open clip
        openshot::Clip c1(path.str());
        c1.Open();

        // Create stabilizer
        CVStabilization stabilizer("{\"protobuf_data_path\": \"stabilizer.data\", \"smoothing_window\": 30}", processingController);

        // Stabilize clip for frames 0-21
        stabilizer.stabilizeClip(c1, 0, 21, true);

        // Get stabilized data
        TransformParam tp = stabilizer.GetTransformParamData(20);
        CamTrajectory ct = stabilizer.GetCamTrajectoryTrackedData(20);

        // Compare if stabilized data is equal to pre-tested ones
        int dx = tp.dx*10000;
        int dy = tp.dy*10000;
        int da = tp.da*10000;
        int x = ct.x*10000;
        int y = ct.y*10000;
        int a = ct.a*10000;
            
        CHECK_EQUAL((int) (37.5902 * 10000), dx);
        CHECK_EQUAL((int) (-31.8099 * 10000), dy);
        CHECK_EQUAL((int) (0.00720559 * 10000), da);
        CHECK_EQUAL((int) (-0.41082 * 10000), x);
        CHECK_EQUAL((int) (-0.368437 * 10000), y);
        CHECK_EQUAL((int) (-0.000501644 * 10000), a);
    }


    TEST(SaveLoad_Protobuf)
    {

        // Create a video clip
        std::stringstream path;
        path << TEST_MEDIA_PATH << "test.avi";

        // Open clip
        openshot::Clip c1(path.str());
        c1.Open();

        // Create first stabilizer
        CVStabilization stabilizer_1("{\"protobuf_data_path\": \"stabilizer.data\", \"smoothing_window\": 30}", processingController);

        // Stabilize clip for frames 0-20
        stabilizer_1.stabilizeClip(c1, 0, 20+1, true);

        // Get stabilized data
        TransformParam tp_1 = stabilizer_1.GetTransformParamData(20);
        CamTrajectory ct_1 = stabilizer_1.GetCamTrajectoryTrackedData(20);

        // Save stabilized data
        stabilizer_1.SaveStabilizedData();

        // Create second stabilizer
        CVStabilization stabilizer_2("{\"protobuf_data_path\": \"stabilizer.data\", \"smoothing_window\": 30}", processingController);

        // Load stabilized data from first stabilizer protobuf data
        stabilizer_2.LoadStabilizedData();

        // Get stabilized data
        TransformParam tp_2 = stabilizer_2.GetTransformParamData(20);
        CamTrajectory ct_2 = stabilizer_2.GetCamTrajectoryTrackedData(20);
        
        // Compare first stabilizer data with second stabilizer data
        CHECK_EQUAL((int) (tp_1.dx * 10000), (int) (tp_2.dx *10000));
        CHECK_EQUAL((int) (tp_1.dy * 10000), (int) (tp_2.dy * 10000));
        CHECK_EQUAL((int) (tp_1.da * 10000), (int) (tp_2.da * 10000));
        CHECK_EQUAL((int) (ct_1.x * 10000), (int) (ct_2.x * 10000));
        CHECK_EQUAL((int) (ct_1.y * 10000), (int) (ct_2.y * 10000));
        CHECK_EQUAL((int) (ct_1.a * 10000), (int) (ct_2.a * 10000));
    }

} // SUITE(Frame_Tests)
