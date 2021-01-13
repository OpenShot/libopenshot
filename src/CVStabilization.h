/**
 * @file
 * @brief Header file for CVStabilization class
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

#ifndef OPENSHOT_STABILIZATION_H
#define OPENSHOT_STABILIZATION_H

#include <google/protobuf/util/time_util.h>

#define int64 opencv_broken_int
#define uint64 opencv_broken_uint
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#undef uint64
#undef int64
#include <cmath>
#include "protobuf_messages/stabilizedata.pb.h"
#include "ProcessingController.h"
#include "Clip.h"
#include "Json.h"

using google::protobuf::util::TimeUtil;

// Store the relative transformation parameters between consecutive frames
struct TransformParam
{
    TransformParam() {}
    TransformParam(double _dx, double _dy, double _da) {
        dx = _dx;
        dy = _dy;
        da = _da;
    }

    double dx;
    double dy;
    double da; // angle
};

// Stores the global camera trajectory for one frame
struct CamTrajectory
{
    CamTrajectory() {}
    CamTrajectory(double _x, double _y, double _a) {
        x = _x;
        y = _y;
        a = _a;
    }

    double x;
    double y;
    double a; // angle
};


/**
 * @brief This class stabilizes a video frame using optical flow
 *
 * The relative motion between two consecutive frames is computed to obtain the global camera trajectory.
 * The camera trajectory is then smoothed to reduce jittering.
 */
class CVStabilization {

    private:

    int smoothingWindow; // In frames. The larger the more stable the video, but less reactive to sudden panning

    size_t start;
    size_t end;
    double avr_dx, avr_dy, avr_da, max_dx, max_dy, max_da;

    cv::Mat last_T;
    cv::Mat prev_grey;
    std::vector <TransformParam> prev_to_cur_transform; // Previous to current
    std::string protobuf_data_path;

    uint progress;
    bool error = false;

    /// Will handle a Thread safely comutication between ClipProcessingJobs and the processing effect classes
	ProcessingController *processingController;

    // Track current frame features and find the relative transformation
    bool TrackFrameFeatures(cv::Mat frame, size_t frameNum);

    std::vector<CamTrajectory> ComputeFramesTrajectory();
    std::map<size_t,CamTrajectory> SmoothTrajectory(std::vector <CamTrajectory> &trajectory);

    // Generate new transformations parameters for each frame to follow the smoothed trajectory
    std::map<size_t,TransformParam> GenNewCamPosition(std::map <size_t,CamTrajectory> &smoothed_trajectory);

    public:

    std::map <size_t,CamTrajectory> trajectoryData; // Save camera trajectory data
    std::map <size_t,TransformParam> transformationData; // Save transormation data

    // Set default smoothing window value to compute stabilization
    CVStabilization(std::string processInfoJson, ProcessingController &processingController);

    // Process clip and store necessary stabilization data
    void stabilizeClip(openshot::Clip& video, size_t _start=0, size_t _end=0, bool process_interval=false);

    /// Protobuf Save and Load methods
    // Save stabilization data to protobuf file
    bool SaveStabilizedData();
    // Add frame stabilization data into protobuf message
    void AddFrameDataToProto(libopenshotstabilize::Frame* pbFrameData, CamTrajectory& trajData, TransformParam& transData, size_t frame_number);

    // Return requested struct info for a given frame
    TransformParam GetTransformParamData(size_t frameId);
    CamTrajectory GetCamTrajectoryTrackedData(size_t frameId);

    /// Get and Set JSON methods
    void SetJson(const std::string value); ///< Load JSON string into this object
    void SetJsonValue(const Json::Value root); ///< Load Json::Value into this object

    // Load protobuf data file (ONLY FOR MAKE TEST)
    bool _LoadStabilizedData();
};

#endif
