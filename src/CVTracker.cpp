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

#include <google/protobuf/util/time_util.h>

#include "OpenCVUtilities.h"
#include "CVTracker.h"

using namespace openshot;
using google::protobuf::util::TimeUtil;

// Constructor
CVTracker::CVTracker(std::string processInfoJson, ProcessingController &processingController)
: processingController(&processingController), json_interval(false){
    SetJson(processInfoJson);
    start = 0;
    end = 0;
}

// Set desirable tracker method
cv::Ptr<OPENCV_TRACKER_TYPE> CVTracker::selectTracker(std::string trackerType){

    if (trackerType == "BOOSTING")
        return OPENCV_TRACKER_NS::TrackerBoosting::create();
    if (trackerType == "MIL")
        return OPENCV_TRACKER_NS::TrackerMIL::create();
    if (trackerType == "KCF")
        return OPENCV_TRACKER_NS::TrackerKCF::create();
    if (trackerType == "TLD")
        return OPENCV_TRACKER_NS::TrackerTLD::create();
    if (trackerType == "MEDIANFLOW")
        return OPENCV_TRACKER_NS::TrackerMedianFlow::create();
    if (trackerType == "MOSSE")
        return OPENCV_TRACKER_NS::TrackerMOSSE::create();
    if (trackerType == "CSRT")
        return OPENCV_TRACKER_NS::TrackerCSRT::create();

    return nullptr;
}

// Track object in the hole clip or in a given interval
void CVTracker::trackClip(openshot::Clip& video, size_t _start, size_t _end, bool process_interval){

    video.Open();
    if(!json_interval){
        start = _start; end = _end;

        if(!process_interval || end <= 0 || end-start == 0){
            // Get total number of frames in video
            start = video.Start() * video.Reader()->info.fps.ToInt();
            end = video.End() * video.Reader()->info.fps.ToInt();
        }
    }
    else{
        start = start + video.Start() * video.Reader()->info.fps.ToInt();
        end = video.End() * video.Reader()->info.fps.ToInt();
    }

    if(error){
        return;
    }

    processingController->SetError(false, "");
    bool trackerInit = false;

    size_t frame;
    // Loop through video
    for (frame = start; frame <= end; frame++)
    {

        // Stop the feature tracker process
        if(processingController->ShouldStop()){
            return;
        }

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
            trackerInit = trackFrame(cvimage, frame_number);

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

    // Correct if bounding box contains negative proportions (width and/or height < 0)
    if(bbox.width < 0){
        bbox.x = bbox.x - abs(bbox.width);
        bbox.width = abs(bbox.width);
    }
    if(bbox.height < 0){
        bbox.y = bbox.y - abs(bbox.height);
        bbox.height = abs(bbox.height);
    }

    // Initialize tracker
    tracker->init(frame, bbox);

    float fw = frame.size().width;
    float fh = frame.size().height;

    // Add new frame data
    trackedDataById[frameId] = FrameData(frameId, 0, (bbox.x)/fw,
                                                     (bbox.y)/fh,
                                                     (bbox.x+bbox.width)/fw,
                                                     (bbox.y+bbox.height)/fh);

    return true;
}

// Update the object tracker according to frame
bool CVTracker::trackFrame(cv::Mat &frame, size_t frameId){
    // Update the tracking result
    bool ok = tracker->update(frame, bbox);

    // Add frame number and box coords if tracker finds the object
    // Otherwise add only frame number
    if (ok)
    {
        float fw = frame.size().width;
        float fh = frame.size().height;

        std::vector<cv::Rect> bboxes = {bbox};
        std::vector<float> confidence = {1.0};
        std::vector<int> classId = {1};

        sort.update(bboxes, frameId, sqrt(pow(frame.rows, 2) + pow(frame.cols, 2)), confidence, classId);

        for(auto TBox : sort.frameTrackingResult)
            bbox = TBox.box;

        // Add new frame data
        trackedDataById[frameId] = FrameData(frameId, 0, (bbox.x)/fw,
                                                         (bbox.y)/fh,
                                                         (bbox.x+bbox.width)/fw,
                                                         (bbox.y+bbox.height)/fh);
    }
    else
    {
        // Add new frame data
        trackedDataById[frameId] = FrameData(frameId);
    }

    return ok;
}

bool CVTracker::SaveTrackedData(){
    using std::ios;

    // Create tracker message
    pb_tracker::Tracker trackerMessage;

    // Iterate over all frames data and save in protobuf message
    for(std::map<size_t,FrameData>::iterator it=trackedDataById.begin(); it!=trackedDataById.end(); ++it){
        FrameData fData = it->second;
        pb_tracker::Frame* pbFrameData;
        AddFrameDataToProto(trackerMessage.add_frame(), fData);
    }

    // Add timestamp
    *trackerMessage.mutable_last_updated() = TimeUtil::SecondsToTimestamp(time(NULL));

    {
        // Write the new message to disk.
        std::fstream output(protobuf_data_path, ios::out | ios::trunc | ios::binary);
        if (!trackerMessage.SerializeToOstream(&output)) {
        std::cerr << "Failed to write protobuf message." << std::endl;
        return false;
        }
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;

}

// Add frame tracked data into protobuf message.
void CVTracker::AddFrameDataToProto(pb_tracker::Frame* pbFrameData, FrameData& fData) {

    // Save frame number and rotation
    pbFrameData->set_id(fData.frame_id);
    pbFrameData->set_rotation(0);

    pb_tracker::Frame::Box* box = pbFrameData->mutable_bounding_box();
    // Save bounding box data
    box->set_x1(fData.x1);
    box->set_y1(fData.y1);
    box->set_x2(fData.x2);
    box->set_y2(fData.y2);
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
    if (!root["tracker-type"].isNull()){
		trackerType = (root["tracker-type"].asString());
	}

    if (!root["region"].isNull()){
        double x = root["region"]["x"].asDouble();
        double y = root["region"]["y"].asDouble();
        double w = root["region"]["width"].asDouble();
        double h = root["region"]["height"].asDouble();
        cv::Rect2d prev_bbox(x,y,w,h);
        bbox = prev_bbox;
	}
    else{
        processingController->SetError(true, "No initial bounding box selected");
        error = true;
    }

    if (!root["region"]["first-frame"].isNull()){
        start = root["region"]["first-frame"].asInt64();
        json_interval = true;
    }
    else{
        processingController->SetError(true, "No first-frame");
        error = true;
    }
}

/*
||||||||||||||||||||||||||||||||||||||||||||||||||
                ONLY FOR MAKE TEST
||||||||||||||||||||||||||||||||||||||||||||||||||
*/

// Load protobuf data file
bool CVTracker::_LoadTrackedData(){
    using std::ios;

    // Create tracker message
    pb_tracker::Tracker trackerMessage;

    {
        // Read the existing tracker message.
        std::fstream input(protobuf_data_path, ios::in | ios::binary);
        if (!trackerMessage.ParseFromIstream(&input)) {
            std::cerr << "Failed to parse protobuf message." << std::endl;
            return false;
        }
    }

    // Make sure the trackedData is empty
    trackedDataById.clear();

    // Iterate over all frames of the saved message
    for (size_t i = 0; i < trackerMessage.frame_size(); i++) {
        const pb_tracker::Frame& pbFrameData = trackerMessage.frame(i);

        // Load frame and rotation data
        size_t id = pbFrameData.id();
        float rotation = pbFrameData.rotation();

        // Load bounding box data
        const pb_tracker::Frame::Box& box = pbFrameData.bounding_box();
        float x1 = box.x1();
        float y1 = box.y1();
        float x2 = box.x2();
        float y2 = box.y2();

        // Assign data to tracker map
        trackedDataById[id] = FrameData(id, rotation, x1, y1, x2, y2);
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;
}
