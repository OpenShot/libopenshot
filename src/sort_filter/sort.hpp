// © OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "KalmanTracker.h"
#include "Hungarian.h"

#include <iostream>
#include <fstream>
#include <iomanip> // to format image names using setw() and setfill()
#include <set>
#include <algorithm> // for std::sort

#include "opencv2/video/tracking.hpp"
#include "opencv2/highgui/highgui.hpp"

#ifndef _OPENCV_KCFTRACKER_HPP_
#define _OPENCV_KCFTRACKER_HPP_
#endif
#pragma once

typedef struct TrackingBox
{
	int frame = 0;
	float confidence = 0;
	int classId = 0;
	int id = 0;
	cv::Rect_<float> box = cv::Rect_<float>(0.0, 0.0, 0.0, 0.0);
	TrackingBox() {}
	TrackingBox(int _frame, float _confidence, int _classId, int _id) : frame(_frame), confidence(_confidence), classId(_classId), id(_id) {}
} TrackingBox;

class SortTracker
{
public:
	// Constructor
	SortTracker(int max_age = 50, int min_hits = 5, int max_missed = 7, double min_iou = 0.1, double nms_iou_thresh = 0.5, double min_conf = 0.3);
	// Initialize tracker

	// Update position based on the new frame
	void update(std::vector<cv::Rect> detection, int frame_count, double image_diagonal, std::vector<float> confidences, std::vector<int> classIds);
	static double GetIOU(cv::Rect_<float> bb_test, cv::Rect_<float> bb_gt);
	double GetCentroidsDistance(cv::Rect_<float> bb_test, cv::Rect_<float> bb_gt);
	std::vector<KalmanTracker> trackers;

	double max_centroid_dist_norm = 0.3;

	std::vector<cv::Rect_<float>> predictedBoxes;
	std::vector<std::vector<double>> cost_matrix;
	std::vector<int> assignment;
	std::set<int> unmatchedDetections;
	std::set<int> unmatchedTrajectories;
	std::set<int> allItems;
	std::set<int> matchedItems;
	std::vector<cv::Point> matchedPairs;

	std::vector<TrackingBox> frameTrackingResult;
	std::vector<int> dead_trackers_id;

	unsigned int trkNum = 0;
	unsigned int detNum = 0;
	int _min_hits;
	int _max_age;
	int _max_missed;
	double _min_iou;
	double _nms_iou_thresh;
	double _min_conf;
	unsigned int _next_id;
	bool alive_tracker;
};
