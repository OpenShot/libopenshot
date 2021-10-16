// © OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "KalmanTracker.h"
#include "Hungarian.h"

#include <iostream>
#include <fstream>
#include <iomanip> // to format image names using setw() and setfill()
#include <set>

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
	SortTracker(int max_age = 7, int min_hits = 2);
	// Initialize tracker

	// Update position based on the new frame
	void update(std::vector<cv::Rect> detection, int frame_count, double image_diagonal, std::vector<float> confidences, std::vector<int> classIds);
	double GetIOU(cv::Rect_<float> bb_test, cv::Rect_<float> bb_gt);
	double GetCentroidsDistance(cv::Rect_<float> bb_test, cv::Rect_<float> bb_gt);
	std::vector<KalmanTracker> trackers;

	double max_centroid_dist_norm = 0.05;

	std::vector<cv::Rect_<float>> predictedBoxes;
	std::vector<std::vector<double>> centroid_dist_matrix;
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
	bool alive_tracker;
};
