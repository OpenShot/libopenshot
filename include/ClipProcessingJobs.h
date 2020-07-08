/**
 * @file
 * @brief Header for the ClipProcessingJobs class
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


#ifdef USE_OPENCV
	#define int64 opencv_broken_int
	#define uint64 opencv_broken_uint
	#include <opencv2/opencv.hpp>
	#include <opencv2/core.hpp>
	#undef uint64
	#undef int64	

	#include "CVStabilization.h"
    #include "CVTracker.h"
#endif

#include "Clip.h"
#include "effects/Tracker.h"

using namespace openshot;

class ClipProcessingJobs{

	private:

		void trackVideo(Clip& videoClip);
		void stabilizeVideo(Clip& video);



	public:
		ClipProcessingJobs(std::string processingType, Clip& videoClip);		




};