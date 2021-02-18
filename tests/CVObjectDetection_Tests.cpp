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

#include "UnitTest++.h"
// Prevent name clashes with juce::UnitTest
#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "Clip.h"
#include "CVObjectDetection.h"
#include "ProcessingController.h"
#include "Json.h"

using namespace openshot;

std::string effectInfo =(" {\"protobuf_data_path\": \"objdetector.data\", "
                         "  \"processing_device\": \"GPU\", "
                         "  \"model_configuration\": \"~/yolo/yolov3.cfg\", "
                         "  \"model_weights\": \"~/yolo/yolov3.weights\", "
                         "  \"classes_file\": \"~/yolo/obj.names\"} ");

SUITE(CVObjectDetection_Tests)
{

    // Just for the stabilizer constructor, it won't be used
    ProcessingController processingController;

    TEST(DetectObject_Video)
    {
        // Create a video clip
        std::stringstream path;
        path << TEST_MEDIA_PATH << "run.mp4";

        // Open clip
        openshot::Clip c1(path.str());
        c1.Open();

        //TODO remove hardcoded path
        CVObjectDetection objectDetector(effectInfo, processingController);

        objectDetector.detectObjectsClip(c1, 0, 20, true);

        CVDetectionData dd = objectDetector.GetDetectionData(20);

        float x1 = dd.boxes.at(20).x;
        float y1 = dd.boxes.at(20).y;
        float x2 = x1 + dd.boxes.at(20).width;
        float y2 = y1 + dd.boxes.at(20).height;
        float confidence = dd.confidences.at(20);
        int classId = dd.classIds.at(20);

        CHECK_EQUAL((int) (x1 * 720), 106);
        CHECK_EQUAL((int) (y1 * 400), 21);
        CHECK_EQUAL((int) (x2 * 720), 628);
        CHECK_EQUAL((int) (y2 * 400), 429);
        CHECK_EQUAL((int) (confidence * 1000), 554);
        CHECK_EQUAL(classId, 0);

    }


    TEST(SaveLoad_Protobuf)
    {

        // Create a video clip
        std::stringstream path;
        path << TEST_MEDIA_PATH << "run.mp4";

        // Open clip
        openshot::Clip c1(path.str());
        c1.Open();

        //TODO remove hardcoded path
        CVObjectDetection objectDetector_1(effectInfo ,processingController);

        objectDetector_1.detectObjectsClip(c1, 0, 20, true);

        CVDetectionData dd_1 = objectDetector_1.GetDetectionData(20);

        float x1_1 = dd_1.boxes.at(20).x;
        float y1_1 = dd_1.boxes.at(20).y;
        float x2_1 = x1_1 + dd_1.boxes.at(20).width;
        float y2_1 = y1_1 + dd_1.boxes.at(20).height;
        float confidence_1 = dd_1.confidences.at(20);
        int classId_1 = dd_1.classIds.at(20);

        objectDetector_1.SaveObjDetectedData();

        CVObjectDetection objectDetector_2(effectInfo, processingController);

        objectDetector_2._LoadObjDetectdData();

        CVDetectionData dd_2 = objectDetector_2.GetDetectionData(20);

        float x1_2 = dd_2.boxes.at(20).x;
        float y1_2 = dd_2.boxes.at(20).y;
        float x2_2 = x1_2 + dd_2.boxes.at(20).width;
        float y2_2 = y1_2 + dd_2.boxes.at(20).height;
        float confidence_2 = dd_2.confidences.at(20);
        int classId_2 = dd_2.classIds.at(20);

        CHECK_EQUAL((int) (x1_1 * 720), (int) (x1_2 * 720));
        CHECK_EQUAL((int) (y1_1 * 400), (int) (y1_2 * 400));
        CHECK_EQUAL((int) (x2_1 * 720), (int) (x2_2 * 720));
        CHECK_EQUAL((int) (y2_1 * 400), (int) (y2_2 * 400));
        CHECK_EQUAL((int) (confidence_1 * 1000), (int) (confidence_2 * 1000));
        CHECK_EQUAL(classId_1, classId_2);

    }

} // SUITE(Frame_Tests)
