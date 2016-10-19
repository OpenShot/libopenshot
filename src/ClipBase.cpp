/**
 * @file
 * @brief Source file for EffectBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/ClipBase.h"

using namespace openshot;

// Generate Json::JsonValue for this object
Json::Value ClipBase::JsonValue() {

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

// Load Json::JsonValue into this object
void ClipBase::SetJsonValue(Json::Value root) {

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
Json::Value ClipBase::add_property_json(string name, float value, string type, string memo, Keyframe* keyframe, float min_value, float max_value, bool readonly, long int requested_frame) {

	// Requested Point
	Point requested_point(requested_frame, requested_frame);

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

Json::Value ClipBase::add_property_choice_json(string name, int value, int selected_value) {

	// Create choice
	Json::Value new_choice = Json::Value(Json::objectValue);
	new_choice["name"] = name;
	new_choice["value"] = value;
	new_choice["selected"] = (value == selected_value);

	// return JsonValue
	return new_choice;
}