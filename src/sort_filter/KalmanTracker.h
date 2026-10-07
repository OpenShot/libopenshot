// © OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

///////////////////////////////////////////////////////////////////////////////
// KalmanTracker.h: KalmanTracker Class Declaration

#ifndef KALMAN_H
#define KALMAN_H 2

#include "opencv2/video/tracking.hpp"
#include "opencv2/highgui/highgui.hpp"

#include <map>
#include <vector>


#define StateType cv::Rect_<float>

struct ClassScore
{
	int classId = 0;
	float score = 0.0f;
	ClassScore() {}
	ClassScore(int _classId, float _score) : classId(_classId), score(_score) {}
};

/// This class represents the internel state of individual tracked objects observed as bounding box.
class KalmanTracker
{
public:
	KalmanTracker()
	{
		init_kf(StateType());
		m_time_since_update = 0;
		m_hits = 0;
		m_hit_streak = 0;
		m_age = 0;
		m_id = 0;
	}
	KalmanTracker(StateType initRect, float confidence, int classId, int objectId, const std::vector<ClassScore>& classScores = {}) : confidence(confidence), classId(classId)
	{
		init_kf(initRect);
		m_time_since_update = 0;
		m_hits = 0;
		m_hit_streak = 0;
		m_age = 0;
		m_id = objectId;
		update_class_scores(classScores, classId, confidence);
	}

	~KalmanTracker()
	{
		m_history.clear();
	}

	StateType predict();
	StateType predict2();
	void update(StateType stateMat);
	void update_class_scores(const std::vector<ClassScore>& classScores, int fallbackClassId, float fallbackConfidence);

	StateType get_state();
	StateType get_rect_xysr(float cx, float cy, float s, float r);

	int m_time_since_update;
	int m_hits;
	int m_hit_streak;
	int m_age;
	int m_id;
	float confidence;
	int classId;
	std::map<int, double> classScoreHistory;

private:
	void init_kf(StateType stateMat);

	cv::KalmanFilter kf;
	cv::Mat measurement;

	std::vector<StateType> m_history;
};

#endif
