/**
 * @file
 * @brief Source file for Point class
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

#include "../include/Point.h"

using namespace std;
using namespace openshot;

// Default constructor (defaults to 0,0)
Point::Point() : interpolation(BEZIER), handle_type(AUTO)
{
	// set new coorinate
	co = Coordinate(1, 0);

	// set handles
	Initialize_Handles();
}

// Constructor which creates a single coordinate at X=0
Point::Point(float y) :
	interpolation(CONSTANT), handle_type(AUTO) {
	// set new coorinate
	co = Coordinate(1, y);

	// set handles
	Initialize_Handles();
}

Point::Point(float x, float y) :
	interpolation(BEZIER), handle_type(AUTO) {
	// set new coorinate
	co = Coordinate(x, y);

	// set handles
	Initialize_Handles();
}

// Constructor which also creates a Point and sets the X,Y, and interpolation of the Point.
Point::Point(float x, float y, InterpolationType interpolation) :
		handle_type(AUTO), interpolation(interpolation) {
	// set new coorinate
	co = Coordinate(x, y);

	// set handles
	Initialize_Handles();
}

Point::Point(Coordinate co) :
	co(co), interpolation(BEZIER), handle_type(AUTO) {
	// set handles
	Initialize_Handles();
}

Point::Point(Coordinate co, InterpolationType interpolation) :
	co(co), interpolation(interpolation), handle_type(AUTO) {
	// set handles
	Initialize_Handles();
}

Point::Point(Coordinate co, InterpolationType interpolation, HandleType handle_type) :
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
string Point::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Point::JsonValue() {

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
void Point::SetJson(string value) throw(InvalidJSON) {

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::Reader reader;
	bool success = reader.parse( value, root );
	if (!success)
		// Raise exception
		throw InvalidJSON("JSON could not be parsed (or is invalid)", "");

	try
	{
		// Set all values that match
		SetJsonValue(root);
	}
	catch (exception e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)", "");
	}
}

// Load Json::JsonValue into this object
void Point::SetJsonValue(Json::Value root) {

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
