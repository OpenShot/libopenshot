/**
 * @file
 * @brief Source file for CVStabilization class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <fstream>
#include <iomanip>
#include <iostream>

#include "CVStabilization.h"
#include "Exceptions.h"

#include "stabilizedata.pb.h"
#include <google/protobuf/util/time_util.h>

using namespace std;
using namespace openshot;
using google::protobuf::util::TimeUtil;

// Set default smoothing window value to compute stabilization
CVStabilization::CVStabilization(std::string processInfoJson, ProcessingController &processingController)
: processingController(&processingController){
    SetJson(processInfoJson);
    start = 1;
    end = 1;
}

// Process clip and store necessary stabilization data
void CVStabilization::stabilizeClip(openshot::Clip& video, size_t _start, size_t _end, bool process_interval){

    if(error){
        return;
    }
    processingController->SetError(false, "");

    start = _start; end = _end;
    // Compute max and average transformation parameters
    avr_dx=0; avr_dy=0; avr_da=0; max_dx=0; max_dy=0; max_da=0;

    video.Open();
    // Save original video width and height
    cv::Size readerDims(video.Reader()->info.width, video.Reader()->info.height);

    size_t frame_number;
    if(!process_interval || end <= 1 || end-start == 0){
        // Get total number of frames in video
        start = (int)(video.Start() * video.Reader()->info.fps.ToFloat()) + 1;
        end = (int)(video.End() * video.Reader()->info.fps.ToFloat()) + 1;
    }

    // Extract and track opticalflow features for each frame
    for (frame_number = start; frame_number <= end; frame_number++)
    {
        // Stop the feature tracker process
        if(processingController->ShouldStop()){
            return;
        }

        std::shared_ptr<openshot::Frame> f = video.GetFrame(frame_number);

        // Grab OpenCV Mat image
        cv::Mat cvimage = f->GetImageCV();
        // Resize frame to original video width and height if they differ
        if(cvimage.size().width != readerDims.width || cvimage.size().height != readerDims.height)
            cv::resize(cvimage, cvimage, cv::Size(readerDims.width, readerDims.height));
        cv::cvtColor(cvimage, cvimage, cv::COLOR_RGB2GRAY);

        if(!TrackFrameFeatures(cvimage, frame_number)){
            prev_to_cur_transform.push_back(TransformParam(0, 0, 0));
        }

        // Update progress
        processingController->SetProgress(uint(100*(frame_number-start)/(end-start)));
    }

    // Calculate trajectory data
    std::vector <CamTrajectory> trajectory = ComputeFramesTrajectory();

    // Calculate and save smoothed trajectory data
    trajectoryData = SmoothTrajectory(trajectory);

    // Calculate and save transformation data
    transformationData = GenNewCamPosition(trajectoryData);

    // Normalize smoothed trajectory data
    for(auto &dataToNormalize : trajectoryData){
        dataToNormalize.second.x/=readerDims.width;
        dataToNormalize.second.y/=readerDims.height;
    }
    // Normalize transformation data
    for(auto &dataToNormalize : transformationData){
        dataToNormalize.second.dx/=readerDims.width;
        dataToNormalize.second.dy/=readerDims.height;
    }
}

// Track current frame features and find the relative transformation
bool CVStabilization::TrackFrameFeatures(cv::Mat frame, size_t frameNum){
    // Check if there are black frames
    if(cv::countNonZero(frame) < 1){
        return false;
    }

    // Initialize prev_grey if not
    if(prev_grey.empty()){
        prev_grey = frame;
        return true;
    }

    // OpticalFlow features vector
    std::vector <cv::Point2f> prev_corner, cur_corner;
    std::vector <cv::Point2f> prev_corner2, cur_corner2;
    std::vector <uchar> status;
    std::vector <float> err;
    // Extract new image features
    cv::goodFeaturesToTrack(prev_grey, prev_corner, 200, 0.01, 30);
    // Track features
    cv::calcOpticalFlowPyrLK(prev_grey, frame, prev_corner, cur_corner, status, err);
    // Remove untracked features
    for(size_t i=0; i < status.size(); i++) {
        if(status[i]) {
            prev_corner2.push_back(prev_corner[i]);
            cur_corner2.push_back(cur_corner[i]);
        }
    }
    // In case no feature was detected
    if(prev_corner2.empty() || cur_corner2.empty()){
        last_T = cv::Mat();
        // prev_grey = cv::Mat();
        return false;
    }

    // Translation + rotation only
    cv::Mat T = cv::estimateAffinePartial2D(prev_corner2, cur_corner2); // false = rigid transform, no scaling/shearing

    double da, dx, dy;
    // If T has nothing inside return (probably a segment where there is nothing to stabilize)
    if(T.size().width == 0 || T.size().height == 0){
        return false;
    }
    else{
        // If no transformation is found, just use the last known good transform
        if(T.data == NULL){
            if(!last_T.empty())
                last_T.copyTo(T);
            else
                return false;
        }
        // Decompose T
        dx = T.at<double>(0,2);
        dy = T.at<double>(1,2);
        da = atan2(T.at<double>(1,0), T.at<double>(0,0));
    }

    // Filter transformations parameters, if they are higher than these: return
    if(dx > 200 || dy > 200 || da > 0.1){
        return false;
    }

    // Keep computing average and max transformation parameters
    avr_dx+=fabs(dx);
    avr_dy+=fabs(dy);
    avr_da+=fabs(da);
    if(fabs(dx) > max_dx)
        max_dx = dx;
    if(fabs(dy) > max_dy)
        max_dy = dy;
    if(fabs(da) > max_da)
        max_da = da;

    T.copyTo(last_T);

    prev_to_cur_transform.push_back(TransformParam(dx, dy, da));
    frame.copyTo(prev_grey);

    return true;
}

std::vector<CamTrajectory> CVStabilization::ComputeFramesTrajectory(){

    // Accumulated frame to frame transform
    double a = 0;
    double x = 0;
    double y = 0;

    vector <CamTrajectory> trajectory; // trajectory at all frames

    // Compute global camera trajectory. First frame is the origin
    for(size_t i=0; i < prev_to_cur_transform.size(); i++) {
        x += prev_to_cur_transform[i].dx;
        y += prev_to_cur_transform[i].dy;
        a += prev_to_cur_transform[i].da;

        // Save trajectory data to vector
        trajectory.push_back(CamTrajectory(x,y,a));
    }
    return trajectory;
}

std::map<size_t,CamTrajectory> CVStabilization::SmoothTrajectory(std::vector <CamTrajectory> &trajectory){

    std::map <size_t,CamTrajectory> smoothed_trajectory; // trajectory at all frames

    for(size_t i=0; i < trajectory.size(); i++) {
        double sum_x = 0;
        double sum_y = 0;
        double sum_a = 0;
        int count = 0;

        for(int j=-smoothingWindow; j <= smoothingWindow; j++) {
            if(i+j < trajectory.size()) {
                sum_x += trajectory[i+j].x;
                sum_y += trajectory[i+j].y;
                sum_a += trajectory[i+j].a;

                count++;
            }
        }

        double avg_a = sum_a / count;
        double avg_x = sum_x / count;
        double avg_y = sum_y / count;

        // Add smoothed trajectory data to map
        smoothed_trajectory[i + start] = CamTrajectory(avg_x, avg_y, avg_a);
    }
    return smoothed_trajectory;
}

// Generate new transformations parameters for each frame to follow the smoothed trajectory
std::map<size_t,TransformParam> CVStabilization::GenNewCamPosition(std::map <size_t,CamTrajectory> &smoothed_trajectory){
    std::map <size_t,TransformParam> new_prev_to_cur_transform;

    // Accumulated frame to frame transform
    double a = 0;
    double x = 0;
    double y = 0;

    for(size_t i=0; i < prev_to_cur_transform.size(); i++) {
        x += prev_to_cur_transform[i].dx;
        y += prev_to_cur_transform[i].dy;
        a += prev_to_cur_transform[i].da;

        // target - current
        double diff_x = smoothed_trajectory[i + start].x - x;
        double diff_y = smoothed_trajectory[i + start].y - y;
        double diff_a = smoothed_trajectory[i + start].a - a;

        double dx = prev_to_cur_transform[i].dx + diff_x;
        double dy = prev_to_cur_transform[i].dy + diff_y;
        double da = prev_to_cur_transform[i].da + diff_a;

        // Add transformation data to map
        new_prev_to_cur_transform[i + start] = TransformParam(dx, dy, da);
    }
    return new_prev_to_cur_transform;
}

// Save stabilization data to protobuf file
bool CVStabilization::SaveStabilizedData(){
    using std::ios;

    // Create stabilization message
    pb_stabilize::Stabilization stabilizationMessage;

    std::map<size_t,CamTrajectory>::iterator trajData = trajectoryData.begin();
    std::map<size_t,TransformParam>::iterator transData = transformationData.begin();

    // Iterate over all frames data and save in protobuf message
    for(; trajData != trajectoryData.end(); ++trajData, ++transData){
        AddFrameDataToProto(stabilizationMessage.add_frame(), trajData->second, transData->second, trajData->first);
    }
    // Add timestamp
    *stabilizationMessage.mutable_last_updated() = TimeUtil::SecondsToTimestamp(time(NULL));

    // Write the new message to disk.
    std::fstream output(protobuf_data_path, ios::out | ios::trunc | ios::binary);
    if (!stabilizationMessage.SerializeToOstream(&output)) {
        std::cerr << "Failed to write protobuf message." << std::endl;
        return false;
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;
}

// Add frame stabilization data into protobuf message
void CVStabilization::AddFrameDataToProto(pb_stabilize::Frame* pbFrameData, CamTrajectory& trajData, TransformParam& transData, size_t frame_number){

    // Save frame number
    pbFrameData->set_id(frame_number);

    // Save camera trajectory data
    pbFrameData->set_a(trajData.a);
    pbFrameData->set_x(trajData.x);
    pbFrameData->set_y(trajData.y);

    // Save transformation data
    pbFrameData->set_da(transData.da);
    pbFrameData->set_dx(transData.dx);
    pbFrameData->set_dy(transData.dy);
}

TransformParam CVStabilization::GetTransformParamData(size_t frameId){

    // Check if the stabilizer info for the requested frame exists
    if ( transformationData.find(frameId) == transformationData.end() ) {

        return TransformParam();
    } else {

        return transformationData[frameId];
    }
}

CamTrajectory CVStabilization::GetCamTrajectoryTrackedData(size_t frameId){

    // Check if the stabilizer info for the requested frame exists
    if ( trajectoryData.find(frameId) == trajectoryData.end() ) {

        return CamTrajectory();
    } else {

        return trajectoryData[frameId];
    }
}

// Load JSON string into this object
void CVStabilization::SetJson(const std::string value) {
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
void CVStabilization::SetJsonValue(const Json::Value root) {

	// Set data from Json (if key is found)
	if (!root["protobuf_data_path"].isNull()){
		protobuf_data_path = (root["protobuf_data_path"].asString());
	}
    if (!root["smoothing-window"].isNull()){
		smoothingWindow = (root["smoothing-window"].asInt());
	}
}

/*
||||||||||||||||||||||||||||||||||||||||||||||||||
                ONLY FOR MAKE TEST
||||||||||||||||||||||||||||||||||||||||||||||||||
*/

// Load protobuf data file
bool CVStabilization::_LoadStabilizedData(){
    using std::ios;
    // Create stabilization message
    pb_stabilize::Stabilization stabilizationMessage;
    // Read the existing tracker message.
    std::fstream input(protobuf_data_path, ios::in | ios::binary);
    if (!stabilizationMessage.ParseFromIstream(&input)) {
        std::cerr << "Failed to parse protobuf message." << std::endl;
        return false;
    }

    // Make sure the data maps are empty
    transformationData.clear();
    trajectoryData.clear();

    // Iterate over all frames of the saved message and assign to the data maps
    for (size_t i = 0; i < stabilizationMessage.frame_size(); i++) {
        const pb_stabilize::Frame& pbFrameData = stabilizationMessage.frame(i);

        // Load frame number
        size_t id = pbFrameData.id();

        // Load camera trajectory data
        float x = pbFrameData.x();
        float y = pbFrameData.y();
        float a = pbFrameData.a();

        // Assign data to trajectory map
        trajectoryData[id] = CamTrajectory(x,y,a);

        // Load transformation data
        float dx = pbFrameData.dx();
        float dy = pbFrameData.dy();
        float da = pbFrameData.da();

        // Assing data to transformation map
        transformationData[id] = TransformParam(dx,dy,da);
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;
}
