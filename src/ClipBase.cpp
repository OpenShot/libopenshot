/**
 * @file
 * @brief Source file for EffectBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ClipBase.h"
#include "Timeline.h"

using namespace openshot;

// Set position on timeline (in seconds)
void ClipBase::Position(float value) {

	position = value;

	if (ParentTimeline()) {
		// Resort timeline items (internal clips/effects arrays)
		Timeline *parentTimeline = (Timeline *) ParentTimeline();
		parentTimeline->SortTimeline();
	}
}

// Set layer of clip on timeline (lower number is covered by higher numbers)
void ClipBase::Layer(int value) {
	layer = value;

	if (ParentTimeline()) {
		// Resort timeline items (internal clips/effects arrays)
		Timeline *parentTimeline = (Timeline *) ParentTimeline();
		parentTimeline->SortTimeline();
	}
}

// Set start position (in seconds) of clip (trim start of video)
void ClipBase::Start(float value) {
	start = value;

	if (ParentTimeline()) {
		// Resort timeline items (internal clips/effects arrays)
		Timeline *parentTimeline = (Timeline *) ParentTimeline();
		parentTimeline->SortTimeline();
	}
}

// Set end position (in seconds) of clip (trim end of video)
void ClipBase::End(float value) {
	end = value;

	if (ParentTimeline()) {
		// Resort timeline items (internal clips/effects arrays)
		Timeline *parentTimeline = (Timeline *) ParentTimeline();
		parentTimeline->SortTimeline();
	}
}

// Generate Json::Value for this object
Json::Value ClipBase::JsonValue() const {

	// Create root json object
	Json::Value root;
	root["id"] = Id();
	root["position"] = Position();
	root["layer"] = Layer();
	root["start"] = Start();
	root["end"] = End();
	root["duration"] = Duration();

	// return JsonValue
	return root;
}

// Load Json::Value into this object
void ClipBase::SetJsonValue(const Json::Value root) {

	// Set data from Json (if key is found)
	if (!root["id"].isNull())
		Id(root["id"].asString());
	if (!root["position"].isNull())
		Position(root["position"].asDouble());
	if (!root["layer"].isNull())
		Layer(root["layer"].asInt());
	if (!root["start"].isNull())
		Start(root["start"].asDouble());
	if (!root["end"].isNull())
		End(root["end"].asDouble());
}

// Generate JSON for a property
Json::Value ClipBase::add_property_json(std::string name, float value, std::string type, std::string memo, const Keyframe* keyframe, float min_value, float max_value, bool readonly, int64_t requested_frame) const {

	// Requested Point
	const Point requested_point(requested_frame, requested_frame);

	// Create JSON Object
	Json::Value prop = Json::Value(Json::objectValue);
	prop["name"] = name;
	prop["value"] = value;
	prop["memo"] = memo;
	prop["type"] = type;
	prop["min"] = min_value;
	prop["max"] = max_value;
	if (keyframe) {
		prop["keyframe"] = keyframe->Contains(requested_point);
		prop["points"] = int(keyframe->GetCount());
		Point closest_point = keyframe->GetClosestPoint(requested_point);
		prop["interpolation"] = closest_point.interpolation;
		prop["closest_point_x"] = closest_point.co.X;
		prop["previous_point_x"] = keyframe->GetPreviousPoint(closest_point).co.X;
	}
	else {
		prop["keyframe"] = false;
		prop["points"] = 0;
		prop["interpolation"] = CONSTANT;
		prop["closest_point_x"] = -1;
		prop["previous_point_x"] = -1;
	}

	prop["readonly"] = readonly;
	prop["choices"] = Json::Value(Json::arrayValue);

	// return JsonValue
	return prop;
}

Json::Value ClipBase::add_property_choice_json(std::string name, int value, int selected_value) const {

	// Create choice
	Json::Value new_choice = Json::Value(Json::objectValue);
	new_choice["name"] = name;
	new_choice["value"] = value;
	new_choice["selected"] = (value == selected_value);

	// return JsonValue
	return new_choice;
}
