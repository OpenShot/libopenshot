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

#ifndef OPENSHOT_CVTRACKER_H
#define OPENSHOT_CVTRACKER_H

#include "OpenCVUtilities.h"

#define int64 int64_t
#define uint64 uint64_t
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/core.hpp>
#undef uint64
#undef int64

#include "Clip.h"
#include "KeyFrame.h"
#include "Frame.h"
#include "Json.h"

#include "ProcessingController.h"

#include "sort_filter/sort.hpp"

// Forward decl
namespace pb_tracker {
    class Frame;
}

namespace openshot
{

	// Store the tracked object information for one frame
	struct FrameData{
		size_t frame_id = -1;
		float rotation = 0;
		float x1 = -1;
		float y1 = -1;
		float x2 = -1;
		float y2 = -1;

		// Constructors
		FrameData()
		{}

		FrameData( size_t _frame_id)
		{frame_id = _frame_id;}

		FrameData( size_t _frame_id , float _rotation, float _x1, float _y1, float _x2, float _y2)
		{
				frame_id = _frame_id;
				rotation = _rotation;
				x1 = _x1;
				y1 = _y1;
				x2 = _x2;
				y2 = _y2;
		}
	};

	/**
	 * @brief The tracker class will receive one bounding box provided by the user and then iterate over the clip frames
	 * to return the object position in all the frames.
	 */
	class CVTracker {
		private:
			std::map<size_t, FrameData> trackedDataById; // Save tracked data
			std::string trackerType; // Name of the chosen tracker
			cv::Ptr<OPENCV_TRACKER_TYPE> tracker; // Pointer of the selected tracker

			cv::Rect2d bbox; // Bounding box coords
			SortTracker sort;

			std::string protobuf_data_path; // Path to protobuf data file

			uint progress; // Pre-processing effect progress

			/// Will handle a Thread safely comutication between ClipProcessingJobs and the processing effect classes
			ProcessingController *processingController;

			bool json_interval;
			size_t start;
			size_t end;

			bool error = false;

			// Initialize the tracker
			bool initTracker(cv::Mat &frame, size_t frameId);

			// Update the object tracker according to frame
			bool trackFrame(cv::Mat &frame, size_t frameId);

		public:

			// Constructor
			CVTracker(std::string processInfoJson, ProcessingController &processingController);

			// Set desirable tracker method
			cv::Ptr<OPENCV_TRACKER_TYPE> selectTracker(std::string trackerType);

			/// Track object in the hole clip or in a given interval
			///
			/// If start, end and process_interval are passed as argument, clip will be processed in [start,end)
			void trackClip(openshot::Clip& video, size_t _start=0, size_t _end=0, bool process_interval=false);

			/// Filter current bounding box jitter
			cv::Rect2d filter_box_jitter(size_t frameId);

			/// Get tracked data for a given frame
			FrameData GetTrackedData(size_t frameId);

			// Protobuf Save and Load methods
			/// Save protobuf file
			bool SaveTrackedData();
			/// Add frame tracked data into protobuf message.
			void AddFrameDataToProto(pb_tracker::Frame* pbFrameData, FrameData& fData);

			// Get and Set JSON methods
			void SetJson(const std::string value); ///< Load JSON string into this object
			void SetJsonValue(const Json::Value root); ///< Load Json::Value into this object

			// Load protobuf file (ONLY FOR MAKE TEST)
			bool _LoadTrackedData();
	};
}

#endif
