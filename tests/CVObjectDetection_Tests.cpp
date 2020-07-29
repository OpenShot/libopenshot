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

SUITE(CVObjectDetection_Tests)
{

    // Just for the stabilizer constructor, it won't be used
    ProcessingController processingController;

    TEST(DetectObject_Video)
    {
        // Create a video clip
        std::stringstream path;
        path << TEST_MEDIA_PATH << "test_video.mp4";

        // Open clip
        openshot::Clip c1(path.str());
        c1.Open();

        CVObjectDetection objectDetector("\"processing_device\": \"GPU\"", processingController);

        objectDetector.detectObjectsClip(c1, 0, 100, true);

        CVDetectionData dd = objectDetector.GetDetectionData(20);

        // int x1 = dd.boxes[20].x;
        // int y1 = dd.boxes[20].y;
        // int x2 = x1 + dd.boxes[20].width();
        // int y2 = y2 + dd.boxes[20].height();
        // float confidence = dd.confidences[20];
        // int classId = dd.classIds[20];

    }


    TEST(SaveLoad_Protobuf)
    {

        // Create a video clip
        std::stringstream path;
        path << TEST_MEDIA_PATH << "test_video.mp4";

        // Open clip
        openshot::Clip c1(path.str());
        c1.Open();

        CVObjectDetection objectDetector_1("{\"protobuf_data_path\": \"object_detector.data\", \"processing_device\": \"GPU\"}", processingController);

        objectDetector_1.detectObjectsClip(c1, 0, 100, true);

        CVDetectionData dd_1 = objectDetector_1.GetDetectionData(20);

        objectDetector_1.SaveTrackedData();

        CVObjectDetection objectDetector_2("{\"protobuf_data_path\": \"object_detector.data\", \"processing_device\": \"\"}", processingController);

        // objectDetector_2.LoadTrackedData();

        CVDetectionData dd_2 = objectDetector_2.GetDetectionData(20);
        
        // int x1_1 = dd_1.boxes[20].x;
        // int y1_1 = dd_1.boxes[20].y;
        // int x2_1 = x1_1 + dd_1.boxes[20].width();
        // int y2_1 = y2_1 + dd_1.boxes[20].height();
        // float confidence_1 = dd_1.confidences[20];
        // int classId_1 = dd_1.classIds[20];

        // int x1_2 = dd_2.boxes[20].x;
        // int y1_2 = dd_2.boxes[20].y;
        // int x2_2 = x1_2 + dd_2.boxes[20].width();
        // int y2_2 = y2_2 + dd_2.boxes[20].height();
        // float confidence_2 = dd_2.confidences[20];
        // int classId_2 = dd_2.classIds[20];

    }

} // SUITE(Frame_Tests)
