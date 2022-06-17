/**
 * @file
 * @brief Unit tests for CVObjectDetection
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
#include "CVObjectDetection.h"
#include "ProcessingController.h"
#include "Json.h"

using namespace openshot;

std::string effectInfo =(" {\"protobuf_data_path\": \"objdetector.data\", "
                         "  \"processing_device\": \"GPU\", "
                         "  \"model_configuration\": \"~/yolo/yolov3.cfg\", "
                         "  \"model_weights\": \"~/yolo/yolov3.weights\", "
                         "  \"classes_file\": \"~/yolo/obj.names\"} ");

// Just for the stabilizer constructor, it won't be used
ProcessingController processingController;

TEST_CASE( "DetectObject_Video", "[libopenshot][opencv][objectdetection]" )
{
    // Create a video clip
    std::stringstream path;
    path << TEST_MEDIA_PATH << "run.mp4";

    // Open clip
    openshot::Clip c1(path.str());
    c1.Open();

    //TODO remove hardcoded path
    CVObjectDetection objectDetector(effectInfo, processingController);

    objectDetector.detectObjectsClip(c1, 1, 20, true);

    CVDetectionData dd = objectDetector.GetDetectionData(20);

    float x1 = dd.boxes.at(20).x;
    float y1 = dd.boxes.at(20).y;
    float x2 = x1 + dd.boxes.at(20).width;
    float y2 = y1 + dd.boxes.at(20).height;
    float confidence = dd.confidences.at(20);
    int classId = dd.classIds.at(20);

    CHECK((int) (x1 * 720) == 106);
    CHECK((int) (y1 * 400) == 21);
    CHECK((int) (x2 * 720) == 628);
    CHECK((int) (y2 * 400) == 429);
    CHECK((int) (confidence * 1000) == 554);
    CHECK(classId == 0);

}


TEST_CASE( "SaveLoad_Protobuf", "[libopenshot][opencv][objectdetection]" )
{

    // Create a video clip
    std::stringstream path;
    path << TEST_MEDIA_PATH << "run.mp4";

    // Open clip
    openshot::Clip c1(path.str());
    c1.Open();

    //TODO remove hardcoded path
    CVObjectDetection objectDetector_1(effectInfo ,processingController);

    objectDetector_1.detectObjectsClip(c1, 1, 20, true);

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

    CHECK((int) (x1_1 * 720) == (int) (x1_2 * 720));
    CHECK((int) (y1_1 * 400) == (int) (y1_2 * 400));
    CHECK((int) (x2_1 * 720) == (int) (x2_2 * 720));
    CHECK((int) (y2_1 * 400) == (int) (y2_2 * 400));
    CHECK((int) (confidence_1 * 1000) == (int) (confidence_2 * 1000));
    CHECK(classId_1 == classId_2);
}
