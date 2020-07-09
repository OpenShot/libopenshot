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
#include "stabilizedata.pb.h"
#include "Clip.h"

using namespace std;
using google::protobuf::util::TimeUtil;

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

class CVStabilization {       
    private:
    cv::Mat last_T;
    cv::Mat cur, cur_grey;
    cv::Mat prev, prev_grey;

    public:
    const int smoothingWindow; // In frames. The larger the more stable the video, but less reactive to sudden panning
    std::vector <TransformParam> prev_to_cur_transform; // previous to current
    std::vector <CamTrajectory> trajectoryData; // Save camera trajectory data
    std::vector <TransformParam> transformationData; // Save transormation data


    CVStabilization();

    CVStabilization(int _smoothingWindow);

    void ProcessClip(openshot::Clip &video);

    // Track current frame features and find the relative transformation
    void TrackFrameFeatures(cv::Mat frame, int frameNum);
    
    std::vector<CamTrajectory> ComputeFramesTrajectory();
    std::vector<CamTrajectory> SmoothTrajectory(std::vector <CamTrajectory> &trajectory);

    // Generate new transformations parameters for each frame to follow the smoothed trajectory
    std::vector<TransformParam> GenNewCamPosition(std::vector <CamTrajectory> &smoothed_trajectory);
    
    // Save protobuf file
    bool SaveStabilizedData(std::string outputFilePath);
    void AddFrameDataToProto(libopenshotstabilize::Frame* pbFrameData, CamTrajectory& trajData, TransformParam& transData, long int frame_number);
    
    // Load protobuf file
    bool LoadStabilizedData(std::string inputFilePath);

};

#endif