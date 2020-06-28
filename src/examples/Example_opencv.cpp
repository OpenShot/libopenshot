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
// #include <google/protobuf/util/time_util.h>
#include "../../include/CVTracker.h"
// #include "treackerdata.pb.h"

#include "../../include/OpenShot.h"
#include "../../include/CrashHandler.h"

using namespace openshot;
using namespace cv;



void trackVideo(openshot::FFmpegReader &r9){
    // Opencv display window
    cv::namedWindow("Display Image", cv::WINDOW_NORMAL );
    // Create Tracker
    CVTracker kcfTracker;
    bool trackerInit = false;

    for (long int frame = 1100; frame <= 1500; frame++)
    {
        int frame_number = frame;
        std::shared_ptr<openshot::Frame> f = r9.GetFrame(frame_number);
        
        // Grab Mat image
        cv::Mat cvimage = f->GetImageCV();
        cvtColor(cvimage, cvimage, CV_RGB2BGR);

        if(!trackerInit){
            Rect2d bbox = selectROI("Display Image", cvimage);

            kcfTracker.initTracker(bbox, cvimage, frame_number);
            rectangle(cvimage, bbox, Scalar( 255, 0, 0 ), 2, 1 );

            trackerInit = true;
        }
        else{
            trackerInit = kcfTracker.trackFrame(cvimage, frame_number);
            
            // Draw box on image
            FrameData fd = kcfTracker.GetTrackedData(frame_number);
            // std::cout<< "fd: "<< fd.x1<< " "<< fd.y1 <<" "<<fd.x2<< " "<<fd.y2<<"\n";
            Rect2d box(fd.x1, fd.y1, fd.x2-fd.x1, fd.y2-fd.y1);
            rectangle(cvimage, box, Scalar( 255, 0, 0 ), 2, 1 );
        }
        
        cv::imshow("Display Image", cvimage);
        // Press  ESC on keyboard to exit
        char c=(char)waitKey(25);
        if(c==27)
            break;

    }

    // Save tracked data to file
    std::cout << "Saving tracker data!" << std::endl;
    kcfTracker.SaveTrackedData("kcf_tracker.data");
}

void displayTrackedData(openshot::FFmpegReader &r9){
    // Opencv display window
    cv::namedWindow("Display Image", cv::WINDOW_NORMAL );
    
    // Create Tracker
    CVTracker kcfTracker;
    // Load saved data
    if(!kcfTracker.LoadTrackedData("kcf_tracker.data")){
        std::cout<<"Was not possible to load the tracked data\n";
        return;
    }

    for (long int frame = 1100; frame <= 1500; frame++)
    {
        int frame_number = frame;
        std::shared_ptr<openshot::Frame> f = r9.GetFrame(frame_number);
        
        // Grab Mat image
        cv::Mat cvimage = f->GetImageCV();
        cvtColor(cvimage, cvimage, CV_RGB2BGR);

        FrameData fd = kcfTracker.GetTrackedData(frame_number);
        Rect2d box(fd.x1, fd.y1, fd.x2-fd.x1, fd.y2-fd.y1);
        rectangle(cvimage, box, Scalar( 255, 0, 0 ), 2, 1 );
    
        cv::imshow("Display Image", cvimage);
        // Press  ESC on keyboard to exit
        char c=(char)waitKey(25);
        if(c==27)
            break;
    }

}



int main(int argc, char* argv[]) {

    bool LOAD_TRACKED_DATA = false;

    openshot::Settings *s = openshot::Settings::Instance();
    s->HARDWARE_DECODER = 2; // 1 VA-API, 2 NVDEC, 6 VDPAU
    s->HW_DE_DEVICE_SET = 0;

    std::string input_filepath = TEST_MEDIA_PATH;
    input_filepath += "Boneyard Memories.mp4";

    openshot::FFmpegReader r9(input_filepath);
    r9.Open();
    r9.DisplayInfo();

    if(!LOAD_TRACKED_DATA)
        trackVideo(r9);
    else
        displayTrackedData(r9);
     

    // Close timeline
    r9.Close();

	std::cout << "Completed successfully!" << std::endl;

    return 0;
}
