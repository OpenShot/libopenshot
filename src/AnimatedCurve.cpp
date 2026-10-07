/**
 * @file
 * @brief Source file for AnimatedCurve classes
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "AnimatedCurve.h"
#include "Exceptions.h"

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace openshot;

namespace {
constexpr double kCurveDomainMax = 255.0;

static double clamp01(const double value) {
	return std::max(0.0, std::min(1.0, value));
}
}

AnimatedCurveNode::AnimatedCurveNode()
	: AnimatedCurveNode(0, 0.0, 0.0, LINEAR) {}

AnimatedCurveNode::AnimatedCurveNode(int node_id, double node_x, double node_y, InterpolationType node_interpolation)
	: id(node_id),
	  x(clamp01(node_x)),
	  y(clamp01(node_y)),
	  left_handle_x(0.5),
	  left_handle_y(1.0),
	  right_handle_x(0.5),
	  right_handle_y(0.0),
	  interpolation(node_interpolation),
	  handle_type(AUTO) {}

Point AnimatedCurveNode::Evaluate(int64_t frame_number, double x_scale) const {
	Point point(static_cast<float>(clamp01(x.GetValue(frame_number)) * x_scale),
	            static_cast<float>(clamp01(y.GetValue(frame_number))),
	            interpolation);
	point.handle_left = Coordinate(clamp01(left_handle_x.GetValue(frame_number)),
	                              clamp01(left_handle_y.GetValue(frame_number)));
	point.handle_right = Coordinate(clamp01(right_handle_x.GetValue(frame_number)),
	                               clamp01(right_handle_y.GetValue(frame_number)));
	point.handle_type = handle_type;
	return point;
}

std::string AnimatedCurveNode::Json() const {
	return JsonValue().toStyledString();
}

Json::Value AnimatedCurveNode::JsonValue() const {
	Json::Value root(Json::objectValue);
	root["id"] = id;
	root["x"] = x.JsonValue();
	root["y"] = y.JsonValue();
	root["left_handle_x"] = left_handle_x.JsonValue();
	root["left_handle_y"] = left_handle_y.JsonValue();
	root["right_handle_x"] = right_handle_x.JsonValue();
	root["right_handle_y"] = right_handle_y.JsonValue();
	root["interpolation"] = interpolation;
	root["handle_type"] = handle_type;
	return root;
}

void AnimatedCurveNode::SetJson(const std::string value) {
	try {
		SetJsonValue(openshot::stringToJson(value));
	} catch (const std::exception&) {
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

void AnimatedCurveNode::SetJsonValue(const Json::Value& root) {
	if (!root["id"].isNull())
		id = root["id"].asInt();
	if (!root["x"].isNull())
		x.SetJsonValue(root["x"]);
	if (!root["y"].isNull())
		y.SetJsonValue(root["y"]);
	if (!root["left_handle_x"].isNull())
		left_handle_x.SetJsonValue(root["left_handle_x"]);
	if (!root["left_handle_y"].isNull())
		left_handle_y.SetJsonValue(root["left_handle_y"]);
	if (!root["right_handle_x"].isNull())
		right_handle_x.SetJsonValue(root["right_handle_x"]);
	if (!root["right_handle_y"].isNull())
		right_handle_y.SetJsonValue(root["right_handle_y"]);
	if (!root["interpolation"].isNull())
		interpolation = static_cast<InterpolationType>(root["interpolation"].asInt());
	if (!root["handle_type"].isNull())
		handle_type = static_cast<HandleType>(root["handle_type"].asInt());
}

AnimatedCurve::AnimatedCurve()
	: enabled(1.0) {
	nodes.emplace_back(0, 0.0, 0.0, LINEAR);
	nodes.emplace_back(1, 1.0, 1.0, LINEAR);
}

Keyframe AnimatedCurve::BuildCurve(int64_t frame_number, double x_scale) const {
	Keyframe curve;
	std::vector<AnimatedCurveNode> ordered_nodes = nodes;

	std::sort(ordered_nodes.begin(), ordered_nodes.end(),
	          [frame_number](const AnimatedCurveNode& lhs, const AnimatedCurveNode& rhs) {
		          const double lhs_x = lhs.x.GetValue(frame_number);
		          const double rhs_x = rhs.x.GetValue(frame_number);
		          if (lhs_x == rhs_x)
			          return lhs.id < rhs.id;
		          return lhs_x < rhs_x;
	          });

	for (const auto& node : ordered_nodes) {
		curve.AddPoint(node.Evaluate(frame_number, x_scale));
	}
	return curve;
}

float AnimatedCurve::Sample(float input, int64_t frame_number) const {
	if (enabled.GetValue(frame_number) < 0.5)
		return static_cast<float>(clamp01(input));

	const Keyframe curve = BuildCurve(frame_number, kCurveDomainMax);
	return static_cast<float>(clamp01(curve.GetValue(std::lround(clamp01(input) * kCurveDomainMax))));
}

std::string AnimatedCurve::Summary(int64_t frame_number) const {
	std::ostringstream ss;
	if (enabled.GetValue(frame_number) < 0.5)
		ss << "Disabled, ";
	ss << nodes.size() << " nodes";
	return ss.str();
}

std::string AnimatedCurve::Json() const {
	return JsonValue().toStyledString();
}

Json::Value AnimatedCurve::JsonValue() const {
	Json::Value root(Json::objectValue);
	root["enabled"] = enabled.JsonValue();
	root["nodes"] = Json::Value(Json::arrayValue);
	for (const auto& node : nodes)
		root["nodes"].append(node.JsonValue());
	return root;
}

void AnimatedCurve::SetJson(const std::string value) {
	try {
		SetJsonValue(openshot::stringToJson(value));
	} catch (const std::exception&) {
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

void AnimatedCurve::SetJsonValue(const Json::Value& root) {
	if (!root["enabled"].isNull())
		enabled.SetJsonValue(root["enabled"]);

	if (root["nodes"].isArray()) {
		std::vector<AnimatedCurveNode> parsed_nodes;
		for (const auto& item : root["nodes"]) {
			AnimatedCurveNode node;
			node.SetJsonValue(item);
			parsed_nodes.push_back(node);
		}
		if (!parsed_nodes.empty())
			nodes.swap(parsed_nodes);
	}
}
