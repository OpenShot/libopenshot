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
#include <google/protobuf/util/time_util.h>
#include "../../include/CVTracker.h"
#include "../../include/CVStabilization.h"
#include "../../include/trackerdata.pb.h"

#include "../../include/OpenShot.h"
#include "../../include/CrashHandler.h"

using namespace openshot;
// using namespace cv;

void displayTrackedData(openshot::Clip &r9){
    // Opencv display window
    cv::namedWindow("Display Image", cv::WINDOW_NORMAL );
    
    // Create Tracker
    CVTracker kcfTracker;
    // Load saved data
    if(!kcfTracker.LoadTrackedData("kcf_tracker.data")){
        std::cout<<"Was not possible to load the tracked data\n";
        return;
    }

    for (long int frame = 1200; frame <= 1600; frame++)
    {
        int frame_number = frame;
        std::shared_ptr<openshot::Frame> f = r9.GetFrame(frame_number);
        
        // Grab Mat image
        cv::Mat cvimage = f->GetImageCV();

        FrameData fd = kcfTracker.GetTrackedData(frame_number);
        cv::Rect2d box(fd.x1, fd.y1, fd.x2-fd.x1, fd.y2-fd.y1);
        cv::rectangle(cvimage, box, cv::Scalar( 255, 0, 0 ), 2, 1 );
    
        cv::imshow("Display Image", cvimage);
        // Press  ESC on keyboard to exit
        char c=(char)cv::waitKey(25);
        if(c==27)
            break;
    }

}


void displayClip(openshot::Clip &r9){
    // Opencv display window
    cv::namedWindow("Display Image", cv::WINDOW_NORMAL );
    
    int videoLenght = r9.Reader()->info.video_length;

    for (long int frame = 0; frame < videoLenght; frame++)
    {
        int frame_number = frame;
        std::shared_ptr<openshot::Frame> f = r9.GetFrame(frame_number);
        // Grab Mat image
        cv::Mat cvimage = f->GetImageCV();
        cv::imshow("Display Image", cvimage);
        // Press  ESC on keyboard to exit
        char c=(char)cv::waitKey(25);
        if(c==27)
            break;
    }

}

int main(int argc, char* argv[]) {

    bool TRACK_DATA = false;
    bool LOAD_TRACKED_DATA = false;
    bool SMOOTH_VIDEO = true;
    bool LOAD_SMOOTH_DATA = false;


    std::string input_filepath = TEST_MEDIA_PATH;
    input_filepath = "../../src/examples/test.avi";

    openshot::Clip r9(input_filepath);
    r9.Open();

    if(TRACK_DATA)
        ClipProcessingJobs clipProcessing("Track", r9);
    if(LOAD_TRACKED_DATA){
        /***/
    }
    if(SMOOTH_VIDEO)
        ClipProcessingJobs clipProcessing("Stabilize", r9);
    if(LOAD_SMOOTH_DATA){
        /***/
    }
    displayClip(r9);

    // Close timeline
    r9.Close();

	std::cout << "Completed successfully!" << std::endl;

    return 0;
}
