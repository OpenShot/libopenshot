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
	#include "CVObjectDetection.h"
#endif

#include <thread>
#include "ProcessingController.h"
#include "Clip.h"

using namespace openshot;

// Constructor responsible to choose processing type and apply to clip
class ClipProcessingJobs{
	private:
		std::string processInfoJson;
		std::string processingType;

		bool processingDone = false;
		bool stopProcessing = false;
		uint processingProgress = 0;

		std::thread t;

		/// Will handle a Thread safely comutication between ClipProcessingJobs and the processing effect classes
		ProcessingController processingController;

		// Apply object tracking to clip 
		void trackClip(Clip& clip, ProcessingController& controller);
		// Apply stabilization to clip
		void stabilizeClip(Clip& clip, ProcessingController& controller);
		// Apply object detection to clip
		void detectObjectsClip(Clip& clip, ProcessingController& controller);


	public:
		// Constructor
		ClipProcessingJobs(std::string processingType, std::string processInfoJson);
		// Process clip accordingly to processingType	
		void processClip(Clip& clip, std::string json);

		// Thread related variables and methods
		int GetProgress();
		bool IsDone();
		void CancelProcessing();
		bool GetError();
		std::string GetErrorMessage();


};