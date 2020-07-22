/**
 * @file
 * @brief Source file for Example Executable (example app for libopenshot)
 * @author Jonathan Thomas <jonathan@openshot.org>
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

#include <fstream>
#include <iostream>
#include <memory>
#include "../../include/CVTracker.h"
#include "../../include/CVStabilization.h"

#include "../../include/OpenShot.h"
#include "../../include/CrashHandler.h"

using namespace openshot;
using namespace std;

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
        // Convert color scheme from RGB (QImage scheme) to BGR (OpenCV scheme) 
        cv::cvtColor(cvimage, cvimage, cv::COLOR_RGB2BGR);
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


/*
||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

The following methods are just for getting JSON info to the pre-processing effects

||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
*/


// Return JSON string for the tracker effect
string trackerJson(cv::Rect2d r, bool onlyProtoPath){
    // Set the tracker
    string tracker = "KCF";

    // Construct all the composition of the JSON string
    string trackerType = "\"tracker_type\": \"" + tracker + "\"";
    string protobuf_data_path = "\"protobuf_data_path\": \"kcf_tracker.data\"";
    stringstream bboxCoords;
    bboxCoords << "\"bbox\": {\"x\":"<<r.x<<", \"y\": "<<r.y<<", \"w\": "<<r.width<<", \"h\": "<<r.height<<"}";
    
    // Return only the the protobuf path in JSON format
    if(onlyProtoPath)
        return "{" + protobuf_data_path + "}";
    // Return all the parameters for the pre-processing effect
    else
        return "{" + protobuf_data_path + ", " + trackerType + ", " + bboxCoords.str() + "}";
}

// Return JSON string for the stabilizer effect
string stabilizerJson(bool onlyProtoPath){
    // Set smoothing window value
    int smoothingWindow = 30;

    // Construct all the composition of the JSON string
    string protobuf_data_path = "\"protobuf_data_path\": \"example_stabilizer.data\"";
    stringstream smoothing_window;
    smoothing_window << "\"smoothing_window\": "<< smoothingWindow;

    // Return only the the protobuf path in JSON format
    if(onlyProtoPath)
        return "{" + protobuf_data_path + "}";
    // Return all the parameters for the pre-processing effect
    else
        return "{" + protobuf_data_path + ", " + smoothing_window.str() + "}";
}

string objectDetectionJson(bool onlyProtoPath){

    // Construct all the composition of the JSON string
    string protobuf_data_path = "\"protobuf_data_path\": \"example_object_detection.data\"";

    // Return only the the protobuf path in JSON format
    if(onlyProtoPath)
        return "{" + protobuf_data_path + "}";
}

int main(int argc, char* argv[]) {

    // Set pre-processing effects
    bool TRACK_DATA = false;
    bool SMOOTH_VIDEO = true;
    bool OBJECT_DETECTION_DATA = false;

    // Get media path
    std::stringstream path;
    path << TEST_MEDIA_PATH << "test.avi";

    // Thread controller just for the pre-processing constructors, it won't be used
    ProcessingController processingController;

    // Open clip
    openshot::Clip r9(path.str());
    r9.Open();

    // Aplly tracking effect on the clip
    if(TRACK_DATA){

        // Take the bounding box coordinates
        cv::Mat roi = r9.GetFrame(0)->GetImageCV();
        cv::Rect2d r = cv::selectROI(roi);
        cv::destroyAllWindows();

        // Create a tracker object by passing a JSON string and a thread controller, this last one won't be used
        // JSON info: path to save the tracked data, type of tracker and bbox coordinates
        CVTracker tracker(trackerJson(r, false), processingController);

        // Start the tracking
        tracker.trackClip(r9);
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

    // Aplly stabilizer effect on the clip
    if(SMOOTH_VIDEO){

        // Create a stabilizer object by passing a JSON string and a thread controller, this last one won't be used
        // JSON info: path to save the stabilized data and smoothing window value
        CVStabilization stabilizer(stabilizerJson(false), processingController);

        // Start the stabilization
        stabilizer.stabilizeClip(r9);
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

    if(OBJECT_DETECTION_DATA){
        // CVObjectDetection objectDetection("GPU");
        // objectDetection.ProcessClip(r9);
    }

    // Show the pre-processed clip on the screen
    displayClip(r9);

    // Close timeline
    r9.Close();

	std::cout << "Completed successfully!" << std::endl;

    return 0;
}
