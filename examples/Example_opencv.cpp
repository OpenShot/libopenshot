/**
 * @file
 * @brief Source file for Example Executable (example app for libopenshot)
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <fstream>
#include <iostream>
#include <memory>
#include "CVTracker.h"
#include "CVStabilization.h"
#include "CVObjectDetection.h"

#include "Clip.h"
#include "EffectBase.h"
#include "EffectInfo.h"
#include "Frame.h"
#include "CrashHandler.h"

using namespace openshot;
using namespace std;

/*
||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
The following methods are just for getting JSON info to the pre-processing effects
*/

string jsonFormat(string key, string value, string type="string"); // Format variables to the needed JSON format
string trackerJson(cv::Rect2d r, bool onlyProtoPath); // Set variable values for tracker effect
string stabilizerJson(bool onlyProtoPath); // Set variable values for stabilizer effect
string objectDetectionJson(bool onlyProtoPath); // Set variable values for object detector effect

/*
||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
*/

// Show the pre-processed clip on the screen
void displayClip(openshot::Clip &r9){

    // Opencv display window
    cv::namedWindow("Display Image", cv::WINDOW_NORMAL );

    // Get video lenght
    int videoLenght = r9.Reader()->info.video_length;

    // Loop through the clip and show it with the effects, if any
    for (long int frame = 0; frame < videoLenght; frame++)
    {
        int frame_number = frame;
        // Get the frame
        std::shared_ptr<openshot::Frame> f = r9.GetFrame(frame_number);
        // Grab OpenCV::Mat image
        cv::Mat cvimage = f->GetImageCV();

        // Display the frame
        cv::imshow("Display Image", cvimage);

        // Press ESC on keyboard to exit
        char c=(char)cv::waitKey(25);
        if(c==27)
            break;
    }
    // Destroy all remaining windows
    cv::destroyAllWindows();
}

int main(int argc, char* argv[]) {

    // Set pre-processing effects
    bool TRACK_DATA = true;
    bool SMOOTH_VIDEO = false;
    bool OBJECT_DETECTION_DATA = false;

    // Get media path
    std::stringstream path;
    path << TEST_MEDIA_PATH << ((OBJECT_DETECTION_DATA) ? "run.mp4" : "test.avi");
    //  run.mp4 --> Used for object detector
    // test.avi --> Used for tracker and stabilizer

    // Thread controller just for the pre-processing constructors, it won't be used
    ProcessingController processingController;

    // Open clip
    openshot::Clip r9(path.str());
    r9.Open();

    // Apply tracking effect on the clip
    if(TRACK_DATA){

        // Take the bounding box coordinates
        cv::Mat roi = r9.GetFrame(0)->GetImageCV();
        cv::Rect2d r = cv::selectROI(roi);
        cv::destroyAllWindows();

        // Create a tracker object by passing a JSON string and a thread controller, this last one won't be used
        // JSON info: path to save the tracked data, type of tracker and bbox coordinates
        CVTracker tracker(trackerJson(r, false), processingController);

        // Start the tracking
        tracker.trackClip(r9, 0, 0, true);
        // Save the tracked data
        tracker.SaveTrackedData();

        // Create a tracker effect
        EffectBase* e = EffectInfo().CreateEffect("Tracker");

        // Pass a JSON string with the saved tracked data
        // The effect will read and save the tracking in a map::<frame,data_struct>
        e->SetJson(trackerJson(r, true));
        // Add the effect to the clip
        r9.AddEffect(e);
    }

    // Apply stabilizer effect on the clip
    if(SMOOTH_VIDEO){

        // Create a stabilizer object by passing a JSON string and a thread controller, this last one won't be used
        // JSON info: path to save the stabilized data and smoothing window value
        CVStabilization stabilizer(stabilizerJson(false), processingController);

        // Start the stabilization
        stabilizer.stabilizeClip(r9, 0, 100, true);
        // Save the stabilization data
        stabilizer.SaveStabilizedData();

        // Create a stabilizer effect
        EffectBase* e = EffectInfo().CreateEffect("Stabilizer");

        // Pass a JSON string with the saved stabilized data
        // The effect will read and save the stabilization in a map::<frame,data_struct>
        e->SetJson(stabilizerJson(true));
        // Add the effect to the clip
        r9.AddEffect(e);
    }

    // Apply object detection effect on the clip
    if(OBJECT_DETECTION_DATA){

        // Create a object detection object by passing a JSON string and a thread controller, this last one won't be used
        // JSON info: path to save the detection data, processing devicee, model weights, model configuration and class names
        CVObjectDetection objectDetection(objectDetectionJson(false), processingController);

        // Start the object detection
        objectDetection.detectObjectsClip(r9, 0, 100, true);
        // Save the object detection data
        objectDetection.SaveObjDetectedData();

        // Create a object detector effect
        EffectBase* e = EffectInfo().CreateEffect("ObjectDetection");

        // Pass a JSON string with the saved detections data
        // The effect will read and save the detections in a map::<frame,data_struct>

        e->SetJson(objectDetectionJson(true));
        // Add the effect to the clip
        r9.AddEffect(e);
    }

    // Show the pre-processed clip on the screen
    displayClip(r9);

    // Close timeline
    r9.Close();

	std::cout << "Completed successfully!" << std::endl;

    return 0;
}



/*
||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

The following methods are just for getting JSON info to the pre-processing effects

||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
*/



string jsonFormat(string key, string value, string type){
    stringstream jsonFormatMessage;
    jsonFormatMessage << ( "\"" + key + "\": " );

    if(type == "string")
        jsonFormatMessage << ( "\"" + value + "\"" );
    if(type == "rstring")
        jsonFormatMessage <<  value;
    if(type == "int")
        jsonFormatMessage << stoi(value);
    if(type == "float")
        jsonFormatMessage << (float)stof(value);
    if(type == "double")
        jsonFormatMessage << (double)stof(value);
    if (type == "bool")
        jsonFormatMessage << ((value == "true" || value == "1") ? "true" : "false");

    return jsonFormatMessage.str();
}

// Return JSON string for the tracker effect
string trackerJson(cv::Rect2d r, bool onlyProtoPath){

    // Define path to save tracked data
    string protobufDataPath = "kcf_tracker.data";
    // Set the tracker
    string tracker = "KCF";

    // Construct all the composition of the JSON string
    string protobuf_data_path = jsonFormat("protobuf_data_path", protobufDataPath);
    string trackerType = jsonFormat("tracker-type", tracker);
    string bboxCoords = jsonFormat(
                                    "region",
                                            "{" + jsonFormat("x", to_string(r.x), "int") +
                                            "," + jsonFormat("y", to_string(r.y), "int") +
                                            "," + jsonFormat("width", to_string(r.width), "int") +
                                            "," + jsonFormat("height", to_string(r.height), "int") +
                                            "," + jsonFormat("first-frame", to_string(0), "int") +
                                            "}",
                                    "rstring");

    // Return only the the protobuf path in JSON format
    if(onlyProtoPath)
        return "{" + protobuf_data_path + "}";
    // Return all the parameters for the pre-processing effect
    else
        return "{" + protobuf_data_path + "," + trackerType + "," + bboxCoords + "}";
}

// Return JSON string for the stabilizer effect
string stabilizerJson(bool onlyProtoPath){

    // Define path to save stabilized data
    string protobufDataPath = "example_stabilizer.data";
    // Set smoothing window value
    string smoothingWindow = "30";

    // Construct all the composition of the JSON string
    string protobuf_data_path = jsonFormat("protobuf_data_path", protobufDataPath);
    string smoothing_window = jsonFormat("smoothing_window", smoothingWindow, "int");

    // Return only the the protobuf path in JSON format
    if(onlyProtoPath)
        return "{" + protobuf_data_path + "}";
    // Return all the parameters for the pre-processing effect
    else
        return "{" + protobuf_data_path + "," + smoothing_window + "}";
}

string objectDetectionJson(bool onlyProtoPath){

    // Define path to save object detection data
    string protobufDataPath = "example_object_detection.data";
    // Define processing device
    string processingDevice = "GPU";
    // Set path to model configuration file
    string modelConfiguration = "yolov3.cfg";
    // Set path to model weights
    string modelWeights = "yolov3.weights";
    // Set path to class names file
    string classesFile = "obj.names";

    // Construct all the composition of the JSON string
    string protobuf_data_path = jsonFormat("protobuf_data_path", protobufDataPath);
    string processing_device = jsonFormat("processing_device", processingDevice);
    string model_configuration = jsonFormat("model_configuration", modelConfiguration);
    string model_weights = jsonFormat("model_weights", modelWeights);
    string classes_file = jsonFormat("classes_file", classesFile);

    // Return only the the protobuf path in JSON format
    if(onlyProtoPath)
        return "{" + protobuf_data_path + "}";
    else
        return "{" + protobuf_data_path + "," + processing_device + "," + model_configuration + ","
                + model_weights + "," + classes_file + "}";
}
