/**
 * @file
 * @brief Track an object selected by the user
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
#include <cmath>
#include <algorithm>

#include <google/protobuf/util/time_util.h>

#include "OpenCVUtilities.h"
#include "CVTracker.h"
#include "trackerdata.pb.h"
#include "Exceptions.h"

using namespace openshot;
using google::protobuf::util::TimeUtil;

// Clamp a rectangle to image bounds and ensure a minimal size
static inline void clampRect(cv::Rect2d &r, int width, int height)
{
    r.x = std::clamp(r.x, 0.0, double(width  - 1));
    r.y = std::clamp(r.y, 0.0, double(height - 1));
    r.width  = std::clamp(r.width,  1.0, double(width  - r.x));
    r.height = std::clamp(r.height, 1.0, double(height - r.y));
}

// Constructor
CVTracker::CVTracker(std::string processInfoJson, ProcessingController &processingController)
: processingController(&processingController), json_interval(false){
    SetJson(processInfoJson);
    start = 1;
    end = 1;
    lostCount = 0;
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

// Track object in the whole clip or in a given interval
void CVTracker::trackClip(openshot::Clip& video,
                          size_t _start,
                          size_t _end,
                          bool process_interval)
{
    video.Open();
    if (!json_interval) {
        start = _start; end = _end;
        if (!process_interval || end <= 1 || end - start == 0) {
            start = int(video.Start() * video.Reader()->info.fps.ToFloat()) + 1;
            end   = int(video.End()   * video.Reader()->info.fps.ToFloat()) + 1;
        }
    } else {
        start = int(start + video.Start() * video.Reader()->info.fps.ToFloat()) + 1;
        end   = int(video.End()   * video.Reader()->info.fps.ToFloat()) + 1;
    }
    if (error) return;
    processingController->SetError(false, "");

    bool trackerInit = false;
    lostCount = 0;  // reset lost counter once at the start

    for (size_t frame = start; frame <= end; ++frame) {
        if (processingController->ShouldStop()) return;

        auto f      = video.GetFrame(frame);
        cv::Mat img = f->GetImageCV();

        if (frame == start) {
            bbox = cv::Rect2d(
                int(bbox.x      * img.cols),
                int(bbox.y      * img.rows),
                int(bbox.width  * img.cols),
                int(bbox.height * img.rows)
            );
        }

        if (!trackerInit) {
            initTracker(img, frame);
            trackerInit = true;
            lostCount    = 0;
        }
        else {
            // trackFrame now manages lostCount and will re-init internally
            trackFrame(img, frame);

            // record whatever bbox we have now
            FrameData fd = GetTrackedData(frame);
        }

        processingController->SetProgress(
            uint(100 * (frame - start) / (end - start))
        );
    }
}

// Initialize the tracker
bool CVTracker::initTracker(cv::Mat &frame, size_t frameId)
{
    // Create new tracker object
    tracker = selectTracker(trackerType);

    // Correct negative width/height
    if (bbox.width < 0) {
        bbox.x    -= bbox.width;
        bbox.width = -bbox.width;
    }
    if (bbox.height < 0) {
        bbox.y     -= bbox.height;
        bbox.height = -bbox.height;
    }

    // Clamp to frame bounds
    clampRect(bbox, frame.cols, frame.rows);

    // Initialize tracker
    tracker->init(frame, bbox);

    float fw = float(frame.cols), fh = float(frame.rows);

    // record original pixel size
    origWidth  = bbox.width;
    origHeight = bbox.height;

    // initialize sub-pixel smoother at true center
    smoothC_x = bbox.x + bbox.width  * 0.5;
    smoothC_y = bbox.y + bbox.height * 0.5;

    // Add new frame data
    trackedDataById[frameId] = FrameData(
        frameId, 0,
        bbox.x           / fw,
        bbox.y           / fh,
        (bbox.x + bbox.width)  / fw,
        (bbox.y + bbox.height) / fh
    );

    return true;
}

// Update the object tracker according to frame
// returns true if KLT succeeded, false otherwise
bool CVTracker::trackFrame(cv::Mat &frame, size_t frameId)
{
    const int W = frame.cols, H = frame.rows;
    const auto& prev = trackedDataById[frameId - 1];

    // Reconstruct last-known box in pixel coords
    cv::Rect2d lastBox(
        prev.x1 * W, prev.y1 * H,
        (prev.x2 - prev.x1) * W,
        (prev.y2 - prev.y1) * H
    );

    // Convert to grayscale
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    cv::Rect2d cand;
    bool didKLT = false;

    // Try KLT-based drift
    if (!prevGray.empty() && !prevPts.empty()) {
        std::vector<cv::Point2f> currPts;
        std::vector<uchar> status;
        std::vector<float> err;
        cv::calcOpticalFlowPyrLK(
            prevGray, gray,
            prevPts, currPts,
            status, err,
            cv::Size(21,21), 3,
            cv::TermCriteria{cv::TermCriteria::COUNT|cv::TermCriteria::EPS,30,0.01},
            cv::OPTFLOW_LK_GET_MIN_EIGENVALS, 1e-4
        );

        // collect per-point displacements
        std::vector<double> dx, dy;
        for (size_t i = 0; i < status.size(); ++i) {
            if (status[i] && err[i] < 12.0) {
                dx.push_back(currPts[i].x - prevPts[i].x);
                dy.push_back(currPts[i].y - prevPts[i].y);
            }
        }

        if ((int)dx.size() >= minKltPts) {
            auto median = [&](auto &v){
                std::nth_element(v.begin(), v.begin()+v.size()/2, v.end());
                return v[v.size()/2];
            };
            double mdx = median(dx), mdy = median(dy);

            cand = lastBox;
            cand.x += mdx;
            cand.y += mdy;
            cand.width  = origWidth;
            cand.height = origHeight;

            lostCount = 0;
            didKLT    = true;
        }
    }

    // Fallback to whole-frame flow if KLT failed
    if (!didKLT) {
        ++lostCount;
        cand = lastBox;
        if (!fullPrevGray.empty()) {
            cv::Mat flow;
            cv::calcOpticalFlowFarneback(
                fullPrevGray, gray, flow,
                0.5,3,15,3,5,1.2,0
            );
            cv::Scalar avg = cv::mean(flow);
            cand.x += avg[0];
            cand.y += avg[1];
        }
        cand.width  = origWidth;
        cand.height = origHeight;

        if (lostCount >= 10) {
            initTracker(frame, frameId);
            cand      = bbox;
            lostCount = 0;
        }
    }

    // Dead-zone sub-pixel smoothing
    {
        constexpr double JITTER_THRESH = 1.0;
        double measCx = cand.x + cand.width  * 0.5;
        double measCy = cand.y + cand.height * 0.5;
        double dx    = measCx - smoothC_x;
        double dy    = measCy - smoothC_y;

        if (std::abs(dx) > JITTER_THRESH || std::abs(dy) > JITTER_THRESH) {
            smoothC_x = measCx;
            smoothC_y = measCy;
        }

        cand.x = smoothC_x - cand.width  * 0.5;
        cand.y = smoothC_y - cand.height * 0.5;
    }


    // Candidate box may now lie outside frame; ROI for KLT is clamped below
    // Re-seed KLT features
    {
        // Clamp ROI to frame bounds and avoid negative width/height
        int roiX = int(std::clamp(cand.x, 0.0, double(W - 1)));
        int roiY = int(std::clamp(cand.y, 0.0, double(H - 1)));
        int roiW = int(std::min(cand.width,  double(W - roiX)));
        int roiH = int(std::min(cand.height, double(H - roiY)));
        roiW = std::max(0, roiW);
        roiH = std::max(0, roiH);

        if (roiW > 0 && roiH > 0) {
            cv::Rect roi(roiX, roiY, roiW, roiH);
            cv::goodFeaturesToTrack(
                gray(roi), prevPts,
                kltMaxCorners, kltQualityLevel,
                kltMinDist, cv::Mat(), kltBlockSize
            );
            for (auto &pt : prevPts)
                pt += cv::Point2f(float(roi.x), float(roi.y));
        } else {
            prevPts.clear();
        }
    }

    // Commit state
    fullPrevGray = gray.clone();
    prevGray     = gray.clone();
    bbox         = cand;
    float fw = float(W), fh = float(H);
    trackedDataById[frameId] = FrameData(
        frameId, 0,
        cand.x              / fw,
        cand.y              / fh,
        (cand.x + cand.width)  / fw,
        (cand.y + cand.height) / fh
    );

    return didKLT;
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
        double x = root["region"]["normalized_x"].asDouble();
        double y = root["region"]["normalized_y"].asDouble();
        double w = root["region"]["normalized_width"].asDouble();
        double h = root["region"]["normalized_height"].asDouble();
        cv::Rect2d prev_bbox(x,y,w,h);
        bbox = prev_bbox;

        if (!root["region"]["first-frame"].isNull()){
            start = root["region"]["first-frame"].asInt64();
            json_interval = true;
        }
        else{
            processingController->SetError(true, "No first-frame");
            error = true;
        }

	}
    else{
        processingController->SetError(true, "No initial bounding box selected");
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
