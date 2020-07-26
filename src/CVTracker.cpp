/**
 * @file
 * @brief Track an object selected by the user
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

#include "../include/CVTracker.h"

// Constructor
CVTracker::CVTracker(std::string processInfoJson, ProcessingController &processingController)
: processingController(&processingController), json_interval(false){   
    SetJson(processInfoJson);
}

// Set desirable tracker method
cv::Ptr<cv::Tracker> CVTracker::selectTracker(std::string trackerType){
    cv::Ptr<cv::Tracker> t;

    if (trackerType == "BOOSTING")
        t = cv::TrackerBoosting::create();
    if (trackerType == "MIL")
        t = cv::TrackerMIL::create();
    if (trackerType == "KCF")
        t = cv::TrackerKCF::create();
    if (trackerType == "TLD")
        t = cv::TrackerTLD::create();
    if (trackerType == "MEDIANFLOW")
        t = cv::TrackerMedianFlow::create();
    if (trackerType == "GOTURN")
        t = cv::TrackerGOTURN::create();
    if (trackerType == "MOSSE")
        t = cv::TrackerMOSSE::create();
    if (trackerType == "CSRT")
        t = cv::TrackerCSRT::create();

    return t;
}

// Track object in the hole clip or in a given interval
void CVTracker::trackClip(openshot::Clip& video, size_t _start, size_t _end, bool process_interval){

    video.Open();

    if(!json_interval){
        start = _start; end = _end;

        if(!process_interval || end <= 0 || end-start <= 0){
            // Get total number of frames in video
            start = video.Start() * video.Reader()->info.fps.ToInt();
            end = video.End() * video.Reader()->info.fps.ToInt();
        }
    }
    else{
        start = start + video.Start() * video.Reader()->info.fps.ToInt();
        end = video.End() * video.Reader()->info.fps.ToInt();
    }

    bool trackerInit = false;

    SortTracker sort;
    RemoveJitter removeJitter(0);

    size_t frame;
    // Loop through video
    for (frame = start; frame <= end; frame++)
    {

        // Stop the feature tracker process
        if(processingController->ShouldStop()){
            return;
        }

        std::cout<<"Frame: "<<frame<<"\n";
        size_t frame_number = frame;
        // Get current frame
        std::shared_ptr<openshot::Frame> f = video.GetFrame(frame_number);
        
        // Grab OpenCV Mat image
        cv::Mat cvimage = f->GetImageCV();

        // Pass the first frame to initialize the tracker
        if(!trackerInit){
            
            // Initialize the tracker
            initTracker(cvimage, frame_number);

            trackerInit = true;
        }
        else{
            // Update the object tracker according to frame 
            trackerInit = trackFrame(cvimage, frame_number, sort, removeJitter);
            
            // Draw box on image
            FrameData fd = GetTrackedData(frame_number);

        }
        // Update progress
        processingController->SetProgress(uint(100*(frame_number-start)/(end-start)));
    }
}

// Initialize the tracker
bool CVTracker::initTracker(cv::Mat &frame, size_t frameId){

    // Create new tracker object
    tracker = selectTracker(trackerType);

    // Initialize tracker
    tracker->init(frame, bbox);

    // Add new frame data
    trackedDataById[frameId] = FrameData(frameId, 0, bbox.x, bbox.y, bbox.x+bbox.width, bbox.y+bbox.height);

    return true;
}

// Update the object tracker according to frame 
bool CVTracker::trackFrame(cv::Mat &frame, size_t frameId, SortTracker &sort, RemoveJitter &removeJitter){
    // Update the tracking result
    bool ok = tracker->update(frame, bbox);

    // Add frame number and box coords if tracker finds the object
    // Otherwise add only frame number
    if (ok)
    {
        std::vector<cv::Rect> bboxes = {bbox};
        
        sort.update(bboxes, frameId, sqrt(pow(frame.rows, 2) + pow(frame.cols, 2)));

        for(auto TBox : sort.frameTrackingResult)
                bbox = TBox.box;

        // removeJitter.update(bbox, bbox);

        // Add new frame data
        trackedDataById[frameId] = FrameData(frameId, 0, bbox.x, bbox.y, bbox.x+bbox.width, bbox.y+bbox.height);
    }
    else
    {
        // Add new frame data
        trackedDataById[frameId] = FrameData(frameId);
    }

    return ok;
}

bool CVTracker::SaveTrackedData(){
    // Create tracker message
    libopenshottracker::Tracker trackerMessage;

    // Iterate over all frames data and save in protobuf message
    for(std::map<size_t,FrameData>::iterator it=trackedDataById.begin(); it!=trackedDataById.end(); ++it){
        FrameData fData = it->second;
        libopenshottracker::Frame* pbFrameData;
        AddFrameDataToProto(trackerMessage.add_frame(), fData);
    }

    // Add timestamp
    *trackerMessage.mutable_last_updated() = TimeUtil::SecondsToTimestamp(time(NULL));

    {
        // Write the new message to disk.
        std::fstream output(protobuf_data_path, ios::out | ios::trunc | ios::binary);
        if (!trackerMessage.SerializeToOstream(&output)) {
        cerr << "Failed to write protobuf message." << endl;
        return false;
        }
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;

}

// Add frame tracked data into protobuf message.
void CVTracker::AddFrameDataToProto(libopenshottracker::Frame* pbFrameData, FrameData& fData) {

    // Save frame number and rotation
    pbFrameData->set_id(fData.frame_id);
    pbFrameData->set_rotation(0);

    libopenshottracker::Frame::Box* box = pbFrameData->mutable_bounding_box();
    // Save bounding box data
    box->set_x1(fData.x1);
    box->set_y1(fData.y1);
    box->set_x2(fData.x2);
    box->set_y2(fData.y2);
}

// Load protobuf data file
bool CVTracker::LoadTrackedData(){
    // Create tracker message
    libopenshottracker::Tracker trackerMessage;

    {
        // Read the existing tracker message.
        fstream input(protobuf_data_path, ios::in | ios::binary);
        if (!trackerMessage.ParseFromIstream(&input)) {
            cerr << "Failed to parse protobuf message." << endl;
            return false;
        }
    }

    // Make sure the trackedData is empty
    trackedDataById.clear();

    // Iterate over all frames of the saved message
    for (size_t i = 0; i < trackerMessage.frame_size(); i++) {
        const libopenshottracker::Frame& pbFrameData = trackerMessage.frame(i);

        // Load frame and rotation data
        size_t id = pbFrameData.id();
        float rotation = pbFrameData.rotation();

        // Load bounding box data
        const libopenshottracker::Frame::Box& box = pbFrameData.bounding_box();
        int x1 = box.x1();
        int y1 = box.y1();
        int x2 = box.x2();
        int y2 = box.y2();

        // Assign data to tracker map
        trackedDataById[id] = FrameData(id, rotation, x1, y1, x2, y2);
    }

    // Show the time stamp from the last update in tracker data file 
    if (trackerMessage.has_last_updated()) {
        cout << "  Loaded Data. Saved Time Stamp: " << TimeUtil::ToString(trackerMessage.last_updated()) << endl;
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;
}

// Get tracker info for the desired frame 
FrameData CVTracker::GetTrackedData(size_t frameId){

    // Check if the tracker info for the requested frame exists
    if ( trackedDataById.find(frameId) == trackedDataById.end() ) {
        
        return FrameData();
    } else {
        
        return trackedDataById[frameId];
    }

}

// Load JSON string into this object
void CVTracker::SetJson(const std::string value) {
	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		// Set all values that match

		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw openshot::InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::Value into this object
void CVTracker::SetJsonValue(const Json::Value root) {
    
	// Set data from Json (if key is found)
	if (!root["protobuf_data_path"].isNull()){
		protobuf_data_path = (root["protobuf_data_path"].asString());
	}
    if (!root["tracker_type"].isNull()){
		trackerType = (root["tracker_type"].asString());
	}
    if (!root["bbox"].isNull()){
        double x = root["bbox"]["x"].asDouble();
        double y = root["bbox"]["y"].asDouble();
        double w = root["bbox"]["w"].asDouble();
        double h = root["bbox"]["h"].asDouble();
        cv::Rect2d prev_bbox(x,y,w,h);
        bbox = prev_bbox;
	}
    if (!root["first_frame"].isNull()){
        start = root["first_frame"].asInt64();
        json_interval = true;
    }
}

RemoveJitter::RemoveJitter(int boxesInterval) : boxesInterval(boxesInterval), boxesInVector(0){
}
    
void RemoveJitter::update(cv::Rect2d bbox, cv::Rect2d &out_bbox){

    bboxTracker.push_back(bbox);

    // Just to initialize the vector properly
    if(boxesInVector < boxesInterval+1){
        boxesInVector++;
        out_bbox = bbox;
    }
    else{
        cv::Rect2d old_bbox = bboxTracker.front();
        cv::Rect2d new_bbox = bboxTracker.back();

        int centerX_1 = old_bbox.x + old_bbox.width/2;
        int centerY_1 = old_bbox.y + old_bbox.height/2;
        int centerX_2 = new_bbox.x + new_bbox.width/2;
        int centerY_2 = new_bbox.y + new_bbox.height/2;

        int dif_centerXs = abs(centerX_1 - centerX_2);
        int dif_centerYs = abs(centerY_1 - centerY_2);

        cout<<dif_centerXs<<"\n";
        cout<<dif_centerYs<<"\n\n";

        if(dif_centerXs > 6 || dif_centerYs > 6){
            out_bbox = new_bbox;
        }
        else{
            cv::Rect2d mean_bbox((old_bbox.x + new_bbox.x)/2, (old_bbox.y + new_bbox.y)/2, (old_bbox.width + new_bbox.width)/2, (old_bbox.height + new_bbox.height)/2);
            out_bbox = mean_bbox;
        }

        bboxTracker.erase(bboxTracker.begin());
    }
}

