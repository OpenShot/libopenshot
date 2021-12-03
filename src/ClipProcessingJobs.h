/**
 * @file
 * @brief Header for ClipProcessingJobs class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

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

namespace openshot {

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

}  // namespace openshot
