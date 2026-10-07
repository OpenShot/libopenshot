// © OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "sort.hpp"
#include <cmath>

using namespace std;

namespace {
struct DetectionBox : public TrackingBox
{
	std::vector<ClassScore> classScores;
};

double box_diagonal(const cv::Rect_<float>& box)
{
	return std::sqrt(box.width * box.width + box.height * box.height);
}

double box_area(const cv::Rect_<float>& box)
{
	return std::max(0.0f, box.width) * std::max(0.0f, box.height);
}

double aspect_ratio(const cv::Rect_<float>& box)
{
	if (box.width <= 0.0f || box.height <= 0.0f)
		return 0.0;
	return static_cast<double>(box.width) / static_cast<double>(box.height);
}

bool box_shape_matches(const cv::Rect_<float>& predicted_box, const cv::Rect_<float>& detection_box, double iou)
{
	const double predicted_area = box_area(predicted_box);
	const double detection_area = box_area(detection_box);
	if (predicted_area <= 1.0 || detection_area <= 1.0)
		return false;

	const double area_ratio = detection_area / predicted_area;
	const double predicted_aspect = aspect_ratio(predicted_box);
	const double detection_aspect = aspect_ratio(detection_box);
	const double aspect_ratio_delta = (predicted_aspect > 0.0 && detection_aspect > 0.0)
		? std::max(predicted_aspect / detection_aspect, detection_aspect / predicted_aspect)
		: 999.0;

	if (iou >= 0.70)
		return area_ratio >= 0.20 && area_ratio <= 5.00 && aspect_ratio_delta <= 4.00;
	return area_ratio >= 0.35 && area_ratio <= 2.80 && aspect_ratio_delta <= 2.75;
}

bool detection_matches_track_gate(
	const KalmanTracker& tracker,
	const cv::Rect_<float>& predicted_box,
	const DetectionBox& detection,
	double iou,
	double centroid_distance)
{
	if (!box_shape_matches(predicted_box, detection.box, iou))
		return false;

	const double scale = std::max(box_diagonal(predicted_box), box_diagonal(detection.box));
	const bool missed_previous_frame = tracker.m_time_since_update > 1;

	if (missed_previous_frame) {
		const double reacquire_distance = std::max(12.0, scale * 0.25);
		return iou >= 0.35 || centroid_distance <= reacquire_distance;
	}

	const double local_distance = std::max(12.0, scale * 0.22);
	return iou >= 0.20 || centroid_distance <= local_distance;
}
}

// Constructor
SortTracker::SortTracker(int max_age, int min_hits, int max_missed, double min_iou, double nms_iou_thresh, double min_conf)
{
	_min_hits = min_hits;
	_max_age = max_age;
	_max_missed = max_missed;
	_min_iou = min_iou;
	_nms_iou_thresh = nms_iou_thresh;
	_min_conf = min_conf;
	_next_id = 0;
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

// Function to apply NMS on detections
void apply_nms(vector<DetectionBox>& detections, double nms_iou_thresh) {
    if (detections.empty()) return;

    // Sort detections by confidence descending
    std::sort(detections.begin(), detections.end(), [](const DetectionBox& a, const DetectionBox& b) {
        return a.confidence > b.confidence;
    });

    vector<bool> suppressed(detections.size(), false);

    for (size_t i = 0; i < detections.size(); ++i) {
        if (suppressed[i]) continue;

        for (size_t j = i + 1; j < detections.size(); ++j) {
            if (suppressed[j]) continue;

            double iou = SortTracker::GetIOU(detections[i].box, detections[j].box);
            if ((detections[i].classId == detections[j].classId && iou > nms_iou_thresh) ||
                iou > 0.85) {
                suppressed[j] = true;
            }
        }
    }

    // Remove suppressed detections
    vector<DetectionBox> filtered;
    for (size_t i = 0; i < detections.size(); ++i) {
        if (!suppressed[i]) {
            filtered.push_back(detections[i]);
        }
    }
    detections = filtered;
}

void SortTracker::update(vector<cv::Rect> detections_cv, int frame_count, double image_diagonal, std::vector<float> confidences, std::vector<int> classIds, std::vector<std::vector<ClassScore>> classScores)
{
	vector<DetectionBox> detections;
	dead_trackers_id.clear();
	if (classScores.size() != detections_cv.size())
		classScores.resize(detections_cv.size());
	if (trackers.size() == 0) // the first frame met
	{
		alive_tracker = false;
		// initialize kalman trackers using first detections.
		for (unsigned int i = 0; i < detections_cv.size(); i++)
		{
			if (confidences[i] < _min_conf) continue; // filter low conf

			DetectionBox tb;

			tb.box = cv::Rect_<float>(detections_cv[i]);
			tb.classId = classIds[i];
			tb.confidence = confidences[i];
			tb.classScores = classScores[i];
			detections.push_back(tb);
		}

		apply_nms(detections, _nms_iou_thresh);

		for (const auto& detection : detections)
		{
			KalmanTracker trk = KalmanTracker(detection.box, detection.confidence, detection.classId, _next_id++, detection.classScores);
			trackers.push_back(trk);
		}
		return;
	}
	else
	{
		for (unsigned int i = 0; i < detections_cv.size(); i++)
		{
			if (confidences[i] < _min_conf) continue; // filter low conf

			DetectionBox tb;
			tb.box = cv::Rect_<float>(detections_cv[i]);
			tb.classId = classIds[i];
			tb.confidence = confidences[i];
			tb.classScores = classScores[i];
			detections.push_back(tb);
		}

		// Apply NMS to remove duplicates
		apply_nms(detections, _nms_iou_thresh);
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

	cost_matrix.clear();
	cost_matrix.resize(trkNum, vector<double>(detNum, 0));
	assignment.clear();
	matchedPairs.clear();
	unmatchedTrajectories.clear();
	unmatchedDetections.clear();
	allItems.clear();
	matchedItems.clear();

	if (trkNum == 0)
	{
		for (auto& detection : detections)
		{
			KalmanTracker tracker = KalmanTracker(detection.box, detection.confidence, detection.classId, _next_id++, detection.classScores);
			trackers.push_back(tracker);
		}
	}
	else if (detNum == 0)
	{
		for (unsigned int i = 0; i < trkNum; ++i)
			unmatchedTrajectories.insert(i);
	}
	else
	{

		for (unsigned int i = 0; i < trkNum; i++) // compute cost matrix using 1 - IOU with gating
		{
			for (unsigned int j = 0; j < detNum; j++)
			{
				double iou = GetIOU(predictedBoxes[i], detections[j].box);
				double centroid_distance = GetCentroidsDistance(predictedBoxes[i], detections[j].box);
				if (!detection_matches_track_gate(trackers[i], predictedBoxes[i], detections[j], iou, centroid_distance))
				{
					cost_matrix[i][j] = 1e9; // large cost for gating
				}
				else
				{
					const double scale = std::max(1.0, std::max(box_diagonal(predictedBoxes[i]), box_diagonal(detections[j].box)));
					const double distance_penalty = std::min(1.0, centroid_distance / scale) * 0.35;
					cost_matrix[i][j] = 1 - iou + distance_penalty + (1 - detections[j].confidence) * 0.1;
				}
			}
		}

		HungarianAlgorithm HungAlgo;
		HungAlgo.Solve(cost_matrix, assignment);

		// find matches, unmatched_detections and unmatched_predictions
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
		for (unsigned int i = 0; i < trkNum; ++i)
		{
			if (assignment[i] == -1) // pass over invalid values
				continue;
			if (cost_matrix[i][assignment[i]] >= 1e8)
			{
				unmatchedTrajectories.insert(i);
				unmatchedDetections.insert(assignment[i]);
			}
			else
				matchedPairs.push_back(cv::Point(i, assignment[i]));
		}
	}

	for (unsigned int i = 0; i < matchedPairs.size(); i++)
	{
		int trkIdx = matchedPairs[i].x;
		int detIdx = matchedPairs[i].y;
		trackers[trkIdx].update(detections[detIdx].box);
		trackers[trkIdx].update_class_scores(detections[detIdx].classScores, detections[detIdx].classId, detections[detIdx].confidence);
		trackers[trkIdx].confidence = detections[detIdx].confidence;
	}

	// create and initialise new trackers for unmatched detections
	for (auto umd : unmatchedDetections)
	{
		KalmanTracker tracker = KalmanTracker(detections[umd].box, detections[umd].confidence, detections[umd].classId, _next_id++, detections[umd].classScores);
		trackers.push_back(tracker);
	}

	// get trackers' output
	frameTrackingResult.clear();
	for (unsigned int i = 0; i < trackers.size();)
	{
		if ((trackers[i].m_hits >= _min_hits && trackers[i].m_time_since_update <= _max_missed) ||
		    frame_count <= _min_hits)
		{
			alive_tracker = true;
			TrackingBox res;
			if (i < predictedBoxes.size() && trackers[i].m_time_since_update > 0)
				res.box = predictedBoxes[i];
			else
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
