/**
 * @file
 * @brief Header file for OpenCVUtilities (set some common macros)
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2021 OpenShot Studios, LLC
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
