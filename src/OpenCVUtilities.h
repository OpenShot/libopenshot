/**
 * @file
 * @brief Header file for OpenCVUtilities (set some common macros)
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2021 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_OPENCV_UTILITIES_H
#define OPENSHOT_OPENCV_UTILITIES_H

#define int64 int64_t
#define uint64 uint64_t
#if USE_LEGACY_TRACKER
    #include <opencv2/tracking.hpp>
    #include <opencv2/tracking/tracking_legacy.hpp>
    #define OPENCV_TRACKER_TYPE cv::legacy::Tracker
    #define OPENCV_TRACKER_NS cv::legacy
#else
    #include <opencv2/tracking.hpp>
    #define OPENCV_TRACKER_TYPE cv::Tracker
    #define OPENCV_TRACKER_NS cv
#endif
#undef int64
#undef uint64

#endif  // OPENSHOT_OPENCV_UTILITIES_H
