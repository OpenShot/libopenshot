// © OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "sort.hpp"

using namespace std;

// Constructor
SortTracker::SortTracker(int max_age, int min_hits)
{
	_min_hits = min_hits;
	_max_age = max_age;
	alive_tracker = true;
}

// Computes IOU between two bounding boxes
double SortTracker::GetIOU(cv::Rect_<float> bb_test, cv::Rect_<float> bb_gt)
{
	float in = (bb_test & bb_gt).area();
	float un = bb_test.area() + bb_gt.area() - in;

	if (un < DBL_EPSILON)
		return 0;

	return (double)(in / un);
}

// Computes centroid distance between two bounding boxes
double SortTracker::GetCentroidsDistance(
	cv::Rect_<float> bb_test,
	cv::Rect_<float> bb_gt)
{
	float bb_test_centroid_x = (bb_test.x + bb_test.width / 2);
	float bb_test_centroid_y = (bb_test.y + bb_test.height / 2);

	float bb_gt_centroid_x = (bb_gt.x + bb_gt.width / 2);
	float bb_gt_centroid_y = (bb_gt.y + bb_gt.height / 2);

	double distance = (double)sqrt(pow(bb_gt_centroid_x - bb_test_centroid_x, 2) + pow(bb_gt_centroid_y - bb_test_centroid_y, 2));

	return distance;
}

void SortTracker::update(vector<cv::Rect> detections_cv, int frame_count, double image_diagonal, std::vector<float> confidences, std::vector<int> classIds)
{
	vector<TrackingBox> detections;
	if (trackers.size() == 0) // the first frame met
	{
		alive_tracker = false;
		// initialize kalman trackers using first detections.
		for (unsigned int i = 0; i < detections_cv.size(); i++)
		{
			TrackingBox tb;

			tb.box = cv::Rect_<float>(detections_cv[i]);
			tb.classId = classIds[i];
			tb.confidence = confidences[i];
			detections.push_back(tb);

			KalmanTracker trk = KalmanTracker(detections[i].box, detections[i].confidence, detections[i].classId, i);
			trackers.push_back(trk);
		}
		return;
	}
	else
	{
		for (unsigned int i = 0; i < detections_cv.size(); i++)
		{
			TrackingBox tb;
			tb.box = cv::Rect_<float>(detections_cv[i]);
			tb.classId = classIds[i];
			tb.confidence = confidences[i];
			detections.push_back(tb);
		}
		for (auto it = frameTrackingResult.begin(); it != frameTrackingResult.end(); it++)
		{
			int frame_age = frame_count - it->frame;
			if (frame_age >= _max_age || frame_age < 0)
			{
				dead_trackers_id.push_back(it->id);
			}
		}
	}

	///////////////////////////////////////
	// 3.1. get predicted locations from existing trackers.
	predictedBoxes.clear();
	for (unsigned int i = 0; i < trackers.size();)
	{
		cv::Rect_<float> pBox = trackers[i].predict();
		if (pBox.x >= 0 && pBox.y >= 0)
		{
			predictedBoxes.push_back(pBox);
			i++;
			continue;
		}
		trackers.erase(trackers.begin() + i);
	}

	trkNum = predictedBoxes.size();
	detNum = detections.size();

	centroid_dist_matrix.clear();
	centroid_dist_matrix.resize(trkNum, vector<double>(detNum, 0));

	for (unsigned int i = 0; i < trkNum; i++) // compute iou matrix as a distance matrix
	{
		for (unsigned int j = 0; j < detNum; j++)
		{
			// use 1-iou because the hungarian algorithm computes a minimum-cost assignment.
			double distance = SortTracker::GetCentroidsDistance(predictedBoxes[i], detections[j].box) / image_diagonal;
			centroid_dist_matrix[i][j] = distance;
		}
	}

	HungarianAlgorithm HungAlgo;
	assignment.clear();
	HungAlgo.Solve(centroid_dist_matrix, assignment);
	// find matches, unmatched_detections and unmatched_predictions
	unmatchedTrajectories.clear();
	unmatchedDetections.clear();
	allItems.clear();
	matchedItems.clear();

	if (detNum > trkNum) //	there are unmatched detections
	{
		for (unsigned int n = 0; n < detNum; n++)
			allItems.insert(n);

		for (unsigned int i = 0; i < trkNum; ++i)
			matchedItems.insert(assignment[i]);

		set_difference(allItems.begin(), allItems.end(),
					   matchedItems.begin(), matchedItems.end(),
					   insert_iterator<set<int>>(unmatchedDetections, unmatchedDetections.begin()));
	}
	else if (detNum < trkNum) // there are unmatched trajectory/predictions
	{
		for (unsigned int i = 0; i < trkNum; ++i)
			if (assignment[i] == -1) // unassigned label will be set as -1 in the assignment algorithm
				unmatchedTrajectories.insert(i);
	}
	else
		;

	// filter out matched with low IOU
	matchedPairs.clear();
	for (unsigned int i = 0; i < trkNum; ++i)
	{
		if (assignment[i] == -1) // pass over invalid values
			continue;
		if (centroid_dist_matrix[i][assignment[i]] > max_centroid_dist_norm)
		{
			unmatchedTrajectories.insert(i);
			unmatchedDetections.insert(assignment[i]);
		}
		else
			matchedPairs.push_back(cv::Point(i, assignment[i]));
	}

	for (unsigned int i = 0; i < matchedPairs.size(); i++)
	{
		int trkIdx = matchedPairs[i].x;
		int detIdx = matchedPairs[i].y;
		trackers[trkIdx].update(detections[detIdx].box);
		trackers[trkIdx].classId = detections[detIdx].classId;
		trackers[trkIdx].confidence = detections[detIdx].confidence;
	}

	// create and initialise new trackers for unmatched detections
	for (auto umd : unmatchedDetections)
	{
		KalmanTracker tracker = KalmanTracker(detections[umd].box, detections[umd].confidence, detections[umd].classId, umd);
		trackers.push_back(tracker);
	}

	for (auto it2 = dead_trackers_id.begin(); it2 != dead_trackers_id.end(); it2++)
	{
		for (unsigned int i = 0; i < trackers.size();)
		{
			if (trackers[i].m_id == (*it2))
			{
				trackers.erase(trackers.begin() + i);
				continue;
			}
			i++;
		}
	}

	// get trackers' output
	frameTrackingResult.clear();
	for (unsigned int i = 0; i < trackers.size();)
	{
		if ((trackers[i].m_time_since_update < 1 && trackers[i].m_hit_streak >= _min_hits) || frame_count <= _min_hits)
		{
			alive_tracker = true;
			TrackingBox res;
			res.box = trackers[i].get_state();
			res.id = trackers[i].m_id;
			res.frame = frame_count;
			res.classId = trackers[i].classId;
			res.confidence = trackers[i].confidence;
			frameTrackingResult.push_back(res);
		}

		// remove dead tracklet
		if (trackers[i].m_time_since_update >= _max_age)
		{
			trackers.erase(trackers.begin() + i);
			continue;
		}
		i++;
	}
}
