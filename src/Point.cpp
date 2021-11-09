/**
 * @file
 * @brief Source file for Point class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Point.h"
#include "Exceptions.h"

using namespace std;
using namespace openshot;

// Default constructor
Point::Point() : Point::Point(Coordinate(1, 0), BEZIER, AUTO) {}

// Constructor which creates a single coordinate at X=1
Point::Point(float y) : Point::Point(Coordinate(1, y), CONSTANT, AUTO) {}

// Constructor which creates a Bezier curve with point at (x, y)
Point::Point(float x, float y) : Point::Point(Coordinate(x, y), BEZIER, AUTO) {}

// Constructor which also creates a Point, setting X,Y, and interpolation.
Point::Point(float x, float y, InterpolationType interpolation)
	: Point::Point(Coordinate(x, y), interpolation, AUTO) {}


// Direct Coordinate-accepting constructors
Point::Point(const Coordinate& co) : Point::Point(co, BEZIER, AUTO) {}

Point::Point(const Coordinate& co, InterpolationType interpolation)
	: Point::Point(co, interpolation, AUTO) {}

Point::Point(const Coordinate& co, InterpolationType interpolation, HandleType handle_type) :
	co(co), interpolation(interpolation), handle_type(handle_type) {
	// set handles
	Initialize_Handles();
}

void Point::Initialize_Handles() {
	// initialize left and right handles (in percentages from 0 to 1)
	// default to a smooth curve
	Initialize_LeftHandle(0.5, 1.0);
	Initialize_RightHandle(0.5, 0.0);
}

void Point::Initialize_LeftHandle(float x, float y) {
	// initialize left handle (in percentages from 0 to 1)
	handle_left = Coordinate(x, y);
}

void Point::Initialize_RightHandle(float x, float y) {
	// initialize right handle (in percentages from 0 to 1)
	handle_right = Coordinate(x, y);
}

// Generate JSON string of this object
std::string Point::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Point::JsonValue() const {

	// Create root json object
	Json::Value root;
	root["co"] = co.JsonValue();
	if (interpolation == BEZIER) {
		root["handle_left"] = handle_left.JsonValue();
		root["handle_right"] = handle_right.JsonValue();
		root["handle_type"] = handle_type;
	}
	root["interpolation"] = interpolation;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Point::SetJson(const std::string value) {

	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::Value into this object
void Point::SetJsonValue(const Json::Value root) {

	if (!root["co"].isNull())
		co.SetJsonValue(root["co"]); // update coordinate
	if (!root["handle_left"].isNull())
		handle_left.SetJsonValue(root["handle_left"]); // update coordinate
	if (!root["handle_right"].isNull())
		handle_right.SetJsonValue(root["handle_right"]); // update coordinate
	if (!root["interpolation"].isNull())
		interpolation = (InterpolationType) root["interpolation"].asInt();
	if (!root["handle_type"].isNull())
		handle_type = (HandleType) root["handle_type"].asInt();

}
