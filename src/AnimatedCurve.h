/**
 * @file
 * @brief Header file for AnimatedCurve classes
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_ANIMATED_CURVE_H
#define OPENSHOT_ANIMATED_CURVE_H

#include "Json.h"
#include "KeyFrame.h"
#include "Point.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openshot {

class AnimatedCurveNode {
public:
	int id;
	Keyframe x;
	Keyframe y;
	Keyframe left_handle_x;
	Keyframe left_handle_y;
	Keyframe right_handle_x;
	Keyframe right_handle_y;
	InterpolationType interpolation;
	HandleType handle_type;

	AnimatedCurveNode();
	AnimatedCurveNode(int node_id, double node_x, double node_y, InterpolationType node_interpolation=LINEAR);

	Point Evaluate(int64_t frame_number, double x_scale=1.0) const;

	std::string Json() const;
	Json::Value JsonValue() const;
	void SetJson(const std::string value);
	void SetJsonValue(const Json::Value& root);
};

class AnimatedCurve {
private:
	std::vector<AnimatedCurveNode> nodes;

public:
	Keyframe enabled;

	AnimatedCurve();

	const std::vector<AnimatedCurveNode>& Nodes() const { return nodes; }
	std::vector<AnimatedCurveNode>& Nodes() { return nodes; }

	Keyframe BuildCurve(int64_t frame_number, double x_scale=1.0) const;
	float Sample(float input, int64_t frame_number) const;
	std::string Summary(int64_t frame_number) const;

	std::string Json() const;
	Json::Value JsonValue() const;
	void SetJson(const std::string value);
	void SetJsonValue(const Json::Value& root);
};

}

#endif
